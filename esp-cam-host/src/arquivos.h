#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

// Arquivos G-code no cartao SD (/sd/gcodes) com dados lidos na chegada:
// miniatura (blocos "; thumbnail begin" do Orca/Prusa), tempo estimado,
// camadas, filamento e material. Os dados ficam em /sd/gcodes/.meta/<nome>.json
// e a miniatura em /sd/gcodes/.meta/<nome>.png (ou .jpg).

#define ARQ_PASTA SD_RAIZ "/gcodes"
#define ARQ_META  ARQ_PASTA "/.meta"
#define ARQ_NOME_MAX 80

// Confere e limpa um nome de arquivo (sem pastas, extensao .gcode/.gco/.g).
bool arq_nome_valido(const char *nome);

// Recebimento em partes (upload pela web ou pelo OrcaSlicer)
typedef struct arq_envio arq_envio_t;
arq_envio_t *arq_envio_abre(const char *nome);
esp_err_t arq_envio_dados(arq_envio_t *e, const uint8_t *dados, size_t n);
// ok=false descarta o arquivo (envio interrompido)
esp_err_t arq_envio_fecha(arq_envio_t *e, bool ok);

// Lista em JSON: [{"nome":..,"tam":..,"data":..,"meta":{...}},...]. Devolve o
// texto alocado (liberar com free) ou NULL.
char *arq_lista_json(void);

esp_err_t arq_apaga(const char *nome);

// Caminhos completos
void arq_caminho(const char *nome, char *saida, size_t tam);
// Miniatura existente (png ou jpg); devolve false se nao ha
bool arq_miniatura(const char *nome, char *saida, size_t tam, const char **tipo);

// Le o tempo estimado (s) gravado nos dados do arquivo; -1 se nao ha
int arq_tempo_estimado(const char *nome);
