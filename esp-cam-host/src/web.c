// Servidor web.
//
// Acesso: senha HTTP (web_senha do secrets.ini) ou o "token" da sessao, que a
// pagina pega em /api/sessao depois de passar pela senha. O token serve onde o
// navegador nao repete a senha sozinho: o WebSocket e o video na porta 81.
//
// O video fica num segundo servidor (porta 81) porque prende a tarefa do
// servidor enquanto transmite.

#include "web.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera.h"
#include "cartao_sd.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "mbedtls/base64.h"
#include "rede.h"
#include "segredos.h"
#include "usb_serial.h"
#include "web_arquivos.h"
#include "web_interno.h"

static const char *TAG = "web";

extern const char wifi_html_inicio[] asm("_binary_wifi_html_start");

static httpd_handle_t s_srv;
static char s_token[33];

// ------------------------------------------------------------------ utilidades
// Copia texto para dentro de uma string JSON (sem as aspas de fora)
static size_t json_esc(char *dst, size_t tam, const char *src)
{
    size_t o = 0;
    for (; *src && o + 7 < tam; src++) {
        unsigned char c = (unsigned char)*src;
        if (c == '"' || c == '\\') {
            dst[o++] = '\\';
            dst[o++] = c;
        } else if (c < 0x20) {
            o += snprintf(dst + o, tam - o, "\\u%04x", c);
        } else {
            dst[o++] = c;
        }
    }
    dst[o] = '\0';
    return o;
}

static bool query_token(httpd_req_t *req)
{
    char q[80], t[40];
    return httpd_req_get_url_query_str(req, q, sizeof q) == ESP_OK &&
           httpd_query_key_value(q, "t", t, sizeof t) == ESP_OK && strcmp(t, s_token) == 0;
}

// ------------------------------------------------------------------ acesso
// Quem esta no AP de configuracao ja passou pela senha do AP: nao pede de novo
// (o navegador do portal cativo dos celulares nao lida bem com a senha HTTP).
// Decide pelo endereco LOCAL da conexao (por qual interface ela entrou), nao pelo
// IP de quem chega: a rede de casa pode usar a mesma faixa.
static bool via_ap(httpd_req_t *req)
{
    if (!rede_ap_ativo()) return false;
    struct sockaddr_storage end;
    socklen_t tam = sizeof end;
    if (getsockname(httpd_req_to_sockfd(req), (struct sockaddr *)&end, &tam) != 0) return false;
    uint32_t ip;
    if (end.ss_family == AF_INET6)
        ip = ((struct sockaddr_in6 *)&end)->sin6_addr.un.u32_addr[3];   // IPv4 mapeado
    else
        ip = ((struct sockaddr_in *)&end)->sin_addr.s_addr;
    const uint8_t ap[4] = {AP_IP_BYTES};
    return ntohl(ip) == ((uint32_t)ap[0] << 24 | ap[1] << 16 | ap[2] << 8 | ap[3]);
}

static bool senha_ok(httpd_req_t *req)
{
    if (WEB_SENHA[0] == '\0' || via_ap(req) || query_token(req)) return true;
    char cab[160];
    if (httpd_req_get_hdr_value_str(req, "Authorization", cab, sizeof cab) == ESP_OK &&
        strncmp(cab, "Basic ", 6) == 0) {
        unsigned char dec[100];
        size_t n = 0;
        if (mbedtls_base64_decode(dec, sizeof dec - 1, &n, (unsigned char *)cab + 6,
                                  strlen(cab + 6)) == 0) {
            dec[n] = '\0';
            char *senha = strchr((char *)dec, ':');
            if (senha && strcmp(senha + 1, WEB_SENHA) == 0) return true;
        }
    }
    return false;
}

static bool autorizado(httpd_req_t *req)
{
    if (senha_ok(req)) return true;
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Impressora\"");
    httpd_resp_sendstr(req, "Senha necessaria");
    return false;
}

bool web_autorizado(httpd_req_t *req) { return autorizado(req); }
bool web_senha_ok(httpd_req_t *req) { return senha_ok(req); }
size_t web_json_esc(char *dst, size_t tam, const char *src) { return json_esc(dst, tam, src); }

