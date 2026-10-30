#include "marlin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "usb_serial.h"

static const char *TAG = "marlin";

typedef struct {
    char s[MARLIN_LINHA_MAX + 1];
} linha_t;

static marlin_ouvinte_t s_ouvinte;
static marlin_estado_t s_e;
static SemaphoreHandle_t s_mtx;
static StreamBufferHandle_t s_rx;
static QueueHandle_t s_fila;
static volatile bool s_conectou, s_desconectou;
static bool s_aguardando;
static char s_ultimo_cmd[MARLIN_LINHA_MAX + 1];   // ultimo comando enviado
static int64_t s_ultimo_rx;

// Configuracao pedida a cada conexao e a cada reinicio do Marlin. Os relatorios
// automaticos so vao para a porta que os pediu: a TFT24 nao recebe nada disto.
static const char *INICIO[] = {"M115", "M155 S2", "M154 S2", "M420", "M220", "M221"};

static void avisa(marlin_tipo_t tipo, const char *linha)
{
    if (s_ouvinte) s_ouvinte(tipo, linha);
}

// ------------------------------------------------------------------ leitura
static bool num_apos(const char *linha, const char *chave, float *v)
{
    const char *p = strstr(linha, chave);
    if (!p) return false;
    char *fim;
    float f = strtof(p + strlen(chave), &fim);
    if (fim == p + strlen(chave)) return false;
    *v = f;
    return true;
}

// "T:26.25 /0.00 B:25.23 /0.00 @:0 B@:0" (sozinha ou depois de "ok")
static bool le_temperatura(const char *l)
{
    const char *t = strstr(l, "T:");
    if (!t || !strchr(t, '/')) return false;
    float v;
    if (num_apos(t, "T:", &v)) s_e.bico = v;
    const char *b = strchr(t, '/');
    if (b) s_e.bico_alvo = strtof(b + 1, NULL);
    const char *m = strstr(t, "B:");
    if (m) {
        s_e.mesa = strtof(m + 2, NULL);
        const char *mb = strchr(m, '/');
        if (mb) s_e.mesa_alvo = strtof(mb + 1, NULL);
    }
    const char *at = strstr(t, " @:");
    if (at) s_e.bico_pwm = atoi(at + 3);
    const char *bat = strstr(t, "B@:");
    if (bat) s_e.mesa_pwm = atoi(bat + 3);
    return true;
}

// "X:196.00 Y:-15.00 Z:0.00 E:0.00 Count A:15680B:-1200 Z:0"
static bool le_posicao(const char *l)
{
    if (strncmp(l, "X:", 2) || !strstr(l, " Y:") || !strstr(l, " E:")) return false;
    char tmp[MARLIN_LINHA_MAX * 2];
    strlcpy(tmp, l, sizeof tmp);
    char *c = strstr(tmp, "Count");
    if (c) *c = '\0';        // depois de "Count" vem a posicao em passos
    float v;
    if (num_apos(tmp, "X:", &v)) s_e.x = v;
    if (num_apos(tmp, "Y:", &v)) s_e.y = v;
    if (num_apos(tmp, "Z:", &v)) s_e.z = v;
    if (num_apos(tmp, "E:", &v)) s_e.e = v;
    return true;
}

// "area:{full:{min:{x:0.00,y:-15.00,z:0.00},max:{x:196.00,y:181.00,z:250.00}},..."
static void le_area(const char *l)
{
    const char *mn = strstr(l, "min:{"), *mx = strstr(l, "max:{");
    if (!mn || !mx) return;
    float v;
    if (num_apos(mn, "x:", &v)) s_e.area_min[0] = v;
    if (num_apos(mn, "y:", &v)) s_e.area_min[1] = v;
    if (num_apos(mn, "z:", &v)) s_e.area_min[2] = v;
    if (num_apos(mx, "x:", &v)) s_e.area_max[0] = v;
    if (num_apos(mx, "y:", &v)) s_e.area_max[1] = v;
    if (num_apos(mx, "z:", &v)) s_e.area_max[2] = v;
}

static void pede_inicio(void)
{
    for (size_t i = 0; i < sizeof INICIO / sizeof INICIO[0]; i++) marlin_envia(INICIO[i], false);
}

