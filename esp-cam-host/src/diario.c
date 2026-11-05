#include "diario.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "diario";

// Item da fila (PSRAM): hora de quando aconteceu + texto
typedef struct {
    time_t t;
    int64_t us;
    char txt[];
} item_t;

static QueueHandle_t s_fila;

// Estado acompanhado (so a tarefa do Marlin mexe: linhas da TinyBee e da impressao)
static int s_ventoinha = -1;               // ultimo M106/M107 visto (-1 = nao sabe)
static float s_bico_esp = -1, s_mesa_esp = -1;   // alvo esperando chegar (-1 = nada)
static int64_t s_bico_desde, s_mesa_desde;

// Temperaturas recentes (relatorio automatico a cada 2 s): 40 = ~80 s
#define HIST 40
typedef struct {
    int64_t us;
    float bico, bico_alvo, mesa, mesa_alvo;
    int bico_pwm, mesa_pwm, ventoinha;
} amostra_t;
static amostra_t s_hist[HIST];
static int s_hist_n, s_hist_i;
static int64_t s_ultimo_despejo;

// ------------------------------------------------------------------ fila
static void enfileira(const char *txt)
{
    if (!s_fila) return;
    size_t n = strlen(txt);
    item_t *it = heap_caps_malloc(sizeof(item_t) + n + 1, MALLOC_CAP_SPIRAM);
    if (!it) return;
    it->t = time(NULL);
    it->us = esp_timer_get_time();
    memcpy(it->txt, txt, n + 1);
    if (xQueueSend(s_fila, &it, 0) != pdTRUE) free(it);   // fila cheia: perde o registro, nao a impressao
}

