#pragma once
#include "esp_err.h"

// Servidor web: pagina inicial, foto, video (porta 81) e configuracao do Wi-Fi.
// Funciona tanto na rede de casa quanto no AP de configuracao.
esp_err_t web_inicia(void);
