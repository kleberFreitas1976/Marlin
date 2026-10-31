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
// Janela de envio: ate MARLIN_JANELA linhas em transito, sem passar de
// MARLIN_JANELA_BYTES (o buffer de recepcao da UART do Marlin no ESP32 e de 256
// bytes). Cada "ok" libera a linha mais antiga. Com uma linha por vez, trechos
// cheios de segmentos curtos (curvas) esvaziam o planejador e a peca sai marcada.
static int s_voo;                                  // linhas enviadas sem "ok"
static int s_voo_bytes;
static uint8_t s_voo_tam[MARLIN_JANELA];          // tamanho de cada uma, em ordem
static bool s_aguardando;                          // s_voo > 0
static char s_ultimo_cmd[MARLIN_LINHA_MAX + 1];   // ultimo comando enviado
static int64_t s_ultimo_rx;
// Ultimo sinal de que a janela anda: um "ok", um "busy" ou o envio com a janela vazia.
// O relatorio de temperatura chega a cada 2 s e nao serve para isso.
static int64_t s_ultimo_avanco;
static marlin_fonte_t s_fonte;                     // impressao em andamento (print_job)
static linha_t s_pendente;                         // proxima linha que nao coube na janela
static bool s_tem_pendente;

// Numero de linha e checksum (N123 G1 X10*45), como o OctoPrint: o Marlin recusa
// uma linha corrompida no caminho e pede de novo ("Resend: N"). Ja apareceu um
// "Serial status mismatch" do lado da TinyBee: um caractere pode se perder.
#define HIST 64
typedef struct {
    uint32_t n;
    char s[MARLIN_LINHA_MAX + 1];
} enviada_t;
static enviada_t s_hist[HIST];
static uint32_t s_n_prox = 1;       // proximo numero de linha
static uint32_t s_reenvio;          // proxima a reenviar (0 = nenhuma)
static uint32_t s_resend_k;         // ultimo "Resend" atendido
static int s_resend_ignorar;        // pedidos repetidos ainda a chegar (linhas que ja estavam a caminho)
static bool s_corromper;            // teste: a proxima linha vai com checksum errado

static void zera_janela(void)
{
    s_voo = s_voo_bytes = 0;
    s_aguardando = false;
    s_reenvio = 0;
    s_resend_ignorar = 0;
}

static void zera_numeracao(void)
{
    s_n_prox = 1;
    s_reenvio = 0;
    s_resend_ignorar = 0;
    memset(s_hist, 0, sizeof s_hist);
}

