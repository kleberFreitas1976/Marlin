// API da configuracao da nuvem (site na hospedagem).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "nuvem.h"
#include "web_interno.h"

static esp_err_t le(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    nuvem_config_t c;
    nuvem_estado_t e;
    nuvem_config(&c);
    nuvem_estado(&e);
    char url[340], erro[200], j[700];
    web_json_esc(url, sizeof url, c.url);
    web_json_esc(erro, sizeof erro, e.erro);
    long ha = e.ultimo_ok ? (long)(esp_timer_get_time() / 1000000 - e.ultimo_ok) : -1;
    snprintf(j, sizeof j,
             "{\"ativo\":%s,\"url\":\"%s\",\"temChave\":%s,\"intervalo\":%d,\"foto\":%d,"
             "\"haSegundos\":%ld,\"erro\":\"%s\",\"envios\":%u,\"falhas\":%u}",
             c.ativo ? "true" : "false", url, nuvem_tem_chave() ? "true" : "false", c.intervalo_s,
             c.foto_s, ha, erro, (unsigned)e.envios, (unsigned)e.falhas);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, j);
}

// Le um campo de um corpo "a=b&c=d" (decodifica %XX e '+')
static bool campo(const char *corpo, const char *chave, char *saida, size_t tam)
{
    char v[260];
    if (httpd_query_key_value(corpo, chave, v, sizeof v) != ESP_OK) return false;
    size_t o = 0;
    for (const char *p = v; *p && o < tam - 1; p++) {
        if (*p == '+') saida[o++] = ' ';
        else if (*p == '%' && p[1] && p[2]) {
            char h[3] = {p[1], p[2], 0};
            saida[o++] = (char)strtol(h, NULL, 16);
            p += 2;
        } else saida[o++] = *p;
    }
    saida[o] = '\0';
    return true;
}

// POST /api/nuvem, corpo de formulario: ativo, url, chave (vazia = manter), intervalo, foto
static esp_err_t grava(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    char corpo[700];
    int n = req->content_len < sizeof corpo - 1 ? req->content_len : sizeof corpo - 1, lido = 0;
    while (lido < n) {
        int r = httpd_req_recv(req, corpo + lido, n - lido);
        if (r <= 0) return ESP_FAIL;
        lido += r;
    }
    corpo[lido] = '\0';
    nuvem_config_t c;
    nuvem_config(&c);
    char v[260];
    if (campo(corpo, "ativo", v, sizeof v)) c.ativo = atoi(v) != 0;
    if (campo(corpo, "url", v, sizeof v)) strlcpy(c.url, v, sizeof c.url);
    if (campo(corpo, "intervalo", v, sizeof v)) c.intervalo_s = atoi(v);
    if (campo(corpo, "foto", v, sizeof v)) c.foto_s = atoi(v);
    bool troca = campo(corpo, "chave", v, sizeof v) && v[0];
    if (troca) strlcpy(c.chave, v, sizeof c.chave);
    if (c.url[0] && strncmp(c.url, "https://", 8)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"erro\":\"o endereco precisa comecar com https://\"}");
    }
    nuvem_grava_config(&c, troca);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

void web_rotas_nuvem(httpd_handle_t s)
{
    const httpd_uri_t r1 = {.uri = "/api/nuvem", .method = HTTP_GET, .handler = le};
    const httpd_uri_t r2 = {.uri = "/api/nuvem", .method = HTTP_POST, .handler = grava};
    httpd_register_uri_handler(s, &r1);
    httpd_register_uri_handler(s, &r2);
}
