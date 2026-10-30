#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// Rede de configuracao, quando nao ha Wi-Fi conhecido ou ele nao conecta
#define AP_SSID   "Impressora-CAM"
#define AP_SENHA  "12345678"
// Faixa pouco comum de proposito: o padrao do ESP-IDF (192.168.4.x) coincidiu com a
// rede de casa, e duas interfaces na mesma faixa confundem as rotas.
#define AP_IP     "192.168.71.1"
#define AP_IP_BYTES 192, 168, 71, 1

// Liga o Wi-Fi: conecta a rede gravada (NVS; na primeira vez, a de secrets.ini).
// Sem rede ou sem conexao em REDE_ESPERA_S, liga tambem o AP de configuracao.
#define REDE_ESPERA_S   20
// Depois de conectar, o AP continua por este tempo (para ver o IP novo) e desliga
#define REDE_AP_SOBRA_S 120

esp_err_t rede_inicia(void);

bool rede_conectado(void);          // conectado na rede de casa
bool rede_ap_ativo(void);
const char *rede_ip(void);          // IP na rede de casa ("" se nao)
const char *rede_ssid(void);        // rede configurada ("" se nenhuma)
int rede_rssi(void);                // sinal da rede de casa, dBm (0 se desconectado)

// Grava a rede e tenta conectar. O AP fica ligado ate conectar.
esp_err_t rede_configura(const char *ssid, const char *senha);

// Procura redes e escreve um JSON [{"ssid":"..","rssi":-50,"aberta":false},...]
esp_err_t rede_procura(char *json, size_t tam);
