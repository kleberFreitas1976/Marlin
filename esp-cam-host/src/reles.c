#include "reles.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "impressao.h"
#include "marlin.h"
#include "nvs.h"
#include "pinos.h"

static const char *TAG = "reles";

// A camera usa LEDC_TIMER_0 / LEDC_CHANNEL_0 (XCLK)
#define LUZ_TIMER LEDC_TIMER_1
#define LUZ_CANAL LEDC_CHANNEL_1

static reles_estado_t s_e = {.ativo_baixo = true, .luz_auto = true, .auto_min = 10, .auto_falta_s = -1};
static int s_luz_manual;
static int64_t s_auto_em_us;     // quando desligar sozinho (0 = nada agendado)

// ------------------------------------------------------------------ pinos
static void rele(int pino, bool energizado)
{
    if (energizado) {
        gpio_set_direction(pino, GPIO_MODE_OUTPUT);
        gpio_set_level(pino, s_e.ativo_baixo ? 0 : 1);
    } else if (s_e.ativo_baixo) {
        gpio_set_direction(pino, GPIO_MODE_INPUT);    // solto: o pull-up do modulo manda
    } else {
        gpio_set_direction(pino, GPIO_MODE_OUTPUT);
        gpio_set_level(pino, 0);
    }
}

static void grava(void)
{
    nvs_handle_t h;
    if (nvs_open("reles", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "imp_off", s_e.impressora_desligada);
    nvs_set_u8(h, "baixo", s_e.ativo_baixo);
    nvs_set_u8(h, "luz", s_luz_manual);
    nvs_set_u8(h, "luz_auto", s_e.luz_auto);
    nvs_set_u8(h, "auto", s_e.auto_desligar);
    nvs_set_u16(h, "auto_min", (uint16_t)s_e.auto_min);
    nvs_commit(h);
    nvs_close(h);
}

static void aplica_luz(int pct)
{
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    s_e.luz = pct;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LUZ_CANAL, (8191 * pct) / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LUZ_CANAL);
}

// ------------------------------------------------------------------ espera do bico
static bool bico_frio(void)
{
    marlin_estado_t m;
    marlin_estado(&m);
    // sem TinyBee respondendo nao da para saber: considera frio so se desconectada
    // ha tempo (a impressora pode ja estar desligada por outro meio)
    if (!m.respondendo) return true;
    return m.bico < RELE_ESFRIAR_C && m.bico_alvo <= 0;
}

static void corta_impressora(void)
{
    s_e.impressora_desligada = true;
    s_e.aguardando_esfriar = false;
    rele(RELE_IMPRESSORA_PIN, true);
    grava();
    ESP_LOGW(TAG, "impressora DESLIGADA pelo rele");
}

static void corta_tudo(void)
{
    s_e.tudo_agendado = false;
    ESP_LOGW(TAG, "DESLIGANDO TUDO (pulso no rele)");
    rele(RELE_TUDO_PIN, true);
    vTaskDelay(pdMS_TO_TICKS(2000));    // se o contator nao abrir, o ESP continua aqui
    rele(RELE_TUDO_PIN, false);
}

static void tarefa(void *arg)
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        if (s_auto_em_us) {
            int64_t falta = s_auto_em_us - esp_timer_get_time();
            s_e.auto_falta_s = falta > 0 ? (int)(falta / 1000000) : 0;
            if (falta <= 0) {
                s_auto_em_us = 0;
                s_e.auto_falta_s = -1;
                char motivo[80];
                ESP_LOGW(TAG, "desligamento automatico depois da impressao");
                if (!s_e.impressora_desligada) reles_desliga_impressora(false, motivo, sizeof motivo);
            }
        }
        if ((s_e.aguardando_esfriar || s_e.tudo_agendado) && bico_frio()) {
            if (s_e.tudo_agendado) corta_tudo();
            else corta_impressora();
        }
    }
}