// Configuracao pedida a cada conexao e a cada reinicio do Marlin. Os relatorios
// automaticos so vao para a porta que os pediu: a TFT24 nao recebe nada disto.
static const char *INICIO[] = {"M110 N0", "M115", "M155 S2", "M154 S2", "M420", "M220", "M221"};

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
        s_e.oks++;
        s_ultimo_avanco = esp_timer_get_time();
        if (s_voo > 0) {
            s_voo_bytes -= s_voo_tam[0];
            memmove(s_voo_tam, s_voo_tam + 1, (MARLIN_JANELA - 1) * sizeof s_voo_tam[0]);
            s_voo--;
        }
        s_aguardando = s_voo > 0;
        s_e.ocupada = false;
        tipo = MARLIN_OK;
        le_temperatura(l);                 // "ok T:..." do M105
    } else if (!strncmp(l, "Resend:", 7) || !strncmp(l, "rs ", 3)) {
        uint32_t k = (uint32_t)strtoul(l + (l[0] == 'R' ? 7 : 3), NULL, 10);
        // Cada linha que ja estava a caminho depois da ruim gera o mesmo pedido:
        // atende o primeiro e ignora os repetidos.
        if (k == s_resend_k && s_resend_ignorar > 0) {
            s_resend_ignorar--;
        } else if (k > 0 && k < s_n_prox) {
            s_resend_k = k;
            s_resend_ignorar = s_voo > 0 ? s_voo - 1 : 0;
            s_reenvio = k;
            s_e.reenvios++;
        }
        tipo = MARLIN_ERRO;
    } else if (!strncmp(l, "echo:busy", 9)) {
        s_e.ocupada = true;
        s_ultimo_avanco = esp_timer_get_time();
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
    if (reiniciou) {
        zera_janela();
        zera_numeracao();          // o Marlin reiniciou: a numeracao dele voltou a zero
    }
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
            zera_janela();
            xSemaphoreGive(s_mtx);
            n = 0;
            avisa(MARLIN_SIS, "TinyBee conectada pela USB");
            // O CH340 acabou de receber a velocidade (250000): a primeira linha enviada
            // logo em seguida chegava como lixo, sem fim de linha, e o Marlin nao
            // respondia "ok" (o contador mostrava 7 enviadas e 6 oks). Espera, manda
            // uma linha em branco para descartar o lixo e so entao comeca.
            vTaskDelay(pdMS_TO_TICKS(500));
            usb_serial_envia("\n", 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            xStreamBufferReset(s_rx);
            pede_inicio();
        }
        if (s_desconectou) {
            s_desconectou = false;
            xSemaphoreTake(s_mtx, portMAX_DELAY);
            s_e.conectada = s_e.respondendo = false;
            zera_janela();
            s_tem_pendente = false;
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

        // "ok" perdido: a linha mais antiga esta ha MARLIN_OK_PERDIDO_S sem "ok" e o Marlin
        // nao disse "busy" nesse tempo. Libera a vaga, senao a janela fica com uma linha
        // fantasma e a pausa e o cancelamento esperam o Marlin "ociosar" para sempre.
        if (s_voo > 0 && esp_timer_get_time() - s_ultimo_avanco > MARLIN_OK_PERDIDO_S * 1000000LL) {
            s_voo_bytes -= s_voo_tam[0];
            memmove(s_voo_tam, s_voo_tam + 1, (MARLIN_JANELA - 1) * sizeof s_voo_tam[0]);
            s_voo--;
            s_aguardando = s_voo > 0;
            s_ultimo_avanco = esp_timer_get_time();
            s_e.oks_perdidos++;
            ESP_LOGW(TAG, "ok perdido: liberando uma vaga da janela (enviadas %lu, oks %lu)",
                     (unsigned long)s_e.linhas_tx, (unsigned long)s_e.oks);
            avisa(MARLIN_SIS, "Um \"ok\" do Marlin nao chegou: liberando a fila");
        }

        // Nada chegou ha tempo demais: a fila nao pode ficar presa para sempre
        if (s_aguardando && esp_timer_get_time() - s_ultimo_rx > MARLIN_SILENCIO_S * 1000000LL) {
            zera_janela();
            ESP_LOGW(TAG, "sem resposta ha %d s: destravando a fila", MARLIN_SILENCIO_S);
            avisa(MARLIN_ERRO, "Sem resposta da TinyBee ha 30 s: destravando a fila");
        }

        // Envia enquanto houver espaco na janela. Comandos manuais (fila) passam
        // na frente das linhas da impressao (fonte).
        while (s_e.conectada && s_voo < MARLIN_JANELA) {
            linha_t cmd;
            bool da_fonte = false;
            uint32_t num = 0;
            if (s_reenvio && s_reenvio < s_n_prox && s_n_prox - s_reenvio <= HIST &&
                s_hist[s_reenvio % HIST].n == s_reenvio) {
                // reenvio pedido pelo Marlin: vem do historico, com o mesmo numero
                num = s_reenvio;
                strlcpy(cmd.s, s_hist[num % HIST].s, sizeof cmd.s);
                da_fonte = true;               // nao repete no console
            } else if (s_reenvio) {
                s_reenvio = 0;                 // fora do historico: segue sem reenviar
                continue;
            } else if (s_tem_pendente) {
                cmd = s_pendente;
                da_fonte = true;
            } else if (xQueueReceive(s_fila, &cmd, 0) == pdTRUE) {
            } else if (s_fonte && s_fonte(cmd.s, sizeof cmd.s)) {
                da_fonte = true;
            } else {
                break;
            }
            // M110 (zera a numeracao) e M112 vao sem numero
            bool cru = !strncmp(cmd.s, "M110", 4) || !strncmp(cmd.s, "M112", 4);
            char saida[MARLIN_LINHA_MAX + 24];
            int k;
            if (cru) {
                k = snprintf(saida, sizeof saida, "%s\n", cmd.s);
            } else {
                uint32_t n_linha = num ? num : s_n_prox;
                int p = snprintf(saida, sizeof saida, "N%lu %s", (unsigned long)n_linha, cmd.s);
                uint8_t cs = 0;
                for (int i = 0; i < p; i++) cs ^= (uint8_t)saida[i];
                if (s_corromper && !num) {
                    cs ^= 0x55;
                    s_corromper = false;
                }
                k = p + snprintf(saida + p, sizeof saida - p, "*%u\n", cs);
            }
            if (s_voo > 0 && s_voo_bytes + k > MARLIN_JANELA_BYTES) {
                if (num) break;               // reenvio: tenta de novo apos o proximo ok
                if (da_fonte) {               // guarda para depois do proximo ok
                    s_pendente = cmd;
                    s_tem_pendente = true;
                } else {
                    xQueueSendToFront(s_fila, &cmd, 0);
                }
                break;
            }
            if (num) {
                s_reenvio = num + 1 < s_n_prox ? num + 1 : 0;
            } else {
                s_tem_pendente = false;
                if (cru) {
                    if (!strncmp(cmd.s, "M110", 4)) zera_numeracao();
                } else {
                    s_hist[s_n_prox % HIST].n = s_n_prox;
                    strlcpy(s_hist[s_n_prox % HIST].s, cmd.s, sizeof s_hist[0].s);
                    s_n_prox++;
                }
            }
            if (usb_serial_envia(saida, k) != ESP_OK) {
                avisa(MARLIN_ERRO, "Falha ao escrever na USB");
                break;
            }
            if (s_voo == 0) s_ultimo_avanco = esp_timer_get_time();
            s_voo_tam[s_voo++] = (uint8_t)k;
            s_voo_bytes += k;
            s_aguardando = true;
            s_ultimo_rx = esp_timer_get_time();
            strlcpy(s_ultimo_cmd, cmd.s, sizeof s_ultimo_cmd);
            xSemaphoreTake(s_mtx, portMAX_DELAY);
            s_e.linhas_tx++;
            xSemaphoreGive(s_mtx);
            if (!da_fonte) avisa(MARLIN_TX, cmd.s);   // as da impressao nao inundam o console
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
    s_fonte = NULL;
    s_tem_pendente = false;
    xQueueReset(s_fila);
    usb_serial_envia("M112\n", 5);
    zera_janela();
    avisa(MARLIN_ERRO, "M112: PARADA DE EMERGENCIA. A TinyBee precisa ser reiniciada.");
}

void marlin_estado(marlin_estado_t *saida)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    *saida = s_e;
    saida->fila = (int)uxQueueMessagesWaiting(s_fila) + s_voo;
    xSemaphoreGive(s_mtx);
}

void marlin_fonte(marlin_fonte_t fonte)
{
    s_fonte = fonte;
    if (!fonte) s_tem_pendente = false;
}

void marlin_teste_corrompe(void) { s_corromper = true; }

bool marlin_ocioso(void)
{
    return s_voo == 0 && !s_tem_pendente && uxQueueMessagesWaiting(s_fila) == 0;
}
