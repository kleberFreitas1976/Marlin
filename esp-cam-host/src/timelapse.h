#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// Time-lapse: uma foto por camada durante a impressao e, no fim, um video AVI
// (MJPEG) montado no proprio cartao. Pasta: /sd/timelapse/<data>_<nome>/.
//
// Gatilho:
//   TL_FOTO   so com "M118 P1 TL:FOTO" (o G-code estaciona o bico e espera)
//   TL_CAMADA a cada troca de camada (sem estacionar: a cabeca aparece)
//   TL_AUTO   TL:FOTO se o arquivo usar; senao, a cada camada

#define TL_PASTA SD_RAIZ "/timelapse"

typedef enum { TL_AUTO = 0, TL_FOTO = 1, TL_CAMADA = 2 } tl_modo_t;

typedef struct {
    bool ativo;
    tl_modo_t modo;
    int resolucao;     // framesize_t do esp32-camera
    int fps;           // do video montado
} tl_config_t;

void tl_inicia(void);
void tl_config(tl_config_t *saida);
esp_err_t tl_grava_config(const tl_config_t *c);

// Estado da gravacao em andamento
bool tl_gravando(void);
void tl_status(char *pasta, size_t tam, int *fotos, bool *montando);

// Ouvintes ligados ao modulo impressao
void tl_camada(int camada, int total);
void tl_foto(void);
void tl_ciclo(bool comecou, const char *nome);

// Monta (ou remonta) o video de uma pasta, em segundo plano
esp_err_t tl_monta_video(const char *pasta);

// Lista em JSON (liberar com free)
char *tl_lista_json(void);
esp_err_t tl_apaga(const char *pasta);
bool tl_pasta_valida(const char *pasta);
