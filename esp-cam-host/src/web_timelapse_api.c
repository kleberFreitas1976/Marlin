// API do time-lapse: lista, configuracao, fotos, video, apagar, remontar.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera.h"
#include "cartao_sd.h"
#include "esp_camera.h"
#include "timelapse.h"
#include "web_interno.h"

#define BLOCO 8192

static esp_err_t json(httpd_req_t *req, const char *status, const char *txt)
{
    if (status) httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, txt);
}

static esp_err_t lista(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    if (!sd_ok()) return json(req, "503 Service Unavailable", "{\"erro\":\"sem cartao SD\"}");
    tl_config_t c;
    tl_config(&c);
    char pasta[96];
    int fotos;
    bool montando;
    tl_status(pasta, sizeof pasta, &fotos, &montando);
    char *l = tl_lista_json();
    if (!l) return json(req, "500 Internal Server Error", "{\"erro\":\"sem memoria\"}");
    size_t tam = strlen(l) + 400;
    char *j = malloc(tam);
    if (!j) {
        free(l);
        return json(req, "500 Internal Server Error", "{\"erro\":\"sem memoria\"}");
    }
    snprintf(j, tam,
             "{\"config\":{\"ativo\":%s,\"modo\":%d,\"res\":%d,\"fps\":%d,\"girar\":%s},"
             "\"gravando\":%s,\"pasta\":\"%s\",\"fotos\":%d,\"montando\":%s,\"lista\":%s}",
             c.ativo ? "true" : "false", c.modo, c.resolucao, c.fps,
             camera_girada() ? "true" : "false", tl_gravando() ? "true" : "false",
             pasta, fotos, montando ? "true" : "false", l);
    esp_err_t r = json(req, NULL, j);
    free(l);
    free(j);
    return r;
}

// POST /api/timelapse/config?ativo=1&modo=0&res=13&fps=25&girar=1
static esp_err_t config(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    tl_config_t c;
    tl_config(&c);
    char v[16];
    if (web_parametro(req, "ativo", v, sizeof v)) c.ativo = atoi(v) != 0;
    if (web_parametro(req, "modo", v, sizeof v)) c.modo = (tl_modo_t)atoi(v);
    if (web_parametro(req, "res", v, sizeof v)) {
        int r = atoi(v);
        if (r >= FRAMESIZE_VGA && r <= FRAMESIZE_QSXGA) c.resolucao = r;
    }
    if (web_parametro(req, "fps", v, sizeof v)) c.fps = atoi(v);
    if (web_parametro(req, "girar", v, sizeof v) && camera_girar(atoi(v) != 0) != ESP_OK)
        return json(req, "500 Internal Server Error", "{\"erro\":\"nao gravou\"}");
    if (tl_grava_config(&c) != ESP_OK) return json(req, "500 Internal Server Error", "{\"erro\":\"nao gravou\"}");
    return json(req, NULL, "{\"ok\":true}");
}

static esp_err_t manda(httpd_req_t *req, const char *caminho, const char *tipo, const char *baixar)
{
    FILE *f = fopen(caminho, "rb");
    if (!f) return json(req, "404 Not Found", "{\"erro\":\"nao encontrado\"}");
    httpd_resp_set_type(req, tipo);
    if (baixar) {
        char cab[160];
        snprintf(cab, sizeof cab, "attachment; filename=\"%s\"", baixar);
        httpd_resp_set_hdr(req, "Content-Disposition", cab);
    } else {
        httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");   // fotos nao mudam
    }
    char *buf = malloc(BLOCO);
    size_t n;
    esp_err_t r = ESP_OK;
    while (buf && r == ESP_OK && (n = fread(buf, 1, BLOCO, f)) > 0) r = httpd_resp_send_chunk(req, buf, n);
    free(buf);
    fclose(f);
    if (r == ESP_OK) httpd_resp_send_chunk(req, NULL, 0);
    return r;
}

// GET /api/timelapse/foto?pasta=X&n=12
static esp_err_t foto(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    char pasta[96], n[12], c[220];
    if (!web_parametro(req, "pasta", pasta, sizeof pasta) || !tl_pasta_valida(pasta) ||
        !web_parametro(req, "n", n, sizeof n))
        return json(req, "400 Bad Request", "{\"erro\":\"parametros\"}");
    snprintf(c, sizeof c, TL_PASTA "/%s/%05d.jpg", pasta, atoi(n));
    return manda(req, c, "image/jpeg", NULL);
}

static esp_err_t video(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    char pasta[96], c[220], nome[120];
    if (!web_parametro(req, "pasta", pasta, sizeof pasta) || !tl_pasta_valida(pasta))
        return json(req, "400 Bad Request", "{\"erro\":\"pasta\"}");
    snprintf(c, sizeof c, TL_PASTA "/%s/timelapse.avi", pasta);
    snprintf(nome, sizeof nome, "timelapse_%s.avi", pasta);
    return manda(req, c, "video/x-msvideo", nome);
}

static esp_err_t apaga(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    char pasta[96];
    if (!web_parametro(req, "pasta", pasta, sizeof pasta)) return json(req, "400 Bad Request", "{\"erro\":\"pasta\"}");
    esp_err_t e = tl_apaga(pasta);
    if (e == ESP_ERR_INVALID_STATE) return json(req, "409 Conflict", "{\"erro\":\"gravando agora\"}");
    if (e != ESP_OK) return json(req, "404 Not Found", "{\"erro\":\"nao apagou\"}");
    return json(req, NULL, "{\"ok\":true}");
}

static esp_err_t remonta(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    char pasta[96];
    if (!web_parametro(req, "pasta", pasta, sizeof pasta) || tl_monta_video(pasta) != ESP_OK)
        return json(req, "400 Bad Request", "{\"erro\":\"pasta\"}");
    return json(req, NULL, "{\"ok\":true}");
}

size_t web_timelapse_json(char *j, size_t tam)
{
    char pasta[96];
    int fotos;
    bool montando;
    tl_status(pasta, sizeof pasta, &fotos, &montando);
    return snprintf(j, tam, "{\"gravando\":%s,\"pasta\":\"%s\",\"fotos\":%d,\"montando\":%s}",
                    tl_gravando() ? "true" : "false", pasta, fotos, montando ? "true" : "false");
}

void web_rotas_timelapse(httpd_handle_t s)
{
    const httpd_uri_t rotas[] = {
        {.uri = "/api/timelapse", .method = HTTP_GET, .handler = lista},
        {.uri = "/api/timelapse", .method = HTTP_DELETE, .handler = apaga},
        {.uri = "/api/timelapse/config", .method = HTTP_POST, .handler = config},
        {.uri = "/api/timelapse/foto", .method = HTTP_GET, .handler = foto},
        {.uri = "/api/timelapse/video", .method = HTTP_GET, .handler = video},
        {.uri = "/api/timelapse/montar", .method = HTTP_POST, .handler = remonta},
    };
    for (size_t i = 0; i < sizeof rotas / sizeof rotas[0]; i++) httpd_register_uri_handler(s, &rotas[i]);
}