// ------------------------------------------------------------------ API
void reles_inicia(void)
{
    nvs_handle_t h;
    if (nvs_open("reles", NVS_READONLY, &h) == ESP_OK) {
        uint8_t u;
        if (nvs_get_u8(h, "imp_off", &u) == ESP_OK) s_e.impressora_desligada = u;
        if (nvs_get_u8(h, "baixo", &u) == ESP_OK) s_e.ativo_baixo = u;
        if (nvs_get_u8(h, "luz", &u) == ESP_OK) s_luz_manual = u;
        if (nvs_get_u8(h, "luz_auto", &u) == ESP_OK) s_e.luz_auto = u;
        if (nvs_get_u8(h, "auto", &u) == ESP_OK) s_e.auto_desligar = u;
        uint16_t m;
        if (nvs_get_u16(h, "auto_min", &m) == ESP_OK) s_e.auto_min = m;
        nvs_close(h);
    }
    gpio_reset_pin(RELE_IMPRESSORA_PIN);
    gpio_reset_pin(RELE_TUDO_PIN);
    rele(RELE_TUDO_PIN, false);
    // estado guardado: se a impressora foi desligada, continua desligada
    rele(RELE_IMPRESSORA_PIN, s_e.impressora_desligada);

    ledc_timer_config_t t = {.speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_13_BIT,
                             .timer_num = LUZ_TIMER, .freq_hz = 1000, .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&t);
    ledc_channel_config_t c = {.gpio_num = LUZ_PIN, .speed_mode = LEDC_LOW_SPEED_MODE,
                               .channel = LUZ_CANAL, .timer_sel = LUZ_TIMER, .duty = 0};
    ledc_channel_config(&c);
    aplica_luz(s_luz_manual);
    xTaskCreate(tarefa, "reles", 3072, NULL, 3, NULL);
    ESP_LOGI(TAG, "reles prontos (modulo ativo em nivel %s); impressora %s",
             s_e.ativo_baixo ? "baixo" : "alto", s_e.impressora_desligada ? "DESLIGADA" : "ligada");
}

void reles_estado(reles_estado_t *e) { *e = s_e; }

esp_err_t reles_desliga_impressora(bool forcar, char *motivo, size_t tam)
{
    if (!forcar && imp_ativa()) {
        strlcpy(motivo, "ha uma impressao em andamento", tam);
        return ESP_ERR_INVALID_STATE;
    }
    if (forcar || bico_frio()) corta_impressora();
    else {
        s_e.aguardando_esfriar = true;
        strlcpy(motivo, "aguardando o bico esfriar", tam);
        ESP_LOGI(TAG, "desligar impressora: aguardando o bico ficar abaixo de %d C", RELE_ESFRIAR_C);
    }
    return ESP_OK;
}

esp_err_t reles_liga_impressora(void)
{
    s_e.impressora_desligada = false;
    s_e.aguardando_esfriar = false;
    rele(RELE_IMPRESSORA_PIN, false);
    grava();
    ESP_LOGI(TAG, "impressora religada");
    return ESP_OK;
}

esp_err_t reles_desliga_tudo(bool forcar, char *motivo, size_t tam)
{
    if (!forcar && imp_ativa()) {
        strlcpy(motivo, "ha uma impressao em andamento", tam);
        return ESP_ERR_INVALID_STATE;
    }
    if (forcar || bico_frio()) corta_tudo();
    else {
        s_e.tudo_agendado = true;
        strlcpy(motivo, "aguardando o bico esfriar", tam);
    }
    return ESP_OK;
}

void reles_cancela_espera(void)
{
    s_auto_em_us = 0;
    s_e.auto_falta_s = -1;
    s_e.aguardando_esfriar = false;
    s_e.tudo_agendado = false;
}

void reles_luz(int pct)
{
    s_luz_manual = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    aplica_luz(s_luz_manual);
    grava();
}

void reles_luz_auto(bool sim)
{
    s_e.luz_auto = sim;
    grava();
}

void reles_polaridade(bool ativo_baixo)
{
    s_e.ativo_baixo = ativo_baixo;
    rele(RELE_IMPRESSORA_PIN, s_e.impressora_desligada);
    rele(RELE_TUDO_PIN, false);
    grava();
}

void reles_auto(bool ativo, int minutos)
{
    s_e.auto_desligar = ativo;
    if (minutos >= 0 && minutos <= 720) s_e.auto_min = minutos;
    if (!ativo) {
        s_auto_em_us = 0;
        s_e.auto_falta_s = -1;
    }
    grava();
}

void reles_impressao(bool imprimindo, bool concluida)
{
    if (imprimindo) {                 // nova impressao: esquece um desligamento pendente
        s_auto_em_us = 0;
        s_e.auto_falta_s = -1;
    } else if (concluida && s_e.auto_desligar && !s_e.impressora_desligada) {
        s_auto_em_us = esp_timer_get_time() + (int64_t)s_e.auto_min * 60 * 1000000;
        s_e.auto_falta_s = s_e.auto_min * 60;
        ESP_LOGI(TAG, "impressao concluida: desliga a impressora em %d min", s_e.auto_min);
    }
    if (s_e.luz_auto) aplica_luz(imprimindo ? 100 : s_luz_manual);
}
