#pragma once
#include "cartao_sd.h"
#include "marlin.h"

// Diario da impressora no cartao: os pontos principais, nao o G-code inteiro.
// Um arquivo por dia em /sd/logs/AAAA-MM-DD.txt (sem relogio: sem_data.txt).
//
// Registra: inicio e fim de impressao (com o motivo), pausa/retomada/cancelamento,
// aquecimentos pedidos e quando chegaram na temperatura, G28/G29, malha concluida,
// falhas de sondagem, cada camada com bico/mesa/potencia/ventoinha, mudancas da
// ventoinha, reinicio da TinyBee e os erros. Num erro, grava tambem as
// temperaturas dos ultimos ~80 s (o Thermal Runaway olha uma janela de 40 s).
//
// Nada aqui espera o cartao: as chamadas so formatam e enfileiram; uma tarefa de
// baixa prioridade grava em lotes, para nao disputar o SD com a leitura do G-code.

#define DIARIO_PASTA SD_RAIZ "/logs"

void diario_inicia(void);

// Texto livre (com data e hora de agora)
void diario_evento(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Cada linha trocada com a TinyBee (comandos manuais, respostas, temperaturas)
void diario_linha(marlin_tipo_t tipo, const char *linha);

// Cada linha de G-code enviada pela impressao da placa (sem comentario)
void diario_gcode(const char *gcode);
