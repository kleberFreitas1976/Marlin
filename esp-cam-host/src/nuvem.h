#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

// Envio para o site na hospedagem (esp-cam-host/hosting). A placa e quem procura
// o site: a cada `intervalo_s` manda o estado e recebe os comandos pendentes; durante
// a impressao, manda uma foto a cada `foto_s`. Nada e aberto no roteador.

typedef struct {
    bool ativo;
    char url[160];         // https://dominio/pasta/api.php
    char chave[80];        // a CHAVE do config.php do site
    int intervalo_s;
    int foto_s;
} nuvem_config_t;

typedef struct {
    int64_t ultimo_ok;     // segundos desde o boot do ultimo contato bem-sucedido (0 = nunca)
    char erro[96];         // ultimo erro ("" se nenhum)
    uint32_t envios, falhas;
} nuvem_estado_t;

// Monta o JSON de estado enviado ao site (fornecido pelo servidor web)
typedef size_t (*nuvem_estado_json_t)(char *j, size_t tam);

void nuvem_inicia(nuvem_estado_json_t estado_json);
void nuvem_config(nuvem_config_t *c);           // a chave volta vazia: nao sai da placa
esp_err_t nuvem_grava_config(const nuvem_config_t *c, bool trocar_chave);
void nuvem_estado(nuvem_estado_t *e);
bool nuvem_tem_chave(void);
