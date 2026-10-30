#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Conversa com o Marlin da TinyBee pela USB (usb_serial).
//
// Envio controlado pelo "ok": uma linha por vez, a proxima so depois do "ok" da
// anterior. Despejar comandos estoura o buffer de recepcao do Marlin e ele
// executa linhas truncadas. Comandos demorados (G28, G29, M109) mandam
// "busy: processing" a cada 2 s (HOST_KEEPALIVE_FEATURE), e isso renova a espera.

#define MARLIN_LINHA_MAX 96          // MAX_CMD_SIZE do Marlin
#define MARLIN_FILA_MAX  64
#define MARLIN_SILENCIO_S 30         // sem nenhuma resposta: destrava a fila

typedef enum {
    MARLIN_TX,       // comando enviado
    MARLIN_RX,       // resposta comum
    MARLIN_OK,       // "ok"
    MARLIN_ERRO,     // Error:, falhas, kill
    MARLIN_SIS,      // echo:, avisos do proprio ESP
    MARLIN_TEMP,     // relatorio automatico de temperatura (nao vai para o console)
    MARLIN_POS,      // relatorio automatico de posicao (idem)
} marlin_tipo_t;

typedef struct {
    bool conectada;           // USB aberta
    bool respondendo;         // ja respondeu desde que conectou
    char firmware[64];
    float bico, bico_alvo, mesa, mesa_alvo;
    int bico_pwm, mesa_pwm;   // 0-127 (@:)
    float x, y, z, e;
    int velocidade;           // M220 (%)
    int fluxo;                // M221 (%)
    int compensacao;          // M420: 1 ligada, 0 desligada, -1 desconhecido
    bool ocupada;             // recebendo "busy"
    int fila;                 // comandos esperando
    uint32_t linhas_tx, linhas_rx;
    // area da maquina (M115 com M115_GEOMETRY_REPORT)
    float area_min[3], area_max[3];
} marlin_estado_t;

// Recebe cada linha (enviada ou recebida), para o console web e o log.
typedef void (*marlin_ouvinte_t)(marlin_tipo_t tipo, const char *linha);

esp_err_t marlin_inicia(marlin_ouvinte_t ouvinte);

// Poe na fila. prioridade = passa na frente (comandos manuais durante uma impressao).
esp_err_t marlin_envia(const char *gcode, bool prioridade);

// Descarta a fila (nao interrompe o que o Marlin ja esta executando).
void marlin_limpa_fila(void);

// M112 imediato, furando a fila. O Marlin para tudo e precisa ser reiniciado.
void marlin_emergencia(void);

// Copia do estado atual (protegida por mutex)
void marlin_estado(marlin_estado_t *saida);