bool web_parametro(httpd_req_t *req, const char *chave, char *saida, size_t tam)
{
    char q[300], v[200];
    if (httpd_req_get_url_query_str(req, q, sizeof q) != ESP_OK ||
        httpd_query_key_value(q, chave, v, sizeof v) != ESP_OK)
        return false;
    size_t o = 0;                      // decodifica %XX e '+'
    for (const char *p = v; *p && o < tam - 1; p++) {
        if (*p == '+') saida[o++] = ' ';
        else if (*p == '%' && p[1] && p[2]) {
            char h[3] = {p[1], p[2], 0};
            saida[o++] = (char)strtol(h, NULL, 16);
            p += 2;
        } else saida[o++] = *p;
    }
    saida[o] = '\0';
    return o > 0;
}

static int le_corpo(httpd_req_t *req, char *buf, size_t tam)
{
    int n = req->content_len < tam - 1 ? req->content_len : tam - 1;
    int lido = 0;
    while (lido < n) {
        int r = httpd_req_recv(req, buf + lido, n - lido);
        if (r <= 0) return -1;
        lido += r;
    }
    buf[lido] = '\0';
    return lido;
}

// ------------------------------------------------------------------ estado
static size_t estado_json(char *j, size_t tam)
{
    marlin_estado_t e;
    marlin_estado(&e);
    char fw[80], ssid[70];
    json_esc(fw, sizeof fw, e.firmware);
    json_esc(ssid, sizeof ssid, rede_ssid());
    return snprintf(j, tam,
        "{\"t\":\"e\",\"tb\":{\"usb\":%s,\"resp\":%s,\"fw\":\"%s\",\"ocupada\":%s,\"fila\":%d,"
        "\"bico\":%.1f,\"bicoAlvo\":%.0f,\"bicoPwm\":%d,\"mesa\":%.1f,\"mesaAlvo\":%.0f,"
        "\"mesaPwm\":%d,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"e\":%.2f,\"vel\":%d,\"fluxo\":%d,"
        "\"comp\":%d,\"min\":[%.1f,%.1f,%.1f],\"max\":[%.1f,%.1f,%.1f]},"
        "\"cam\":{\"sensor\":\"%s\",\"ligada\":%s,\"larg\":%d,\"alt\":%d},"
        "\"sd\":{\"ok\":%s,\"livreMB\":%llu,\"totalMB\":%llu},"
        "\"rede\":{\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"ap\":%s},"
        "\"sis\":{\"seg\":%lld,\"ram\":%u,\"psram\":%u},\"imp\":",
        e.conectada ? "true" : "false", e.respondendo ? "true" : "false", fw,
        e.ocupada ? "true" : "false", e.fila, e.bico, e.bico_alvo, e.bico_pwm, e.mesa,
        e.mesa_alvo, e.mesa_pwm, e.x, e.y, e.z, e.e, e.velocidade, e.fluxo, e.compensacao,
        e.area_min[0], e.area_min[1], e.area_min[2], e.area_max[0], e.area_max[1],
        e.area_max[2], camera_nome(), camera_ligada() ? "true" : "false", camera_largura_max(),
        camera_altura_max(), sd_ok() ? "true" : "false", sd_ok() ? sd_livre_bytes() >> 20 : 0ULL,
        sd_ok() ? sd_total_bytes() >> 20 : 0ULL, ssid, rede_ip(), rede_rssi(),
        rede_ap_ativo() ? "true" : "false", esp_timer_get_time() / 1000000,
        (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
        (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
}

static size_t estado_completo(char *j, size_t tam)
{
    size_t n = estado_json(j, tam);
    if (n + 600 >= tam) return n;
    n += web_impressao_json(j + n, tam - n);
    n += snprintf(j + n, tam - n, ",\"tl\":");
    n += web_timelapse_json(j + n, tam - n);
    n += snprintf(j + n, tam - n, "}");
    return n;
}

// ------------------------------------------------------------------ WebSocket
#define WS_MAX 8
#define HIST_MAX 200            // linhas do console guardadas para quem chega depois

static int s_ws[WS_MAX];
static SemaphoreHandle_t s_ws_mtx;
static char *s_hist[HIST_MAX];
static int s_hist_ini, s_hist_n;

typedef struct {
    int fd;                // -1 = todos
    size_t n;
    char txt[];
} envio_t;

static void ws_remove(int fd)
{
    for (int i = 0; i < WS_MAX; i++)
        if (s_ws[i] == fd) s_ws[i] = -1;
}

static void trabalho_envio(void *arg)
{
    envio_t *m = arg;
    httpd_ws_frame_t f = {.type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t *)m->txt, .len = m->n};
    xSemaphoreTake(s_ws_mtx, portMAX_DELAY);
    for (int i = 0; i < WS_MAX; i++) {
        int fd = s_ws[i];
        if (fd < 0 || (m->fd >= 0 && fd != m->fd)) continue;
        if (httpd_ws_get_fd_info(s_srv, fd) != HTTPD_WS_CLIENT_WEBSOCKET ||
            httpd_ws_send_frame_async(s_srv, fd, &f) != ESP_OK)
            s_ws[i] = -1;
    }
    xSemaphoreGive(s_ws_mtx);
    free(m);
}

// Envia para um navegador (fd) ou para todos (-1). Pode ser chamada de qualquer tarefa.
static void ws_envia(int fd, const char *txt, size_t n)
{
    if (!s_srv) return;
    envio_t *m = malloc(sizeof *m + n + 1);
    if (!m) return;
    m->fd = fd;
    m->n = n;
    memcpy(m->txt, txt, n + 1);
    if (httpd_queue_work(s_srv, trabalho_envio, m) != ESP_OK) free(m);
}

static bool ws_alguem(void)
{
    for (int i = 0; i < WS_MAX; i++)
        if (s_ws[i] >= 0) return true;
    return false;
}

void web_linha_marlin(marlin_tipo_t tipo, const char *linha)
{
    if (tipo == MARLIN_TEMP || tipo == MARLIN_POS) return;   // viram estado, nao console
    static const char *const K[] = {"tx", "rx", "ok", "err", "sis"};
    char j[MARLIN_LINHA_MAX * 3 + 64], esc[MARLIN_LINHA_MAX * 3];
    json_esc(esc, sizeof esc, linha);
    int64_t ms = esp_timer_get_time() / 1000;
    int n = snprintf(j, sizeof j, "{\"t\":\"l\",\"k\":\"%s\",\"ms\":%lld,\"s\":\"%s\"}", K[tipo],
                     ms, esc);
    if (n <= 0 || (size_t)n >= sizeof j) return;

    xSemaphoreTake(s_ws_mtx, portMAX_DELAY);
    int pos = (s_hist_ini + s_hist_n) % HIST_MAX;
    if (s_hist_n == HIST_MAX) {
        free(s_hist[s_hist_ini]);
        s_hist_ini = (s_hist_ini + 1) % HIST_MAX;
        s_hist_n--;
    }
    s_hist[pos] = strdup(j);
    s_hist_n++;
    xSemaphoreGive(s_ws_mtx);
    ws_envia(-1, j, n);
}

static void ws_historico(int fd)
{
    xSemaphoreTake(s_ws_mtx, portMAX_DELAY);
    for (int i = 0; i < s_hist_n; i++) {
        const char *l = s_hist[(s_hist_ini + i) % HIST_MAX];
        if (l) ws_envia(fd, l, strlen(l));
    }
    xSemaphoreGive(s_ws_mtx);
}

static esp_err_t ws_manipula(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {          // abertura da conexao
        if (!query_token(req) && !via_ap(req)) {
            ESP_LOGW(TAG, "WebSocket sem token");
            return ESP_FAIL;
        }
        int fd = httpd_req_to_sockfd(req);
        xSemaphoreTake(s_ws_mtx, portMAX_DELAY);
        ws_remove(fd);
        for (int i = 0; i < WS_MAX; i++)
            if (s_ws[i] < 0) {
                s_ws[i] = fd;
                break;
            }
        xSemaphoreGive(s_ws_mtx);
        char j[2200];
        size_t n = estado_completo(j, sizeof j);
        ws_envia(fd, j, n);
        ws_historico(fd);
        return ESP_OK;
    }

    // Mensagem do navegador: G-code (uma ou mais linhas) ou "!comando"
    httpd_ws_frame_t f = {.type = HTTPD_WS_TYPE_TEXT};
    if (httpd_ws_recv_frame(req, &f, 0) != ESP_OK || f.len == 0 || f.len > 2048) return ESP_OK;
    char *buf = malloc(f.len + 1);
    if (!buf) return ESP_ERR_NO_MEM;
    f.payload = (uint8_t *)buf;
    if (httpd_ws_recv_frame(req, &f, f.len) == ESP_OK) {
        buf[f.len] = '\0';
        if (!strcmp(buf, "!m112")) {
            marlin_emergencia();
        } else if (!strcmp(buf, "!limpa")) {
            marlin_limpa_fila();
        } else {
            char *salvo;
            bool cheia = false;
            for (char *l = strtok_r(buf, "\n", &salvo); l; l = strtok_r(NULL, "\n", &salvo))
                if (marlin_envia(l, true) == ESP_ERR_NO_MEM) cheia = true;
            if (cheia) web_linha_marlin(MARLIN_ERRO, "Fila cheia: comando descartado");
        }
    }
    free(buf);
    return ESP_OK;
}

static void tarefa_estado(void *arg)
{
    static char j[2200];
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (!ws_alguem()) continue;
        size_t n = estado_completo(j, sizeof j);
        ws_envia(-1, j, n);
    }
}

// ------------------------------------------------------------------ API
static esp_err_t api_sessao(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    char j[64];
    snprintf(j, sizeof j, "{\"token\":\"%s\"}", s_token);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, j);
}