static void processa(char *l)
{
    // tira espacos do fim
    size_t n = strlen(l);
    while (n && (l[n - 1] == ' ' || l[n - 1] == '\t')) l[--n] = '\0';
    if (!n) return;
    s_ultimo_rx = esp_timer_get_time();

    marlin_tipo_t tipo = MARLIN_RX;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_e.respondendo = true;
    s_e.linhas_rx++;

    if (!strncmp(l, "ok", 2)) {
        s_aguardando = false;
        s_e.ocupada = false;
        tipo = MARLIN_OK;
        le_temperatura(l);                 // "ok T:..." do M105
    } else if (!strncmp(l, "echo:busy", 9)) {
        s_e.ocupada = true;
        tipo = MARLIN_SIS;
    } else if (le_temperatura(l) && (l[0] == 'T' || l[0] == ' ')) {
        tipo = MARLIN_TEMP;
    } else if (le_posicao(l)) {
        // Resposta a um M114 pedido a mao aparece no console; o relatorio automatico, nao
        tipo = (s_aguardando && !strncmp(s_ultimo_cmd, "M114", 4)) ? MARLIN_RX : MARLIN_POS;
    } else if (!strncmp(l, "Error", 5) || strstr(l, "kill") || strstr(l, "Printer halted") ||
               strstr(l, "Probing Failed")) {
        tipo = MARLIN_ERRO;
    } else if (!strncmp(l, "echo:", 5) || !strncmp(l, "//", 2)) {
        tipo = MARLIN_SIS;
    }

    if (!strncmp(l, "FIRMWARE_NAME:", 14)) {
        const char *f = l + 14;
        const char *fim = strstr(f, " SOURCE_CODE_URL");
        size_t k = fim ? (size_t)(fim - f) : strlen(f);
        if (k >= sizeof s_e.firmware) k = sizeof s_e.firmware - 1;
        memcpy(s_e.firmware, f, k);
        s_e.firmware[k] = '\0';
    }
    if (!strncmp(l, "area:", 5)) le_area(l);
    const char *p;
    if ((p = strstr(l, "FR:")) != NULL) s_e.velocidade = atoi(p + 3);
    if ((p = strstr(l, "Flow:")) != NULL) s_e.fluxo = atoi(p + 5);
    if (strstr(l, "Bed Leveling ON")) s_e.compensacao = 1;
    if (strstr(l, "Bed Leveling OFF")) s_e.compensacao = 0;
    bool reiniciou = !strcmp(l, "start");
    if (reiniciou) s_aguardando = false;
    xSemaphoreGive(s_mtx);

    avisa(tipo, l);
    if (reiniciou) {
        // O Marlin reiniciou (reset, M999, kill): os relatorios automaticos se perderam
        avisa(MARLIN_SIS, "A TinyBee reiniciou: pedindo os relatorios de novo");
        pede_inicio();
    }
}

