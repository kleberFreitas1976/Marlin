// API dos reles e da iluminacao.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reles.h"
#include "web_interno.h"

size_t web_reles_json(char *j, size_t tam)
{
    reles_estado_t e;
    reles_estado(&e);
    return snprintf(j, tam,
                    "{\"impressoraDesligada\":%s,\"aguardando\":%s,\"tudoAgendado\":%s,"
                    "\"ativoBaixo\":%s,\"luz\":%d,\"luzAuto\":%s,\"auto\":%s,\"autoMin\":%d,"
                    "\"autoFalta\":%d}",
                    e.impressora_desligada ? "true" : "false", e.aguardando_esfriar ? "true" : "false",
                    e.tudo_agendado ? "true" : "false", e.ativo_baixo ? "true" : "false", e.luz,
                    e.luz_auto ? "true" : "false", e.auto_desligar ? "true" : "false", e.auto_min,
                    e.auto_falta_s);
}

// POST /api/reles?acao=desliga_impressora|liga_impressora|desliga_tudo|cancela|luz|luz_auto|polaridade|auto
//                 [&forcar=1] [&valor=N] [&ativo=0|1 (auto: valor = minutos)]
static esp_err_t reles(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    char acao[24], v[12] = "", motivo[80] = "";
    if (!web_parametro(req, "acao", acao, sizeof acao)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"erro\":\"sem acao\"}");
    }
    bool forcar = web_parametro(req, "forcar", v, sizeof v) && atoi(v);
    v[0] = '\0';
    web_parametro(req, "valor", v, sizeof v);
    esp_err_t r = ESP_OK;
    if (!strcmp(acao, "desliga_impressora")) r = reles_desliga_impressora(forcar, motivo, sizeof motivo);
    else if (!strcmp(acao, "liga_impressora")) r = reles_liga_impressora();
    else if (!strcmp(acao, "desliga_tudo")) r = reles_desliga_tudo(forcar, motivo, sizeof motivo);
    else if (!strcmp(acao, "cancela")) reles_cancela_espera();
    else if (!strcmp(acao, "luz")) reles_luz(atoi(v));
    else if (!strcmp(acao, "luz_auto")) reles_luz_auto(atoi(v) != 0);
    else if (!strcmp(acao, "polaridade")) reles_polaridade(atoi(v) != 0);
    else if (!strcmp(acao, "auto")) {
        char a[4] = "1";
        web_parametro(req, "ativo", a, sizeof a);
        reles_auto(atoi(a) != 0, v[0] ? atoi(v) : -1);
    }
    else r = ESP_ERR_INVALID_ARG;

    char j[420], m[120];
    web_json_esc(m, sizeof m, motivo);
    size_t n = snprintf(j, sizeof j, "{\"ok\":%s,\"motivo\":\"%s\",\"estado\":", r == ESP_OK ? "true" : "false", m);
    n += web_reles_json(j + n, sizeof j - n);
    snprintf(j + n, sizeof j - n, "}");
    if (r != ESP_OK) httpd_resp_set_status(req, "409 Conflict");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, j);
}

void web_rotas_reles(httpd_handle_t s)
{
    const httpd_uri_t u = {.uri = "/api/reles", .method = HTTP_POST, .handler = reles};
    httpd_register_uri_handler(s, &u);
}
