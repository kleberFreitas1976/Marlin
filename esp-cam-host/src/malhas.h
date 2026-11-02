#pragma once
#include "cartao_sd.h"
#include "marlin.h"

// Historico das malhas de nivelamento no cartao: cada "Bilinear Leveling Grid" que a
// TinyBee manda (fim do G29, M420 V) e cada falha de sondagem vao para
// /sd/malhas/malhas.txt, com data, arquivo em impressao e temperaturas.

#define MALHAS_ARQ SD_RAIZ "/malhas/malhas.txt"

void malhas_inicia(void);

// Chamado para cada linha da TinyBee, na tarefa do Marlin: so copia e enfileira
// (sem esperar), quem grava no cartao e uma tarefa de baixa prioridade.
void malhas_linha(marlin_tipo_t tipo, const char *linha);