static esp_err_t api_estado(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    char j[2200];
    estado_completo(j, sizeof j);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, j);
}

// POST /api/gcode: corpo = uma ou mais linhas de G-code
static esp_err_t api_gcode(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    char corpo[2048];
    if (le_corpo(req, corpo, sizeof corpo) < 0) return ESP_FAIL;
    int n = 0;
    char *salvo;
    for (char *l = strtok_r(corpo, "\n", &salvo); l; l = strtok_r(NULL, "\n", &salvo))
        if (marlin_envia(l, true) == ESP_OK) n++;
    char j[48];
    snprintf(j, sizeof j, "{\"enfileirados\":%d}", n);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, j);
}

// ------------------------------------------------------------------ nivelamento
// O historico e a configuracao da tela de nivelamento ficam num JSON no SD. A
// pagina le, altera e grava o arquivo inteiro (no formato do nivelamento.json do
// console Python, para poder importar de la).
#define NIVEL_ARQ SD_RAIZ "/nivelamento.json"
#define NIVEL_MAX (512 * 1024)

static esp_err_t api_nivel_le(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (!sd_ok()) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "{\"erro\":\"sem cartao SD\"}");
    }
    FILE *f = fopen(NIVEL_ARQ, "rb");
    if (!f) return httpd_resp_sendstr(req, "{\"cfg\":{},\"historico\":[]}");
    char *buf = malloc(4096);
    size_t n;
    while (buf && (n = fread(buf, 1, 4096, f)) > 0) httpd_resp_send_chunk(req, buf, n);
    free(buf);
    fclose(f);
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t api_nivel_grava(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    if (!sd_ok()) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "sem cartao SD");
    }
    if (req->content_len > NIVEL_MAX) {
        httpd_resp_set_status(req, "413 Payload Too Large");
        return httpd_resp_sendstr(req, "grande demais");
    }
    // grava num temporario e troca no fim: uma queda no meio nao estraga o historico
    FILE *f = fopen(NIVEL_ARQ ".tmp", "wb");
    if (!f) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    char *buf = malloc(4096);
    size_t falta = req->content_len;
    bool ok = buf != NULL;
    while (ok && falta) {
        int r = httpd_req_recv(req, buf, falta < 4096 ? falta : 4096);
        if (r <= 0) ok = false;
        else {
            ok = fwrite(buf, 1, r, f) == (size_t)r;
            falta -= r;
        }
    }
    free(buf);
    fclose(f);
    if (!ok) {
        remove(NIVEL_ARQ ".tmp");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    remove(NIVEL_ARQ);
    rename(NIVEL_ARQ ".tmp", NIVEL_ARQ);
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t api_emergencia(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    marlin_emergencia();
    return httpd_resp_sendstr(req, "ok");
}

// ------------------------------------------------------------------ camera
static esp_err_t pag_foto(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    camera_fb_t *fb = camera_foto();
    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t r = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    camera_devolve(fb);
    return r;
}

#define LIMITE "quadro"
static esp_err_t pag_stream(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    if (camera_usa() != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=" LIMITE);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char cab[96];
    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) break;
        int n = snprintf(cab, sizeof cab, "--" LIMITE "\r\nContent-Type: image/jpeg\r\n"
                         "Content-Length: %u\r\n\r\n", (unsigned)fb->len);
        esp_err_t r = httpd_resp_send_chunk(req, cab, n);
        if (r == ESP_OK) r = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        if (r == ESP_OK) r = httpd_resp_send_chunk(req, "\r\n", 2);
        esp_camera_fb_return(fb);
        if (r != ESP_OK) break;      // navegador fechou
    }
    camera_solta();                  // a camera desliga sozinha depois de um tempo
    return ESP_OK;
}

