#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define SD_RAIZ "/sd"

esp_err_t sd_inicia(void);
bool sd_ok(void);
uint64_t sd_total_bytes(void);
uint64_t sd_livre_bytes(void);

// Formata (MBR + FAT32) um cartao que nao montou. Nunca apaga um FAT valido.
esp_err_t sd_formata(void);

// Lista a pasta no console (diagnostico)
void sd_lista(const char *pasta);
