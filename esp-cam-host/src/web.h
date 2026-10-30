#pragma once
#include "esp_err.h"
#include "marlin.h"

// Servidor web: interface (web/), API, WebSocket ao vivo, foto, video (porta 81)
// e configuracao do Wi-Fi. Funciona tanto na rede de casa quanto no AP.
esp_err_t web_inicia(void);

// Repassa uma linha do Marlin ao console dos navegadores conectados.
void web_linha_marlin(marlin_tipo_t tipo, const char *linha);