// ------------------------------------------------------------------ Wi-Fi
static esp_err_t pag_wifi(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, wifi_html_inicio);
}

static esp_err_t api_wifi_estado(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    char json[200], ssid[70];
    json_esc(ssid, sizeof ssid, rede_ssid());
    snprintf(json, sizeof json,
             "{\"ssid\":\"%s\",\"conectado\":%s,\"ip\":\"%s\",\"rssi\":%d,\"ap\":%s}",
             ssid, rede_conectado() ? "true" : "false", rede_ip(), rede_rssi(),
             rede_ap_ativo() ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t api_wifi_redes(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    static char json[2400];
    rede_procura(json, sizeof json);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

// Decodifica um campo de formulario (application/x-www-form-urlencoded)
static void campo(const char *corpo, const char *nome, char *saida, size_t tam)
{
    saida[0] = '\0';
    size_t ln = strlen(nome);
    for (const char *p = corpo; p && *p; p = strchr(p, '&') ? strchr(p, '&') + 1 : NULL) {
        if (strncmp(p, nome, ln) || p[ln] != '=') continue;
        p += ln + 1;
        size_t o = 0;
        while (*p && *p != '&' && o < tam - 1) {
            if (*p == '+') {
                saida[o++] = ' ';
                p++;
            } else if (*p == '%' && p[1] && p[2]) {
                char h[3] = {p[1], p[2], 0};
                saida[o++] = (char)strtol(h, NULL, 16);
                p += 3;
            } else {
                saida[o++] = *p++;
            }
        }
        saida[o] = '\0';
        return;
    }
}

static esp_err_t api_wifi_grava(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    char corpo[400];
    if (le_corpo(req, corpo, sizeof corpo) < 0) return ESP_FAIL;
    char ssid[40], senha[80];
    campo(corpo, "ssid", ssid, sizeof ssid);
    campo(corpo, "senha", senha, sizeof senha);
    if (rede_configura(ssid, senha) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "rede invalida");
    }
    return httpd_resp_sendstr(req, "ok");
}

// ------------------------------------------------------------------ interface
static esp_err_t pag_arquivo(httpd_req_t *req)
{
    char uri[64];
    strlcpy(uri, req->uri, sizeof uri);
    char *q = strchr(uri, '?');
    if (q) *q = '\0';
    if (!strcmp(uri, "/")) strcpy(uri, "/index.html");

    const web_arquivo_t *a = NULL;
    for (size_t i = 0; i < WEB_N_ARQUIVOS; i++)
        if (!strcmp(WEB_ARQUIVOS[i].caminho, uri)) a = &WEB_ARQUIVOS[i];

    if (!a) {
        // Portal cativo: no AP, qualquer endereco desconhecido (os testes de internet
        // dos celulares e do Windows) leva a pagina de configuracao.
        if (rede_ap_ativo() && via_ap(req)) {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "http://" AP_IP "/wifi");
            return httpd_resp_send(req, NULL, 0);
        }
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Nao encontrado");
        return ESP_OK;
    }
    if (!autorizado(req)) return ESP_OK;

    char inm[24];
    if (httpd_req_get_hdr_value_str(req, "If-None-Match", inm, sizeof inm) == ESP_OK &&
        !strcmp(inm, a->etag)) {
        httpd_resp_set_status(req, "304 Not Modified");
        return httpd_resp_send(req, NULL, 0);
    }
    httpd_resp_set_type(req, a->tipo);
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "ETag", a->etag);
    return httpd_resp_send(req, (const char *)a->dados, a->tamanho);
}

