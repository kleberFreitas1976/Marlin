#pragma once
// Uso interno entre os arquivos do servidor web.
#include <stdbool.h>
#include <stddef.h>
#include "esp_http_server.h"

// Confere a senha/token; se falhar, ja responde 401 e devolve false.
bool web_autorizado(httpd_req_t *req);
// Confere sem responder (para APIs com chave propria, como a do OctoPrint)
bool web_senha_ok(httpd_req_t *req);
// Parametro da URL ja decodificado (%XX e '+'); false se nao existe
bool web_parametro(httpd_req_t *req, const char *chave, char *saida, size_t tam);
// Copia texto para dentro de uma string JSON (sem as aspas de fora)
size_t web_json_esc(char *dst, size_t tam, const char *src);

// Rotas de arquivos, impressao e da API do OctoPrint (web_arquivos_api.c)
void web_rotas_arquivos(httpd_handle_t srv);
// Estado da impressao em JSON (para o estado geral)
size_t web_impressao_json(char *j, size_t tam);

// Rotas e estado do time-lapse (web_timelapse_api.c)
void web_rotas_timelapse(httpd_handle_t srv);
size_t web_timelapse_json(char *j, size_t tam);
