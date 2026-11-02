#include "malhas.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "cartao_sd.h"
#include "impressao.h"

static const char *TAG = "malhas";

// Item da fila: texto na PSRAM, prefixo 'M' (malha) ou 'F' (falha)
static QueueHandle_t s_fila;

// Malha sendo recebida (so a tarefa do Marlin mexe)
#define MALHA_MAX 1024
static char *s_buf;
static size_t s_len;
static bool s_coletando;

static void enfileira(char tipo, const char *txt, size_t n)
{
    if (!s_fila) return;
    char *c = heap_caps_malloc(n + 2, MALLOC_CAP_SPIRAM);
    if (!c) return;
    c[0] = tipo;
    memcpy(c + 1, txt, n);
    c[n + 1] = '\0';
    if (xQueueSend(s_fila, &c, 0) != pdTRUE) free(c);   // fila cheia: perde o registro, nao a impressao
}

// Linhas da grade: o cabecalho ("      0      1 ...") e as linhas (" 0 +0.138 +0.418 ...")
static bool linha_da_grade(const char *l)
{
    while (*l == ' ') l++;
    if (*l < '0' || *l > '9') return false;
    return !strchr(l, ':');
}

void malhas_linha(marlin_tipo_t tipo, const char *l)
{
    if (!s_buf || tipo == MARLIN_TX || tipo == MARLIN_TEMP || tipo == MARLIN_OK) return;
    if (strstr(l, "Bilinear Leveling Grid")) {
        s_coletando = true;
        s_len = 0;
        return;
    }
    if (s_coletando) {
        if (linha_da_grade(l)) {
            size_t n = strlen(l);
            if (s_len + n + 2 < MALHA_MAX) {
                memcpy(s_buf + s_len, l, n);
                s_len += n;
                s_buf[s_len++] = '\n';
            }
            return;
        }
        s_coletando = false;
        if (s_len) enfileira('M', s_buf, s_len);
    }
    if (strstr(l, "Probing Failed"))      // o "G29 Retrying" que vem junto seria repetido
        enfileira('F', l, strlen(l));
}

static void grava(const char *item)
{
    if (!sd_ok()) return;
    mkdir(SD_RAIZ "/malhas", 0775);
    FILE *f = fopen(MALHAS_ARQ, "a");
    if (!f) {
        ESP_LOGW(TAG, "nao abriu %s", MALHAS_ARQ);
        return;
    }
    char quando[24] = "sem hora";
    time_t t = time(NULL);
    if (t > 1700000000) {      // relogio acertado pela rede
        struct tm tm;
        localtime_r(&t, &tm);
        strftime(quando, sizeof quando, "%Y-%m-%d %H:%M:%S", &tm);
    }
    marlin_estado_t e;
    marlin_estado(&e);
    imp_info_t i;
    imp_info(&i);
    const char *arq = i.estado != IMP_PARADA ? i.nome : i.tft ? i.tft_nome : "-";
    if (item[0] == 'M')
        fprintf(f, "=== %s | malha | arquivo %s | mesa %.1f/%.0f bico %.1f/%.0f\n%s\n",
                quando, arq, e.mesa, e.mesa_alvo, e.bico, e.bico_alvo, item + 1);
    else      // a posicao vem do relatorio automatico (a cada 2 s): perto de onde falhou
        fprintf(f, "--- %s | %s | bico em X%.2f Y%.2f Z%.2f | arquivo %s | mesa %.1f/%.0f\n\n",
                quando, item + 1, e.x, e.y, e.z, arq, e.mesa, e.mesa_alvo);
    fclose(f);
    ESP_LOGI(TAG, "%s gravada em %s", item[0] == 'M' ? "malha" : "falha", MALHAS_ARQ);
}

static void tarefa(void *arg)
{
    char *item;
    while (true) {
        if (xQueueReceive(s_fila, &item, portMAX_DELAY) == pdTRUE) {
            grava(item);
            free(item);
        }
    }
}

void malhas_inicia(void)
{
    s_buf = heap_caps_malloc(MALHA_MAX, MALLOC_CAP_SPIRAM);
    s_fila = xQueueCreateWithCaps(8, sizeof(char *), MALLOC_CAP_SPIRAM);
    if (!s_buf || !s_fila) {
        ESP_LOGE(TAG, "sem memoria");
        return;
    }
    xTaskCreate(tarefa, "malhas", 4096, NULL, 2, NULL);
}