// ------------------------------------------------------------------ servidores
esp_err_t web_inicia(void)
{
    for (int i = 0; i < 4; i++) sprintf(s_token + i * 8, "%08lx", (unsigned long)esp_random());
    s_ws_mtx = xSemaphoreCreateMutex();
    for (int i = 0; i < WS_MAX; i++) s_ws[i] = -1;

    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.max_uri_handlers = 40;
    c.max_open_sockets = 7;         // com os 3 internos e o video, cabe nos 16 do lwIP
    c.lru_purge_enable = true;      // celulares abrem muitas conexoes no portal cativo
    c.uri_match_fn = httpd_uri_match_wildcard;
    c.stack_size = 8192;
    esp_err_t err = httpd_start(&s_srv, &c);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "servidor nao subiu (0x%x)", err);
        return err;
    }
    const httpd_uri_t rotas[] = {
        {.uri = "/ws", .method = HTTP_GET, .handler = ws_manipula, .is_websocket = true},
        {.uri = "/api/sessao", .method = HTTP_GET, .handler = api_sessao},
        {.uri = "/api/estado", .method = HTTP_GET, .handler = api_estado},
        {.uri = "/api/gcode", .method = HTTP_POST, .handler = api_gcode},
        {.uri = "/api/emergencia", .method = HTTP_POST, .handler = api_emergencia},
        {.uri = "/api/nivel", .method = HTTP_GET, .handler = api_nivel_le},
        {.uri = "/api/nivel", .method = HTTP_POST, .handler = api_nivel_grava},
        {.uri = "/foto.jpg", .method = HTTP_GET, .handler = pag_foto},
        {.uri = "/wifi", .method = HTTP_GET, .handler = pag_wifi},
        {.uri = "/api/wifi/estado", .method = HTTP_GET, .handler = api_wifi_estado},
        {.uri = "/api/wifi/redes", .method = HTTP_GET, .handler = api_wifi_redes},
        {.uri = "/api/wifi", .method = HTTP_POST, .handler = api_wifi_grava},
    };
    for (size_t i = 0; i < sizeof rotas / sizeof rotas[0]; i++)
        httpd_register_uri_handler(s_srv, &rotas[i]);
    web_rotas_arquivos(s_srv);
    web_rotas_timelapse(s_srv);
    // por ultimo: a interface responde tudo o que nao for API
    const httpd_uri_t interface = {.uri = "/*", .method = HTTP_GET, .handler = pag_arquivo};
    httpd_register_uri_handler(s_srv, &interface);

    httpd_handle_t hv = NULL;
    httpd_config_t cv = HTTPD_DEFAULT_CONFIG();
    cv.server_port = 81;
    cv.ctrl_port = 32769;
    cv.max_open_sockets = 2;        // um ou dois videos ao mesmo tempo bastam
    if (httpd_start(&hv, &cv) == ESP_OK) {
        httpd_uri_t u = {.uri = "/stream", .method = HTTP_GET, .handler = pag_stream};
        httpd_register_uri_handler(hv, &u);
    }
    xTaskCreate(tarefa_estado, "web_estado", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "servidor web no ar (%u arquivos de interface)", (unsigned)WEB_N_ARQUIVOS);
    return ESP_OK;
}
