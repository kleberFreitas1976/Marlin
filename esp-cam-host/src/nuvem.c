#include "nuvem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "impressao.h"
#include "nvs.h"
#include "rede.h"
#include "reles.h"

static const char *TAG = "nuvem";

static nuvem_config_t s_cfg = {.intervalo_s = 20, .foto_s = 60};
static nuvem_estado_t s_est;
static nuvem_estado_json_t s_estado_json;
static bool s_foto_ja;              // o site pediu uma foto

// Confirmacoes que nao chegaram ao site (Wi-Fi fraco): reenviadas no proximo contato,
// senao o comando aparece la como "entregue" para sempre.
#define CONF_MAX 8
static char s_conf[CONF_MAX][160];
static int s_n_conf;

static void guarda_confirmacao(const char *consulta)
{
    if (s_n_conf < CONF_MAX) strlcpy(s_conf[s_n_conf++], consulta, sizeof s_conf[0]);
}
static int64_t s_ultima_foto;

// ------------------------------------------------------------------ configuracao
static void le_config(void)
{
    nvs_handle_t h;
    if (nvs_open("nuvem", NVS_READONLY, &h) != ESP_OK) return;
    uint8_t u;
    int32_t i;
    size_t n;
    if (nvs_get_u8(h, "ativo", &u) == ESP_OK) s_cfg.ativo = u;
    n = sizeof s_cfg.url;
    nvs_get_str(h, "url", s_cfg.url, &n);
    n = sizeof s_cfg.chave;
    nvs_get_str(h, "chave", s_cfg.chave, &n);
    if (nvs_get_i32(h, "intervalo", &i) == ESP_OK) s_cfg.intervalo_s = i;
    if (nvs_get_i32(h, "foto", &i) == ESP_OK) s_cfg.foto_s = i;
    nvs_close(h);
}

void nuvem_config(nuvem_config_t *c)
{
    *c = s_cfg;
    c->chave[0] = '\0';
}

bool nuvem_tem_chave(void) { return s_cfg.chave[0] != '\0'; }

