// Arquivos G-code, impressao e a parte da API do OctoPrint que o OrcaSlicer usa.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arquivos.h"
#include "cartao_sd.h"
#include "esp_log.h"
#include "impressao.h"
#include "segredos.h"
#include "web_interno.h"

static const char *TAG = "web_arq";

#define BLOCO 4096

static esp_err_t erro_json(httpd_req_t *req, const char *status, const char *msg)
{
    char j[200], e[160];
    web_json_esc(e, sizeof e, msg);
    snprintf(j, sizeof j, "{\"erro\":\"%s\"}", e);
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, j);
}

// ------------------------------------------------------------------ lista, envio, apaga
static esp_err_t lista(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    if (!sd_ok()) return erro_json(req, "503 Service Unavailable", "sem cartao SD");
    char *j = arq_lista_json();
    if (!j) return erro_json(req, "500 Internal Server Error", "sem memoria");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t r = httpd_resp_sendstr(req, j);
    free(j);
    return r;
}

// POST /api/arquivos?nome=peca.gcode, corpo = o arquivo
static esp_err_t envia(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    char nome[ARQ_NOME_MAX + 1];
    if (!web_parametro(req, "nome", nome, sizeof nome) || !arq_nome_valido(nome))
        return erro_json(req, "400 Bad Request", "nome invalido (use .gcode)");
    imp_info_t i;
    imp_info(&i);
    if (imp_ativa() && !strcmp(i.nome, nome))
        return erro_json(req, "409 Conflict", "este arquivo esta sendo impresso");
    arq_envio_t *e = arq_envio_abre(nome);
    if (!e) return erro_json(req, "503 Service Unavailable", "nao consegui gravar no SD");
    uint8_t *buf = malloc(BLOCO);
    size_t falta = req->content_len;
    bool ok = buf != NULL;
    while (ok && falta) {
        int r = httpd_req_recv(req, (char *)buf, falta < BLOCO ? falta : BLOCO);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) ok = false;
        else {
            ok = arq_envio_dados(e, buf, r) == ESP_OK;
            falta -= r;
        }
    }
    free(buf);
    if (arq_envio_fecha(e, ok) != ESP_OK) return erro_json(req, "500 Internal Server Error", "envio interrompido");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t apaga(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    char nome[ARQ_NOME_MAX + 1];
    if (!web_parametro(req, "nome", nome, sizeof nome)) return erro_json(req, "400 Bad Request", "sem nome");
    imp_info_t i;
    imp_info(&i);
    if (imp_ativa() && !strcmp(i.nome, nome))
        return erro_json(req, "409 Conflict", "este arquivo esta sendo impresso");
    if (arq_apaga(nome) != ESP_OK) return erro_json(req, "404 Not Found", "arquivo nao encontrado");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t manda_arquivo(httpd_req_t *req, const char *caminho, const char *tipo)
{
    FILE *f = fopen(caminho, "rb");
    if (!f) return erro_json(req, "404 Not Found", "nao encontrado");
    httpd_resp_set_type(req, tipo);
    char *buf = malloc(BLOCO);
    size_t n;
    esp_err_t r = ESP_OK;
    while (buf && r == ESP_OK && (n = fread(buf, 1, BLOCO, f)) > 0) r = httpd_resp_send_chunk(req, buf, n);
    free(buf);
    fclose(f);
    if (r == ESP_OK) httpd_resp_send_chunk(req, NULL, 0);
    return r;
}

static esp_err_t miniatura(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    char nome[ARQ_NOME_MAX + 1], c[200];
    const char *tipo;
    if (!web_parametro(req, "nome", nome, sizeof nome) || !arq_miniatura(nome, c, sizeof c, &tipo))
        return erro_json(req, "404 Not Found", "sem miniatura");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");
    return manda_arquivo(req, c, tipo);
}

static esp_err_t baixa(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    char nome[ARQ_NOME_MAX + 1], c[200];
    if (!web_parametro(req, "nome", nome, sizeof nome) || !arq_nome_valido(nome))
        return erro_json(req, "400 Bad Request", "nome invalido");
    arq_caminho(nome, c, sizeof c);
    return manda_arquivo(req, c, "text/plain");
}

// ------------------------------------------------------------------ impressao
static esp_err_t imprime(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    char nome[ARQ_NOME_MAX + 1], erro[96];
    if (!web_parametro(req, "nome", nome, sizeof nome)) return erro_json(req, "400 Bad Request", "sem nome");
    if (imp_imprime(nome, erro, sizeof erro) != ESP_OK) return erro_json(req, "409 Conflict", erro);
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t controle(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    char acao[16];
    if (!web_parametro(req, "acao", acao, sizeof acao)) return erro_json(req, "400 Bad Request", "sem acao");
    if (!strcmp(acao, "pausa")) imp_pausa();
    else if (!strcmp(acao, "retoma")) imp_retoma();
    else if (!strcmp(acao, "cancela")) imp_cancela();
    else return erro_json(req, "400 Bad Request", "acao desconhecida");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

size_t web_impressao_json(char *j, size_t tam)
{
    static const char *const EST[] = {"parada", "imprimindo", "pausando", "pausada",
                                      "cancelando", "concluida", "cancelada", "erro"};
    imp_info_t i;
    imp_info(&i);
    char nome[180], aviso[200], tft[180];
    web_json_esc(nome, sizeof nome, i.nome);
    web_json_esc(aviso, sizeof aviso, i.aviso);
    web_json_esc(tft, sizeof tft, i.tft_nome);
    return snprintf(j, tam,
        "{\"estado\":\"%s\",\"nome\":\"%s\",\"bytes\":%u,\"total\":%u,\"linhas\":%u,"
        "\"camada\":%d,\"camadas\":%d,\"decorrido\":%d,\"estimado\":%d,\"restante\":%d,"
        "\"aviso\":\"%s\",\"tft\":%s,\"tftNome\":\"%s\",\"tftCamada\":%d,\"tftCamadas\":%d,"
        "\"tftDecorrido\":%d,\"fimHa\":%d,\"fimOk\":%s}",
        EST[i.estado], nome, (unsigned)i.bytes, (unsigned)i.total, (unsigned)i.linhas, i.camada,
        i.camadas, i.decorrido_s, i.estimado_s, i.restante_s, aviso, i.tft ? "true" : "false",
        tft, i.tft_camada, i.tft_camadas, i.tft_decorrido_s, i.fim_ha_s, i.fim_ok ? "true" : "false");
}

// ------------------------------------------------------------------ OctoPrint
// O OrcaSlicer (e o PrusaSlicer) cadastram a impressora como "OctoPrint" e usam
// so isto: GET /api/version para testar e POST /api/files/local (multipart) para
// enviar, com print=true para imprimir em seguida. A chave (X-Api-Key) e a senha
// da pagina.
static bool chave_ok(httpd_req_t *req)
{
    char k[80];
    if (httpd_req_get_hdr_value_str(req, "X-Api-Key", k, sizeof k) == ESP_OK)
        return WEB_SENHA[0] == '\0' || !strcmp(k, WEB_SENHA);
    return web_senha_ok(req);
}

static esp_err_t op_versao(httpd_req_t *req)
{
    if (!chave_ok(req)) return erro_json(req, "403 Forbidden", "chave invalida");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"api\":\"0.1\",\"server\":\"1.9.3\","
                                   "\"text\":\"OctoPrint 1.9.3 (ESP32-S3-CAM)\"}");
}

// Le o corpo multipart em blocos. Cada parte: cabecalhos ate a linha em branco,
// depois os dados ate "\r\n--<limite>". So a parte "file" e gravada; os campos
// pequenos ("print", "select") sao guardados.
static esp_err_t op_envio(httpd_req_t *req)
{
    if (!chave_ok(req)) return erro_json(req, "403 Forbidden", "chave invalida");
    char ct[160];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", ct, sizeof ct) != ESP_OK)
        return erro_json(req, "400 Bad Request", "sem Content-Type");
    const char *b = strstr(ct, "boundary=");
    if (!b) return erro_json(req, "400 Bad Request", "sem boundary");
    b += 9;
    char delim[90];
    int nd = snprintf(delim, sizeof delim, "\r\n--%.*s", (int)strcspn(b, "\"; "), *b == '"' ? b + 1 : b);

    // buffer comeca com "\r\n" para o primeiro limite ser igual aos demais
    char *buf = malloc(BLOCO + 200);
    if (!buf) return erro_json(req, "500 Internal Server Error", "sem memoria");
    size_t n = 2;
    buf[0] = '\r';
    buf[1] = '\n';
    size_t falta = req->content_len;
    enum { PROCURA, CABECALHO, DADOS, FIM } est = PROCURA;
    char campo[32] = "", arquivo[ARQ_NOME_MAX + 1] = "", valor[16] = "";
    size_t n_valor = 0;
    bool imprimir = false, erro = false;
    arq_envio_t *e = NULL;

    while (!erro && est != FIM) {
        if (falta && n < BLOCO) {
            int r = httpd_req_recv(req, buf + n, (BLOCO - n) < falta ? (BLOCO - n) : falta);
            if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
            if (r <= 0) { erro = true; break; }
            n += r;
            falta -= r;
        }
        if (est == PROCURA || est == DADOS) {
            char *d = memmem(buf, n, delim, nd);
            size_t usar = d ? (size_t)(d - buf) : (n > (size_t)nd ? n - nd : 0);
            if (est == DADOS && usar) {
                if (e) erro = arq_envio_dados(e, (uint8_t *)buf, usar) != ESP_OK;
                else {
                    size_t k = usar < sizeof valor - 1 - n_valor ? usar : sizeof valor - 1 - n_valor;
                    memcpy(valor + n_valor, buf, k);
                    n_valor += k;
                }
            }
            if (d) {
                if (est == DADOS) {          // fim de uma parte
                    if (e) {
                        erro = arq_envio_fecha(e, true) != ESP_OK;
                        e = NULL;
                    } else if (!strcmp(campo, "print")) {
                        valor[n_valor] = '\0';
                        imprimir = !strncmp(valor, "true", 4);
                    }
                }
                size_t pula = (d - buf) + nd;
                if (n >= pula + 2 && buf[pula] == '-' && buf[pula + 1] == '-') { est = FIM; break; }
                memmove(buf, buf + pula, n - pula);
                n -= pula;
                est = CABECALHO;
            } else {
                memmove(buf, buf + usar, n - usar);
                n -= usar;
                if (!falta && n <= (size_t)nd) erro = true;   // acabou sem o limite final
            }
        }
        if (est == CABECALHO) {
            buf[n < BLOCO + 199 ? n : BLOCO + 199] = '\0';
            char *fim = strstr(buf, "\r\n\r\n");
            if (!fim) {
                if (!falta || n >= BLOCO) erro = true;   // cabecalho sem fim ou grande demais
                continue;
            }
            *fim = '\0';
            campo[0] = arquivo[0] = '\0';
            char *p = strstr(buf, "name=\"");
            if (p) snprintf(campo, sizeof campo, "%.*s", (int)strcspn(p + 6, "\""), p + 6);
            p = strstr(buf, "filename=\"");
            if (p) {
                const char *nomep = p + 10;
                size_t k = strcspn(nomep, "\"");
                const char *barra = NULL;      // so o nome, sem pastas
                for (size_t i = 0; i < k; i++)
                    if (nomep[i] == '/' || nomep[i] == '\\') barra = nomep + i;
                if (barra) {
                    k -= (barra + 1 - nomep);
                    nomep = barra + 1;
                }
                snprintf(arquivo, sizeof arquivo, "%.*s", (int)k, nomep);
            }
            size_t pula = (fim + 4) - buf;
            memmove(buf, buf + pula, n - pula);
            n -= pula;
            n_valor = 0;
            if (!strcmp(campo, "file")) {
                if (!arq_nome_valido(arquivo)) {
                    free(buf);
                    return erro_json(req, "415 Unsupported Media Type", "arquivo precisa ser .gcode");
                }
                e = arq_envio_abre(arquivo);
                if (!e) erro = true;
            }
            est = DADOS;
        }
    }
    free(buf);
    if (e) arq_envio_fecha(e, false);
    if (erro || !arquivo[0]) return erro_json(req, "400 Bad Request", "envio incompleto");
    ESP_LOGI(TAG, "OctoPrint: recebido %s%s", arquivo, imprimir ? ", imprimindo" : "");

    char motivo[96] = "";
    if (imprimir && imp_imprime(arquivo, motivo, sizeof motivo) != ESP_OK)
        ESP_LOGW(TAG, "nao comecou: %s", motivo);
    char nomej[180], j[600];
    web_json_esc(nomej, sizeof nomej, arquivo);
    snprintf(j, sizeof j, "{\"done\":true,\"files\":{\"local\":{\"name\":\"%s\",\"origin\":\"local\","
                          "\"refs\":{\"resource\":\"/api/files/local/%s\"}}}}", nomej, nomej);
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, j);
}

void web_rotas_arquivos(httpd_handle_t s)
{
    const httpd_uri_t rotas[] = {
        {.uri = "/api/arquivos", .method = HTTP_GET, .handler = lista},
        {.uri = "/api/arquivos", .method = HTTP_POST, .handler = envia},
        {.uri = "/api/arquivos", .method = HTTP_DELETE, .handler = apaga},
        {.uri = "/api/miniatura", .method = HTTP_GET, .handler = miniatura},
        {.uri = "/api/arquivo", .method = HTTP_GET, .handler = baixa},
        {.uri = "/api/imprimir", .method = HTTP_POST, .handler = imprime},
        {.uri = "/api/impressao", .method = HTTP_POST, .handler = controle},
        {.uri = "/api/version", .method = HTTP_GET, .handler = op_versao},
        {.uri = "/api/files/local", .method = HTTP_POST, .handler = op_envio},
    };
    for (size_t i = 0; i < sizeof rotas / sizeof rotas[0]; i++) httpd_register_uri_handler(s, &rotas[i]);
}
