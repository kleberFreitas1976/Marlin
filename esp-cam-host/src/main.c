// Central da impressora — ESP32-S3-CAM.
// Fase 1: chip, PSRAM, camera (ID do sensor), cartao SD e pagina com foto.
// Console serial na porta USB-UART: digite "ajuda".

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "camera.h"
#include "cartao_sd.h"
#include "driver/uart.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "nvs_flash.h"
#include "web.h"

static const char *TAG = "principal";

static void mostra_info(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    uint32_t flash = 0;
    esp_flash_get_size(NULL, &flash);
    printf("\nChip ESP32-S3 rev %d.%d, %d nucleos\n", chip.revision / 100, chip.revision % 100,
           chip.cores);
    printf("Flash %lu MB, PSRAM %u KB (livre %u KB), RAM interna livre %u KB\n",
           (unsigned long)(flash >> 20), (unsigned)(esp_psram_get_size() / 1024),
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    printf("Camera: %s (PID 0x%04x), resolucao maxima %dx%d, %s\n", camera_nome(), camera_pid(),
           camera_largura_max(), camera_altura_max(), camera_ligada() ? "ligada" : "desligada");
    if (sd_ok())
        printf("SD: %llu MB livres de %llu MB\n", sd_livre_bytes() >> 20, sd_total_bytes() >> 20);
    else
        printf("SD: ausente\n");
    printf("Wi-Fi: %s\n", web_conectado() ? web_ip() : "desconectado");
}

static const struct { const char *nome; framesize_t tam; } RESOLUCOES[] = {
    {"vga", FRAMESIZE_VGA},   {"svga", FRAMESIZE_SVGA}, {"xga", FRAMESIZE_XGA},
    {"hd", FRAMESIZE_HD},     {"sxga", FRAMESIZE_SXGA}, {"uxga", FRAMESIZE_UXGA},
    {"qxga", FRAMESIZE_QXGA}, {"qsxga", FRAMESIZE_QSXGA},
};

// Envia a foto pelo console em base64, entre marcadores, para o PC salvar.
// (tools/foto_serial.py faz isso.)
static void foto_serial(const char *res)
{
    if (!camera_ok()) {
        printf("camera nao iniciou\n");
        return;
    }
    if (res && *res) {
        bool achou = false;
        for (size_t i = 0; i < sizeof RESOLUCOES / sizeof RESOLUCOES[0]; i++) {
            if (strcasecmp(res, RESOLUCOES[i].nome) == 0) {
                achou = true;
                if (camera_resolucao(RESOLUCOES[i].tam) != ESP_OK) {
                    printf("o sensor nao suporta %s\n", res);
                    return;
                }
                // o sensor reajusta exposicao e balanco de branco: espera e descarta quadros
                if (camera_usa() == ESP_OK) {
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    for (int k = 0; k < 4; k++) {
                        camera_fb_t *v = esp_camera_fb_get();
                        if (v) esp_camera_fb_return(v);
                    }
                    camera_solta();
                }
            }
        }
        if (!achou) {
            printf("resolucao desconhecida: %s\n", res);
            return;
        }
    }
    camera_fb_t *fb = camera_foto();
    if (!fb) {
        printf("falha ao fotografar\n");
        return;
    }
    printf("\n@@FOTO INICIO %u %dx%d\n", (unsigned)fb->len, fb->width, fb->height);
    unsigned char linha[80];
    for (size_t i = 0; i < fb->len; i += 57) {
        size_t n = fb->len - i < 57 ? fb->len - i : 57, saida = 0;
        mbedtls_base64_encode(linha, sizeof linha, &saida, fb->buf + i, n);
        fwrite(linha, 1, saida, stdout);
        fputc('\n', stdout);
    }
    printf("@@FOTO FIM\n");
    fflush(stdout);
    camera_devolve(fb);
}

static void foto_sd(void)
{
    if (!sd_ok() || !camera_ok()) {
        printf("precisa de camera e SD\n");
        return;
    }
    camera_fb_t *fb = camera_foto();
    if (!fb) return;
    FILE *f = fopen(SD_RAIZ "/teste.jpg", "wb");
    if (f) {
        fwrite(fb->buf, 1, fb->len, f);
        fclose(f);
        printf("gravado %s/teste.jpg (%u bytes)\n", SD_RAIZ, (unsigned)fb->len);
    } else {
        printf("nao consegui gravar no SD\n");
    }
    camera_devolve(fb);
}

static void executa(char *linha)
{
    char *cmd = strtok(linha, " \t");
    char *arg = strtok(NULL, " \t");
    if (!cmd) return;
    if (!strcasecmp(cmd, "info")) mostra_info();
    else if (!strcasecmp(cmd, "foto")) foto_serial(arg);
    else if (!strcasecmp(cmd, "fotosd")) foto_sd();
    else if (!strcasecmp(cmd, "sd")) sd_ok() ? sd_lista(arg ? arg : SD_RAIZ) : (void)printf("SD ausente\n");
    else if (!strcasecmp(cmd, "reinicia")) esp_restart();
    else
        printf("comandos: info | foto [vga|svga|xga|hd|sxga|uxga|qxga|qsxga] | fotosd | "
               "sd [pasta] | reinicia\n");
}

static void console(void *arg)
{
    uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
    char buf[128];
    size_t n = 0;
    printf("\nConsole pronto. Digite \"ajuda\".\n");
    while (true) {
        uint8_t c;
        if (uart_read_bytes(UART_NUM_0, &c, 1, portMAX_DELAY) != 1) continue;
        if (c == '\r' || c == '\n') {
            if (n) {
                buf[n] = '\0';
                executa(buf);
                n = 0;
            }
        } else if (n < sizeof buf - 1) {
            buf[n++] = (char)c;
        }
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_LOGI(TAG, "iniciando");

    camera_inicia();
    sd_inicia();
    web_inicia();
    mostra_info();

    xTaskCreate(console, "console", 6144, NULL, 3, NULL);
}