void diario_evento(const char *fmt, ...)
{
    char l[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(l, sizeof l, fmt, ap);
    va_end(ap);
    enfileira(l);
}

// ------------------------------------------------------------------ gravacao
static bool tem_relogio(time_t t) { return t > 1700000000; }   // acertado pela rede

static void nome_arquivo(const item_t *it, char *s, size_t tam)
{
    if (tem_relogio(it->t)) {
        struct tm tm;
        localtime_r(&it->t, &tm);
        snprintf(s, tam, DIARIO_PASTA "/%04d-%02d-%02d.txt", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    } else {
        snprintf(s, tam, DIARIO_PASTA "/sem_data.txt");
    }
}

static void grava(FILE *f, const item_t *it)
{
    char quando[24];
    if (tem_relogio(it->t)) {
        struct tm tm;
        localtime_r(&it->t, &tm);
        strftime(quando, sizeof quando, "%H:%M:%S", &tm);
    } else {      // sem rede: tempo desde que a ESP ligou
        snprintf(quando, sizeof quando, "+%llus", (unsigned long long)(it->us / 1000000));
    }
    fprintf(f, "%s %s\n", quando, it->txt);
}

static void tarefa(void *arg)
{
    item_t *it;
    while (true) {
        if (xQueueReceive(s_fila, &it, portMAX_DELAY) != pdTRUE) continue;
        // Junta o que chegar no proximo segundo: um fopen por lote, nao por linha
        vTaskDelay(pdMS_TO_TICKS(1000));
        char atual[64] = "", nome[64];
        FILE *f = NULL;
        bool pasta = false;
        do {
            if (sd_ok()) {
                nome_arquivo(it, nome, sizeof nome);
                if (strcmp(nome, atual)) {
                    if (f) fclose(f);
                    if (!pasta) {
                        mkdir(DIARIO_PASTA, 0775);
                        pasta = true;
                    }
                    f = fopen(nome, "a");
                    strlcpy(atual, nome, sizeof atual);
                    if (!f) ESP_LOGW(TAG, "nao abriu %s", nome);
                }
                if (f) grava(f, it);
            }
            free(it);
        } while (xQueueReceive(s_fila, &it, 0) == pdTRUE);
        if (f) fclose(f);
    }
}

void diario_inicia(void)
{
    s_fila = xQueueCreateWithCaps(64, sizeof(item_t *), MALLOC_CAP_SPIRAM);
    if (!s_fila) {
        ESP_LOGE(TAG, "sem memoria");
        return;
    }
    xTaskCreate(tarefa, "diario", 4096, NULL, 2, NULL);
    diario_evento("--- ESP32-S3-CAM ligada");
}

// ------------------------------------------------------------------ G-code
// Numero depois da letra (S, P...), ou def
static float valor(const char *g, char letra, float def)
{
    for (const char *p = g; *p; p++)
        if (*p == letra && (p == g || p[-1] == ' ')) return strtof(p + 1, NULL);
    return def;
}

static bool comando(const char *g, const char *cmd)
{
    size_t n = strlen(cmd);
    return !strncmp(g, cmd, n) && (g[n] == '\0' || g[n] == ' ');
}

static void espera_bico(float alvo)
{
    s_bico_esp = alvo > 0 ? alvo : -1;
    s_bico_desde = esp_timer_get_time();
}

static void espera_mesa(float alvo)
{
    s_mesa_esp = alvo > 0 ? alvo : -1;
    s_mesa_desde = esp_timer_get_time();
}

void diario_gcode(const char *g)
{
    if (g[0] != 'M' && g[0] != 'G') return;      // a maioria das linhas sai aqui (G0/G1 abaixo)
    if (g[0] == 'G' && (g[1] == '0' || g[1] == '1') && (g[2] == ' ' || g[2] == '\0')) return;
    if (comando(g, "M104") || comando(g, "M109")) {
        float s = valor(g, 'S', 0);
        bool espera = g[3] == '9';
        if (s > 0) diario_evento("bico: aquecer a %.0f%s", s, espera ? " (esperando)" : "");
        else diario_evento("bico: desligado");
        espera_bico(s);
    } else if (comando(g, "M140") || comando(g, "M190")) {
        float s = valor(g, 'S', 0);
        bool espera = g[3] == '9';
        if (s > 0) diario_evento("mesa: aquecer a %.0f%s", s, espera ? " (esperando)" : "");
        else diario_evento("mesa: desligada");
        espera_mesa(s);
    } else if (comando(g, "M106") || comando(g, "M107")) {
        int v = g[3] == '7' ? 0 : (int)valor(g, 'S', 255);
        if (v != s_ventoinha) diario_evento("ventoinha de peca: %d%%", (v * 100 + 127) / 255);
        s_ventoinha = v;
    } else if (comando(g, "G28")) {
        diario_evento("home (G28)");
    } else if (comando(g, "G29")) {
        diario_evento("malha: comecou (G29)");
    } else if (comando(g, "M303")) {
        diario_evento("PID: autoajuste pedido (%s)", g);
    } else if (comando(g, "M301") || comando(g, "M500") || comando(g, "M600") ||
               comando(g, "M0") || comando(g, "M1") || comando(g, "M112") || comando(g, "M999")) {
        diario_evento("comando: %s", g);
    }
}

// ------------------------------------------------------------------ linhas da TinyBee
static void guarda_amostra(const marlin_estado_t *e)
{
    amostra_t *a = &s_hist[s_hist_i];
    a->us = esp_timer_get_time();
    a->bico = e->bico;
    a->bico_alvo = e->bico_alvo;
    a->mesa = e->mesa;
    a->mesa_alvo = e->mesa_alvo;
    a->bico_pwm = e->bico_pwm;
    a->mesa_pwm = e->mesa_pwm;
    a->ventoinha = s_ventoinha;
    s_hist_i = (s_hist_i + 1) % HIST;
    if (s_hist_n < HIST) s_hist_n++;
}

// As temperaturas que levaram ao erro (potencia 0-127: 127 = aquecedor no maximo)
static void despeja_historico(void)
{
    int64_t agora = esp_timer_get_time();
    if (agora - s_ultimo_despejo < 10000000) return;     // erro em cascata: uma vez so
    s_ultimo_despejo = agora;
    diario_evento("    temperaturas antes do erro (s atras: bico/alvo potencia | mesa/alvo potencia | ventoinha):");
    for (int k = 0; k < s_hist_n; k++) {
        const amostra_t *a = &s_hist[(s_hist_i - s_hist_n + k + HIST) % HIST];
        char v[8] = "?";
        if (a->ventoinha >= 0) snprintf(v, sizeof v, "%d%%", (a->ventoinha * 100 + 127) / 255);
        diario_evento("    -%3llds  %6.1f/%.0f @%3d | %5.1f/%.0f @%3d | %s",
                      (long long)((agora - a->us) / 1000000), a->bico, a->bico_alvo, a->bico_pwm,
                      a->mesa, a->mesa_alvo, a->mesa_pwm, v);
    }
}

static bool erro_de_protocolo(const char *l)
{
    static const char *const P[] = {"Error:Line Number", "Error:checksum mismatch", "Error:No Checksum",
                                    "Error:No Line Number", "Error:Serial status mismatch"};
    for (size_t i = 0; i < sizeof P / sizeof P[0]; i++)
        if (!strncmp(l, P[i], strlen(P[i]))) return true;
    return false;
}

static void temperaturas(void)
{
    marlin_estado_t e;
    marlin_estado(&e);
    guarda_amostra(&e);
    int64_t agora = esp_timer_get_time();
    if (s_bico_esp > 0 && fabsf(e.bico - s_bico_esp) <= 1.0f) {
        diario_evento("bico: chegou a %.1f (alvo %.0f) em %llds", e.bico, s_bico_esp,
                      (long long)((agora - s_bico_desde) / 1000000));
        s_bico_esp = -1;
    }
    if (s_mesa_esp > 0 && fabsf(e.mesa - s_mesa_esp) <= 1.0f) {
        diario_evento("mesa: chegou a %.1f (alvo %.0f) em %llds", e.mesa, s_mesa_esp,
                      (long long)((agora - s_mesa_desde) / 1000000));
        s_mesa_esp = -1;
    }
}

static void camada(const char *c)
{
    marlin_estado_t e;
    marlin_estado(&e);
    char v[8] = "?";
    if (s_ventoinha >= 0) snprintf(v, sizeof v, "%d%%", (s_ventoinha * 100 + 127) / 255);
    diario_evento("camada %s | bico %.1f/%.0f @%d | mesa %.1f/%.0f @%d | ventoinha %s | Z%.2f",
                  c, e.bico, e.bico_alvo, e.bico_pwm, e.mesa, e.mesa_alvo, e.mesa_pwm, v, e.z);
}

void diario_linha(marlin_tipo_t tipo, const char *l)
{
    if (!s_fila) return;
    switch (tipo) {
    case MARLIN_TX:               // comandos manuais (console, pagina)
        diario_gcode(l);
        return;
    case MARLIN_TEMP:
        temperaturas();
        return;
    case MARLIN_OK:
        if (strstr(l, "T:")) temperaturas();       // "ok T:..." do M105
        return;
    case MARLIN_POS:
        return;
    case MARLIN_ERRO:
        if (erro_de_protocolo(l) || !strncmp(l, "Resend:", 7) || !strncmp(l, "rs ", 3))
            return;                                 // o reenvio corrige sozinho
        if (strstr(l, "Probing Failed")) {
            marlin_estado_t e;
            marlin_estado(&e);
            diario_evento("malha: falha de sondagem perto de X%.1f Y%.1f", e.x, e.y);
            return;
        }
        diario_evento("ERRO: %s", l);
        despeja_historico();
        return;
    default:
        break;
    }
    if (!strncmp(l, "TL:", 3)) {
        const char *c = l + 3;
        if (!strncmp(c, "INICIO", 6)) {
            s_ventoinha = -1;
            diario_evento("=== impressao: %s", c[6] == ' ' ? c + 7 : "(sem nome)");
        } else if (!strncmp(c, "CAMADA ", 7)) {
            camada(c + 7);
        } else if (!strncmp(c, "FIM", 3)) {
            diario_evento("=== fim do G-code");
        }
        return;
    }
    if (!strcmp(l, "start")) {
        s_ventoinha = -1;
        s_bico_esp = s_mesa_esp = -1;
        diario_evento("--- TinyBee reiniciou");
    } else if (strstr(l, "Bilinear Leveling Grid")) {
        diario_evento("malha: concluida (grade em /malhas/malhas.txt)");
    } else if (strstr(l, "PID Autotune") || strstr(l, "Kp:") || strstr(l, "DEFAULT_K") ||
               strstr(l, "Thermal Runaway") || strstr(l, "Heating failed") ||
               strstr(l, "MAXTEMP") || strstr(l, "MINTEMP")) {
        diario_evento("TinyBee: %s", l);
    }
}