esp_err_t nuvem_grava_config(const nuvem_config_t *c, bool trocar_chave)
{
    char chave[sizeof s_cfg.chave];
    strlcpy(chave, s_cfg.chave, sizeof chave);
    s_cfg = *c;
    if (!trocar_chave) strlcpy(s_cfg.chave, chave, sizeof s_cfg.chave);
    if (s_cfg.intervalo_s < 5) s_cfg.intervalo_s = 5;
    if (s_cfg.foto_s < 10) s_cfg.foto_s = 10;
    nvs_handle_t h;
    esp_err_t err = nvs_open("nuvem", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_set_u8(h, "ativo", s_cfg.ativo);
    nvs_set_str(h, "url", s_cfg.url);
    nvs_set_str(h, "chave", s_cfg.chave);
    nvs_set_i32(h, "intervalo", s_cfg.intervalo_s);
    nvs_set_i32(h, "foto", s_cfg.foto_s);
    err = nvs_commit(h);
    nvs_close(h);
    s_est.erro[0] = '\0';
    return err;
}

void nuvem_estado(nuvem_estado_t *e) { *e = s_est; }

// ------------------------------------------------------------------ HTTP
typedef struct {
    char *buf;
    size_t n, tam;
} resposta_t;

static esp_err_t evento(esp_http_client_event_t *ev)
{
    resposta_t *r = ev->user_data;
    if (ev->event_id == HTTP_EVENT_ON_DATA && r && r->n + ev->data_len < r->tam) {
        memcpy(r->buf + r->n, ev->data, ev->data_len);
        r->n += ev->data_len;
        r->buf[r->n] = '\0';
    }
    return ESP_OK;
}

// POST <url>?<consulta> com o corpo dado. Devolve o status HTTP (ou -1).
static int posta(const char *consulta, const char *tipo, const void *corpo, size_t n,
                 char *resp, size_t tam_resp)
{
    char url[300];
    snprintf(url, sizeof url, "%s%s%s", s_cfg.url, strchr(s_cfg.url, '?') ? "&" : "?", consulta);
    resposta_t r = {.buf = resp, .n = 0, .tam = tam_resp};
    if (resp && tam_resp) resp[0] = '\0';
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,   // HTTPS com os certificados do ESP-IDF
        .event_handler = evento,
        .user_data = resp ? &r : NULL,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return -1;
    esp_http_client_set_header(c, "X-Chave", s_cfg.chave);
    esp_http_client_set_header(c, "Content-Type", tipo);
    esp_http_client_set_post_field(c, corpo, n);
    esp_err_t err = esp_http_client_perform(c);
    int status = err == ESP_OK ? esp_http_client_get_status_code(c) : -1;
    if (err != ESP_OK) snprintf(s_est.erro, sizeof s_est.erro, "conexao: %s", esp_err_to_name(err));
    else if (status != 200) snprintf(s_est.erro, sizeof s_est.erro, "o site respondeu %d%s", status,
                                     status == 403 ? " (chave errada?)" : "");
    esp_http_client_cleanup(c);
    return status;
}

// ------------------------------------------------------------------ comandos
// Valor de "chave" a partir de p, aceitando espacos em volta do ':'. Devolve o
// ponto logo apos o valor, ou NULL.
static const char *valor(const char *p, const char *chave, char *saida, size_t tam)
{
    char alvo[24];
    snprintf(alvo, sizeof alvo, "\"%s\"", chave);
    p = strstr(p, alvo);
    if (!p) return NULL;
    p += strlen(alvo);
    while (*p == ' ' || *p == ':') p++;
    bool aspas = *p == '"';
    if (aspas) p++;
    size_t k = 0;
    while (*p && k < tam - 1 && (aspas ? *p != '"' : (*p != ',' && *p != '}' && *p != ' ')))
        saida[k++] = *p++;
    saida[k] = '\0';
    return p;
}

// {"ok":true,"comandos":[{"id":"a1b2","acao":"desliga_impressora","forcar":false},...]}
static void executa_comandos(const char *resp)
{
    const char *p = strstr(resp, "\"comandos\"");
    while (p) {
        char id[24] = "", acao[32] = "", f[8] = "";
        const char *a = valor(p, "id", id, sizeof id);
        if (!a) break;
        a = valor(a, "acao", acao, sizeof acao);
        if (!a) break;
        const char *fim = strchr(a, '}');
        bool forcar = valor(a, "forcar", f, sizeof f) && (!fim || strstr(a, "\"forcar\"") < fim) &&
                      !strcmp(f, "true");
        char motivo[80] = "ok";
        esp_err_t r = ESP_OK;
        if (!strcmp(acao, "desliga_impressora")) r = reles_desliga_impressora(forcar, motivo, sizeof motivo);
        else if (!strcmp(acao, "liga_impressora")) r = reles_liga_impressora();
        else if (!strcmp(acao, "desliga_tudo")) r = reles_desliga_tudo(forcar, motivo, sizeof motivo);
        else if (!strcmp(acao, "cancela")) reles_cancela_espera();
        else if (!strcmp(acao, "foto")) s_foto_ja = true;
        else strlcpy(motivo, "acao desconhecida", sizeof motivo);
        if (r != ESP_OK && !strcmp(motivo, "ok")) strlcpy(motivo, esp_err_to_name(r), sizeof motivo);
        ESP_LOGI(TAG, "comando do site: %s%s -> %s", acao, forcar ? " (forcado)" : "", motivo);
        // confirma ao site
        char q[160], esc[100];
        size_t k = 0;
        for (const char *c = motivo; *c && k < sizeof esc - 4; c++) {
            if ((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9')) esc[k++] = *c;
            else if (*c == ' ') esc[k++] = '+';
        }
        esc[k] = '\0';
        snprintf(q, sizeof q, "acao=feito&id=%s&resultado=%s", id, esc);
        if (posta(q, "text/plain", "", 0, NULL, 0) != 200) guarda_confirmacao(q);
        p = a;
    }
}

// Envio em pedacos de 4 KB, cada um conferido: com Wi-Fi fraco o envio de uma vez
// so falhava no meio sem aviso e o site recebia meia foto.
static int posta_grande(const char *consulta, const char *tipo, const uint8_t *corpo, size_t n)
{
    char url[300];
    snprintf(url, sizeof url, "%s%s%s", s_cfg.url, strchr(s_cfg.url, '?') ? "&" : "?", consulta);
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 20000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return -1;
    esp_http_client_set_header(c, "X-Chave", s_cfg.chave);
    esp_http_client_set_header(c, "Content-Type", tipo);
    int status = -1;
    if (esp_http_client_open(c, n) == ESP_OK) {
        size_t enviado = 0;
        while (enviado < n) {
            int w = esp_http_client_write(c, (const char *)corpo + enviado,
                                          n - enviado > 4096 ? 4096 : n - enviado);
            if (w <= 0) break;
            enviado += w;
        }
        if (enviado == n && esp_http_client_fetch_headers(c) >= 0)
            status = esp_http_client_get_status_code(c);
        else
            snprintf(s_est.erro, sizeof s_est.erro, "foto: envio interrompido em %u de %u bytes",
                     (unsigned)enviado, (unsigned)n);
    } else {
        snprintf(s_est.erro, sizeof s_est.erro, "foto: sem conexao com o site");
    }
    if (status > 0 && status != 200)
        snprintf(s_est.erro, sizeof s_est.erro, "foto: o site respondeu %d", status);
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return status;
}

// ------------------------------------------------------------------ tarefa
static void envia_foto(void)
{
    camera_fb_t *fb = camera_foto();
    if (!fb) return;
    int st = posta_grande("acao=foto", "image/jpeg", fb->buf, fb->len);
    camera_devolve(fb);
    if (st == 200) s_ultima_foto = esp_timer_get_time();
    else ESP_LOGW(TAG, "%s", s_est.erro);
}

static void tarefa(void *arg)
{
    char *estado = malloc(2400);
    char *resp = malloc(2048);
    while (true) {
        vTaskDelay(pdMS_TO_TICKS((s_cfg.intervalo_s > 0 ? s_cfg.intervalo_s : 20) * 1000));
        if (!s_cfg.ativo || !s_cfg.url[0] || !s_cfg.chave[0] || !rede_conectado() || !estado || !resp)
            continue;
        size_t n = s_estado_json(estado, 2400);
        int st = posta("acao=status", "application/json", estado, n, resp, 2048);
        if (st == 200) {
            s_est.ultimo_ok = esp_timer_get_time() / 1000000;
            s_est.envios++;
            s_est.erro[0] = '\0';
            // primeiro as confirmacoes atrasadas, depois os comandos novos
            int k = 0;
            for (int i = 0; i < s_n_conf; i++)
                if (posta(s_conf[i], "text/plain", "", 0, NULL, 0) != 200) strlcpy(s_conf[k++], s_conf[i], sizeof s_conf[0]);
            s_n_conf = k;
            executa_comandos(resp);
        } else {
            s_est.falhas++;
            continue;
        }
        imp_info_t i;
        imp_info(&i);
        bool imprimindo = imp_ativa() || i.tft;
        if (s_foto_ja || (imprimindo && esp_timer_get_time() - s_ultima_foto > s_cfg.foto_s * 1000000LL)) {
            s_foto_ja = false;
            envia_foto();
        }
    }
}

void nuvem_inicia(nuvem_estado_json_t estado_json)
{
    s_estado_json = estado_json;
    le_config();
    xTaskCreatePinnedToCore(tarefa, "nuvem", 8192, NULL, 2, NULL, 0);
}
