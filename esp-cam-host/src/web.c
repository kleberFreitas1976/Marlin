// Wi-Fi e servidor web da fase 1: pagina com foto e video ao vivo.
// O video fica num segundo servidor (porta 81): ele prende a tarefa do servidor
// enquanto transmite, e assim a pagina principal continua respondendo.

#include "web.h"

#include <stdio.h>
#include <string.h>

#include "camera.h"
#include "cartao_sd.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mbedtls/base64.h"
#include "segredos.h"

static const char *TAG = "web";

static bool s_conectado;
static char s_ip[16] = "";

// ------------------------------------------------------------------ senha
// HTTP Basic: qualquer usuario, a senha e a de secrets.ini (web_senha).
static bool autorizado(httpd_req_t *req)
{
    if (WEB_SENHA[0] == '\0') return true;
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
    char html[1800];
    snprintf(html, sizeof html,
        "<!doctype html><html lang=pt-br><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>ESP32-S3-CAM</title>"
        "<style>body{background:#16191d;color:#e6e9ec;font-family:Segoe UI,sans-serif;margin:16px}"
        "img{max-width:100%%;border-radius:8px;background:#000}a{color:#4aa3ff}"
        "td{padding:2px 12px 2px 0}</style>"
        "<h2>ESP32-S3-CAM &mdash; fase 1</h2>"
        "<table><tr><td>Sensor</td><td>%s (PID 0x%04x), max %dx%d</td></tr>"
        "<tr><td>PSRAM</td><td>%u KB</td></tr>"
        "<tr><td>Cartao SD</td><td>%s</td></tr></table>"
        "<p><a href='/foto.jpg'>Foto</a> &middot; <a id=v href='#'>Video ao vivo</a></p>"
        "<img id=i src='/foto.jpg'>"
        "<script>document.getElementById('v').onclick=function(e){e.preventDefault();"
        "document.getElementById('i').src=location.protocol+'//'+location.hostname+':81/stream'};"
        "</script></html>",
        camera_nome(), camera_pid(), camera_largura_max(), camera_altura_max(),
        (unsigned)(esp_psram_get_size() / 1024),
        sd_ok() ? "montado" : "ausente");
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

static void sobe_servidores(void)
{
    httpd_handle_t h = NULL;
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&h, &c) == ESP_OK) {
        httpd_uri_t u1 = {.uri = "/", .method = HTTP_GET, .handler = pag_inicio};
        httpd_uri_t u2 = {.uri = "/foto.jpg", .method = HTTP_GET, .handler = pag_foto};
        httpd_register_uri_handler(h, &u1);
        httpd_register_uri_handler(h, &u2);
    }
    httpd_handle_t hv = NULL;
    httpd_config_t cv = HTTPD_DEFAULT_CONFIG();
    cv.server_port = 81;
    cv.ctrl_port = 32769;
    if (httpd_start(&hv, &cv) == ESP_OK) {
        httpd_uri_t u = {.uri = "/stream", .method = HTTP_GET, .handler = pag_stream};
        httpd_register_uri_handler(hv, &u);
    }
}

// ------------------------------------------------------------------ Wi-Fi
static void evento(void *arg, esp_event_base_t base, int32_t id, void *dados)
{
    if (base == WIFI_EVENT && (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED)) {
        if (id == WIFI_EVENT_STA_DISCONNECTED && s_conectado)
            ESP_LOGW(TAG, "Wi-Fi caiu, reconectando");
        s_conectado = false;
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)dados;
        snprintf(s_ip, sizeof s_ip, IPSTR, IP2STR(&e->ip_info.ip));
        s_conectado = true;
        ESP_LOGI(TAG, "conectado: abra http://%s/", s_ip);
    }
}

esp_err_t web_inicia(void)
{
    if (WIFI_SSID[0] == '\0') {
        ESP_LOGW(TAG, "sem Wi-Fi configurado (secrets.ini): servidor web desligado");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(netif, "impressora-cam");

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, evento, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, evento, NULL);

    wifi_config_t sta = {0};
    strlcpy((char *)sta.sta.ssid, WIFI_SSID, sizeof sta.sta.ssid);
    strlcpy((char *)sta.sta.password, WIFI_SENHA, sizeof sta.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);       // video sem engasgos
    ESP_LOGI(TAG, "conectando a \"%s\"...", WIFI_SSID);

    sobe_servidores();
    return ESP_OK;
}

bool web_conectado(void) { return s_conectado; }
const char *web_ip(void) { return s_ip; }
