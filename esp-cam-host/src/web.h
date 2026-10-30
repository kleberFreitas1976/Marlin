#pragma once
#include <stdbool.h>
#include "esp_err.h"

// Conecta ao Wi-Fi (credenciais de secrets.ini) e sobe o servidor web.
// Sem credenciais, nao faz nada e devolve ESP_ERR_NOT_FOUND.
esp_err_t web_inicia(void);
bool web_conectado(void);
const char *web_ip(void);