// ------------------------------------------------------------------ tarefa
static void tarefa(void *arg)
{
    char linha[256];
    size_t n = 0;
    uint8_t buf[128];
    while (true) {
        if (s_conectou) {
            s_conectou = false;
            xSemaphoreTake(s_mtx, portMAX_DELAY);
            s_e.conectada = true;
            s_e.respondendo = false;
            s_aguardando = false;
            xSemaphoreGive(s_mtx);
            n = 0;
            avisa(MARLIN_SIS, "TinyBee conectada pela USB");
            pede_inicio();
        }
        if (s_desconectou) {
            s_desconectou = false;
            xSemaphoreTake(s_mtx, portMAX_DELAY);
            s_e.conectada = s_e.respondendo = false;
            s_aguardando = false;
            xSemaphoreGive(s_mtx);
            xQueueReset(s_fila);
            avisa(MARLIN_ERRO, "TinyBee desconectada da USB");
        }

        size_t r = xStreamBufferReceive(s_rx, buf, sizeof buf, pdMS_TO_TICKS(20));
        for (size_t i = 0; i < r; i++) {
            char c = (char)buf[i];
            if (c == '\n' || c == '\r') {
                if (n) {
                    linha[n] = '\0';
                    processa(linha);
                    n = 0;
                }
            } else if (n < sizeof linha - 1) {
                linha[n++] = c;
            }
        }

        // Nada chegou ha tempo demais: a fila nao pode ficar presa para sempre
        if (s_aguardando && esp_timer_get_time() - s_ultimo_rx > MARLIN_SILENCIO_S * 1000000LL) {
            s_aguardando = false;
            ESP_LOGW(TAG, "sem resposta ha %d s: destravando a fila", MARLIN_SILENCIO_S);
            avisa(MARLIN_ERRO, "Sem resposta da TinyBee ha 30 s: destravando a fila");
        }

        linha_t cmd;
        if (!s_aguardando && s_e.conectada && xQueueReceive(s_fila, &cmd, 0) == pdTRUE) {
            char saida[MARLIN_LINHA_MAX + 2];
            int k = snprintf(saida, sizeof saida, "%s\n", cmd.s);
            if (usb_serial_envia(saida, k) == ESP_OK) {
                s_aguardando = true;
                s_ultimo_rx = esp_timer_get_time();
                strlcpy(s_ultimo_cmd, cmd.s, sizeof s_ultimo_cmd);
                xSemaphoreTake(s_mtx, portMAX_DELAY);
                s_e.linhas_tx++;
                xSemaphoreGive(s_mtx);
                avisa(MARLIN_TX, cmd.s);
            } else {
                avisa(MARLIN_ERRO, "Falha ao escrever na USB");
            }
        }
    }
}

static void usb_rx(const uint8_t *d, size_t n)
{
    xStreamBufferSend(s_rx, d, n, 0);      // na tarefa do driver USB: nao bloquear
}

static void usb_estado(bool conectado)
{
    if (conectado) s_conectou = true;
    else s_desconectou = true;
}

// ------------------------------------------------------------------ API
esp_err_t marlin_inicia(marlin_ouvinte_t ouvinte)
{
    s_ouvinte = ouvinte;
    s_mtx = xSemaphoreCreateMutex();
    s_rx = xStreamBufferCreate(8192, 1);
    s_fila = xQueueCreate(MARLIN_FILA_MAX, sizeof(linha_t));
    s_e.compensacao = -1;
    s_e.velocidade = s_e.fluxo = 100;
    xTaskCreatePinnedToCore(tarefa, "marlin", 6144, NULL, 6, NULL, 1);
    return usb_serial_inicia(usb_rx, usb_estado);
}

esp_err_t marlin_envia(const char *gcode, bool prioridade)
{
    // so a primeira linha, sem comentario, sem espacos nas pontas
    linha_t l;
    size_t k = 0;
    while (*gcode == ' ' || *gcode == '\t') gcode++;
    for (; *gcode && *gcode != '\n' && *gcode != '\r' && *gcode != ';' && k < MARLIN_LINHA_MAX;
         gcode++)
        l.s[k++] = *gcode;
    while (k && l.s[k - 1] == ' ') k--;
    l.s[k] = '\0';
    if (!k) return ESP_ERR_INVALID_ARG;
    BaseType_t ok = prioridade ? xQueueSendToFront(s_fila, &l, 0) : xQueueSend(s_fila, &l, 0);
    return ok == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM;
}

void marlin_limpa_fila(void)
{
    UBaseType_t n = uxQueueMessagesWaiting(s_fila);
    xQueueReset(s_fila);
    char msg[64];
    snprintf(msg, sizeof msg, "Fila limpa (%u comandos descartados)", (unsigned)n);
    avisa(MARLIN_SIS, msg);
}

void marlin_emergencia(void)
{
    xQueueReset(s_fila);
    usb_serial_envia("M112\n", 5);
    s_aguardando = false;
    avisa(MARLIN_ERRO, "M112: PARADA DE EMERGENCIA. A TinyBee precisa ser reiniciada.");
}

void marlin_estado(marlin_estado_t *saida)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    *saida = s_e;
    saida->fila = (int)uxQueueMessagesWaiting(s_fila) + (s_aguardando ? 1 : 0);
    xSemaphoreGive(s_mtx);
}
