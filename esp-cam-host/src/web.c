// Servidor web. O video fica num segundo servidor (porta 81): ele prende a tarefa
// do servidor enquanto transmite, e assim as paginas continuam respondendo.

#include "web.h"

#include <stdio.h>
#include <string.h>

#include "camera.h"
#include "cartao_sd.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "lwip/sockets.h"
#include "mbedtls/base64.h"
#include "rede.h"
#include "segredos.h"

static const char *TAG = "web";

extern const char wifi_html_inicio[] asm("_binary_wifi_html_start");

// ------------------------------------------------------------------ acesso
// Quem esta no AP de configuracao ja passou pela senha do AP: nao pede de novo
// (o navegador do portal cativo dos celulares nao lida bem com a senha HTTP).
static bool via_ap(httpd_req_t *req)
{
    struct sockaddr_storage end;
    socklen_t tam = sizeof end;
    if (getpeername(httpd_req_to_sockfd(req), (struct sockaddr *)&end, &tam) != 0) return false;
    uint32_t ip;
    if (end.ss_family == AF_INET6)
        ip = ((struct sockaddr_in6 *)&end)->sin6_addr.un.u32_addr[3];   // IPv4 mapeado
    else
        ip = ((struct sockaddr_in *)&end)->sin_addr.s_addr;
    return (ntohl(ip) & 0xFFFFFF00) == 0xC0A80400;     // 192.168.4.x
}

// HTTP Basic: qualquer usuario, a senha e a de secrets.ini (web_senha).
static bool autorizado(httpd_req_t *req)
{
    if (WEB_SENHA[0] == '\0' || via_ap(req)) return true;
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
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Impressora\"");
    httpd_resp_sendstr(req, "Senha necessaria");
    return false;
}

// ------------------------------------------------------------------ paginas
static esp_err_t pag_inicio(httpd_req_t *req)
{
    if (!autorizado(req)) return ESP_OK;
    char html[2000];
    snprintf(html, sizeof html,
        "<!doctype html><html lang=pt-br><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Impressora</title>"
        "<style>body{background:#16191d;color:#e6e9ec;font-family:Segoe UI,sans-serif;margin:16px}"
        "img{max-width:100%%;border-radius:8px;background:#000}a{color:#4aa3ff}"
        "td{padding:2px 12px 2px 0}</style>"
        "<h2>ESP32-S3-CAM</h2>"
        "<table><tr><td>Sensor</td><td>%s (PID 0x%04x), max %dx%d</td></tr>"
        "<tr><td>PSRAM</td><td>%u KB</td></tr>"
        "<tr><td>Cartao SD</td><td>%s</td></tr>"
        "<tr><td>Wi-Fi</td><td>%s %s</td></tr></table>"
        "<p><a href='/foto.jpg'>Foto</a> &middot; <a id=v href='#'>Video ao vivo</a>"
        " &middot; <a href='/wifi'>Configurar Wi-Fi</a></p>"
        "<img id=i src='/foto.jpg'>"
        "<script>document.getElementById('v').onclick=function(e){e.preventDefault();"
        "document.getElementById('i').src=location.protocol+'//'+location.hostname+':81/stream'};"
        "</script></html>",
        camera_nome(), camera_pid(), camera_largura_max(), camera_altura_max(),
        (unsigned)(esp_psram_get_size() / 1024),
        sd_ok() ? "montado" : "ausente",
        rede_conectado() ? rede_ssid() : "desconectado", rede_conectado() ? rede_ip() : "");
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, html);
}

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
    size_t q = 0;
    for (const char *c = rede_ssid(); *c && q < sizeof ssid - 3; c++) {
        if (*c == '"' || *c == '\\') ssid[q++] = '\\';
        ssid[q++] = *c;
    }
    ssid[q] = '\0';
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
    int n = req->content_len < sizeof corpo - 1 ? req->content_len : sizeof corpo - 1;
    int lido = 0;
    while (lido < n) {
        int r = httpd_req_recv(req, corpo + lido, n - lido);
        if (r <= 0) return ESP_FAIL;
        lido += r;
    }
    corpo[lido] = '\0';
    char ssid[40], senha[80];
    campo(corpo, "ssid", ssid, sizeof ssid);
    campo(corpo, "senha", senha, sizeof senha);
    if (rede_configura(ssid, senha) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "rede invalida");
    }
    return httpd_resp_sendstr(req, "ok");
}

// Portal cativo: no AP, qualquer endereco desconhecido (os testes de internet dos
// celulares e do Windows) leva a pagina de configuracao.
static esp_err_t nao_achado(httpd_req_t *req, httpd_err_code_t erro)
{
    if (rede_ap_ativo() && via_ap(req)) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://" AP_IP "/wifi");
        return httpd_resp_send(req, NULL, 0);
    }
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Nao encontrado");
    return ESP_FAIL;
}

// ------------------------------------------------------------------ servidores
esp_err_t web_inicia(void)
{
    httpd_handle_t h = NULL;
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.max_uri_handlers = 16;
    c.lru_purge_enable = true;      // celulares abrem muitas conexoes no portal cativo
    esp_err_t err = httpd_start(&h, &c);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "servidor nao subiu (0x%x)", err);
        return err;
    }
    const httpd_uri_t rotas[] = {
        {.uri = "/", .method = HTTP_GET, .handler = pag_inicio},
        {.uri = "/foto.jpg", .method = HTTP_GET, .handler = pag_foto},
        {.uri = "/wifi", .method = HTTP_GET, .handler = pag_wifi},
        {.uri = "/api/wifi/estado", .method = HTTP_GET, .handler = api_wifi_estado},
        {.uri = "/api/wifi/redes", .method = HTTP_GET, .handler = api_wifi_redes},
        {.uri = "/api/wifi", .method = HTTP_POST, .handler = api_wifi_grava},
    };
    for (size_t i = 0; i < sizeof rotas / sizeof rotas[0]; i++)
        httpd_register_uri_handler(h, &rotas[i]);
    httpd_register_err_handler(h, HTTPD_404_NOT_FOUND, nao_achado);

    httpd_handle_t hv = NULL;
    httpd_config_t cv = HTTPD_DEFAULT_CONFIG();
    cv.server_port = 81;
    cv.ctrl_port = 32769;
    if (httpd_start(&hv, &cv) == ESP_OK) {
        httpd_uri_t u = {.uri = "/stream", .method = HTTP_GET, .handler = pag_stream};
        httpd_register_uri_handler(hv, &u);
    }
    return ESP_OK;
}
