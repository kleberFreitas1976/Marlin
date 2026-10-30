#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "marlin.h"

// Impressao lida do cartao SD da placa e enviada ao Marlin pela janela do
// marlin.c (como o OctoPrint). Tambem acompanha impressoes feitas pela TFT24,
// pelas linhas "M118 P1 TL:..." que o G-code do fatiador manda para a USB.

typedef enum {
    IMP_PARADA,       // nada em andamento
    IMP_IMPRIMINDO,
    IMP_PAUSANDO,     // esperando o Marlin esvaziar para estacionar
    IMP_PAUSADA,
    IMP_CANCELANDO,
    IMP_CONCLUIDA,
    IMP_CANCELADA,
    IMP_ERRO,
} imp_estado_t;

typedef struct {
    imp_estado_t estado;
    char nome[84];
    uint32_t bytes, total;    // lidos / tamanho do arquivo
    uint32_t linhas;          // enviadas
    int camada;               // ;LAYER_CHANGE ja passados
    int camadas;              // total (dos dados do arquivo; 0 se nao sabe)
    int decorrido_s;          // sem contar pausas
    int estimado_s;           // do fatiador (-1 se nao sabe)
    int restante_s;           // -1 se nao da para estimar
    char aviso[96];
    // impressao pela TFT24 (linhas TL:)
    bool tft;
    char tft_nome[84];
    int tft_camada, tft_camadas, tft_decorrido_s;
} imp_info_t;

void imp_inicia(void);

// Comeca a imprimir um arquivo de /sd/gcodes. Recusa se ja ha impressao (da
// placa ou da TFT) ou se a TinyBee nao responde; o motivo vai em erro.
esp_err_t imp_imprime(const char *nome, char *erro, size_t tam);
void imp_pausa(void);
void imp_retoma(void);
void imp_cancela(void);
void imp_info(imp_info_t *saida);
bool imp_ativa(void);         // imprimindo, pausando, pausada ou cancelando

// Linhas do Marlin (TL:, erros): chamar para cada linha recebida
void imp_linha_marlin(marlin_tipo_t tipo, const char *linha);

// Chamada a cada troca de camada (impressao da placa ou TL:CAMADA da TFT)
typedef void (*imp_camada_cb_t)(int camada, int total);
// Chamada no pedido de foto do time-lapse (TL:FOTO)
typedef void (*imp_foto_cb_t)(void);
// Chamada no inicio (true) e no fim (false) de qualquer impressao
typedef void (*imp_ciclo_cb_t)(bool comecou, const char *nome);
void imp_ouvintes(imp_camada_cb_t camada, imp_foto_cb_t foto, imp_ciclo_cb_t ciclo);
