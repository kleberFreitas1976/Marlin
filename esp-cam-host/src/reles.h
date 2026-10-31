#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// Reles e iluminacao (ver ESPECIFICACAO.md, secao 11).
//
// Rele "impressora" (GPIO21): contato NF em serie com a fonte da impressora.
//   Rele solto = impressora ligada; energizado (e mantido) = desligada. Assim um
//   reinicio do ESP nao desliga a impressora no meio de uma impressao.
// Rele "tudo" (GPIO47): contato NF na retencao de um contator com botao de liga.
//   Um pulso abre a retencao e tudo desliga, inclusive o ESP, ate alguem apertar
//   o botao de liga.
//
// Os pinos ficam em alta impedancia enquanto o rele deve estar solto: a maioria
// dos modulos aciona com nivel BAIXO e tem resistor de pull-up na entrada, e um
// pino puxado para baixo durante o boot desligaria a impressora.

#define RELE_ESFRIAR_C 50        // so corta a impressora com o bico abaixo disto

typedef struct {
    bool impressora_desligada;
    bool aguardando_esfriar;     // pedido feito, esperando o bico esfriar
    bool tudo_agendado;          // "desligar tudo" esperando o bico esfriar
    bool ativo_baixo;            // polaridade do modulo de rele
    int luz;                     // 0-100 %
    bool luz_auto;               // acende durante a impressao (time-lapse)
    bool auto_desligar;          // desliga a impressora depois que uma impressao termina bem
    int auto_min;                // ... tantos minutos depois do fim
    int auto_falta_s;            // contagem em andamento; -1 se nao ha
} reles_estado_t;

void reles_inicia(void);
void reles_estado(reles_estado_t *e);

// forcar=true ignora a espera do bico e a impressao em andamento
esp_err_t reles_desliga_impressora(bool forcar, char *motivo, size_t tam);
esp_err_t reles_liga_impressora(void);
esp_err_t reles_desliga_tudo(bool forcar, char *motivo, size_t tam);
void reles_cancela_espera(void);

void reles_luz(int pct);
void reles_luz_auto(bool sim);
void reles_polaridade(bool ativo_baixo);

// Desligar a impressora sozinho minutos depois do fim de uma impressao concluida
// (a da placa ou a da TFT); depois ainda espera o bico esfriar.
void reles_auto(bool ativo, int minutos);

// Chamada no inicio/fim das impressoes (luz automatica e desligamento automatico).
// concluida: terminou normalmente (so vale no fim).
void reles_impressao(bool imprimindo, bool concluida);
