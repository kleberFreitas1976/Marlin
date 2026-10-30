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
#include "rede.h"
#include "reles.h"
#include "timelapse.h"
#include "arquivos.h"
#include "impressao.h"
#include "marlin.h"
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
    marlin_estado_t tb;
    marlin_estado(&tb);
    printf("TinyBee: %s\n", !tb.conectada ? "nao conectada"
                            : tb.respondendo ? "conectada e respondendo" : "conectada, sem resposta");
    printf("Wi-Fi: %s %s%s\n", rede_ssid()[0] ? rede_ssid() : "(nenhuma rede)",
           rede_conectado() ? rede_ip() : "desconectado",
           rede_ap_ativo() ? "  | AP \"" AP_SSID "\" ligado: http://" AP_IP "/wifi" : "");
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

// ---------------------------------------------------------------- TinyBee
// Cada linha trocada com o Marlin vai para o console serial e para os navegadores.
static void marlin_linha(marlin_tipo_t tipo, const char *linha)
{
    if (tipo != MARLIN_TEMP && tipo != MARLIN_POS)
        printf("%s %s\n", tipo == MARLIN_TX ? "TB>" : "TB<", linha);
    web_linha_marlin(tipo, linha);
    imp_linha_marlin(tipo, linha);
}

static void tb_envia(const char *gcode)
{
    esp_err_t err = marlin_envia(gcode, true);
    if (err != ESP_OK) printf("TB: nao enfileirou (%s)\n", esp_err_to_name(err));
}

static void tb_estado(void)
{
    marlin_estado_t e;
    marlin_estado(&e);
    printf("TinyBee: %s | bico %.1f/%.0f  mesa %.1f/%.0f | X%.2f Y%.2f Z%.2f | fila %d%s\n",
           !e.conectada ? "nao conectada" : e.respondendo ? "respondendo" : "conectada, calada",
           e.bico, e.bico_alvo, e.mesa, e.mesa_alvo, e.x, e.y, e.z, e.fila,
           e.ocupada ? " | ocupada" : "");
    if (e.firmware[0]) printf("Firmware: %s\n", e.firmware);
}

// G-code de teste inofensivo: so leituras e pausas, sem mover e sem aquecer.
// Passa pelo mesmo caminho de um envio (miniatura e dados sao extraidos).
static void gera_teste(void)
{
    static const char *const LINHAS[] = {
        "; generated by teste_seguro (ESP32-S3-CAM)",
        "; thumbnail begin 1x1 92",
        "; iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==",
        "; thumbnail end",
        "; estimated printing time (normal mode) = 0m 20s",
        "; filament used [g] = 0.00",
        "; filament_type = TESTE",
        "M118 P1 TL:INICIO teste_seguro",
        ";LAYER_CHANGE", "M105", "G4 P300 ; comentario no fim da linha", "M114",
        ";LAYER_CHANGE", "M105", "G4 P300", "M31",
        ";LAYER_CHANGE", "M105", "G4 P300", "M118 P1 TL:CAMADA 3/3",
        "M118 P1 TL:FIM",
    };
    arq_envio_t *e = arq_envio_abre("teste_seguro.gcode");
    if (!e) {
        printf("nao consegui criar o arquivo (SD?)\n");
        return;
    }
    for (int rep = 0; rep < 20; rep++)            // 20 repeticoes: ~20 s de "impressao"
        for (size_t i = (rep ? 7 : 0); i < sizeof LINHAS / sizeof LINHAS[0]; i++) {
            if (rep && i == 7) continue;
            if (rep && (!strncmp(LINHAS[i], "M118", 4))) continue;
            arq_envio_dados(e, (const uint8_t *)LINHAS[i], strlen(LINHAS[i]));
            arq_envio_dados(e, (const uint8_t *)"\n", 1);
        }
    arq_envio_dados(e, (const uint8_t *)"M118 P1 TL:FIM\n", 15);
    printf(arq_envio_fecha(e, true) == ESP_OK ? "criado /sd/gcodes/teste_seguro.gcode\n"
                                              : "falha ao gravar\n");
}

// Manda um arquivo do SD pelo console em base64 (diagnostico; tools/baixa_serial.py)
static void baixa_serial(const char *caminho)
{
    FILE *f = caminho ? fopen(caminho, "rb") : NULL;
    if (!f) {
        printf("nao abriu %s\n", caminho ? caminho : "(nada)");
        return;
    }
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fseek(f, 0, SEEK_SET);
    printf("\n@@ARQ INICIO %ld\n", tam);
    unsigned char in[57], out[80];
    size_t n, s;
    while ((n = fread(in, 1, sizeof in, f)) > 0) {
        mbedtls_base64_encode(out, sizeof out, &s, in, n);
        fwrite(out, 1, s, stdout);
        fputc('\n', stdout);
    }
    printf("@@ARQ FIM\n");
    fflush(stdout);
    fclose(f);
}

static void mostra_impressao(void)
{
    imp_info_t i;
    imp_info(&i);
    static const char *const EST[] = {"parada", "imprimindo", "pausando", "pausada",
                                      "cancelando", "concluida", "cancelada", "erro"};
    printf("Impressao: %s | %s | %u/%u bytes | %u linhas | camada %d/%d | %d s | restam %d s | %s\n",
           EST[i.estado], i.nome, (unsigned)i.bytes, (unsigned)i.total, (unsigned)i.linhas,
           i.camada, i.camadas, i.decorrido_s, i.restante_s, i.aviso);
    if (i.tft) printf("TFT: %s, camada %d/%d, %d s\n", i.tft_nome, i.tft_camada, i.tft_camadas,
                      i.tft_decorrido_s);
}

static void executa(char *linha)
{
    // "m <G-code>": manda a linha inteira para o Marlin
    if ((linha[0] == 'm' || linha[0] == 'M') && linha[1] == ' ') {
        tb_envia(linha + 2);
        return;
    }
    char *cmd = strtok(linha, " \t");
    char *arg = strtok(NULL, " \t");
    if (!cmd) return;
    if (!strcasecmp(cmd, "info")) mostra_info();
    else if (!strcasecmp(cmd, "tb")) tb_estado();
    else if (!strcasecmp(cmd, "foto")) foto_serial(arg);
    else if (!strcasecmp(cmd, "fotosd")) foto_sd();
    else if (!strcasecmp(cmd, "formatar")) {
        if (arg && !strcmp(arg, "SIM"))
            sd_formata();
        else
            printf("apaga o cartao SD (so se ele nao montar). Para confirmar: formatar SIM\n");
    }
    else if (!strcasecmp(cmd, "sd")) sd_ok() ? sd_lista(arg ? arg : SD_RAIZ) : (void)printf("SD ausente\n");
    else if (!strcasecmp(cmd, "reinicia")) esp_restart();
    else if (!strcasecmp(cmd, "gerateste")) gera_teste();
    else if (!strcasecmp(cmd, "baixa")) baixa_serial(arg);
    else if (!strcasecmp(cmd, "imp")) mostra_impressao();
    else if (!strcasecmp(cmd, "imprime")) {
        char erro[96];
        if (!arg) printf("uso: imprime <arquivo.gcode>\n");
        else if (imp_imprime(arg, erro, sizeof erro) != ESP_OK) printf("nao comecou: %s\n", erro);
    }
    else if (!strcasecmp(cmd, "pausa")) imp_pausa();
    else if (!strcasecmp(cmd, "retoma")) imp_retoma();
    else if (!strcasecmp(cmd, "cancela")) imp_cancela();
    else
        printf("comandos: info | foto [vga|svga|xga|hd|sxga|uxga|qxga|qsxga] | fotosd | "
               "sd [pasta] | formatar SIM | tb | m <G-code para a TinyBee> | gerateste | "
               "imprime <arq> | imp | pausa | retoma | cancela | reinicia\n");
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

// Inicio e fim de qualquer impressao (da placa ou da TFT)
static void ciclo_impressao(bool comecou, const char *nome)
{
    tl_ciclo(comecou, nome);
    reles_impressao(comecou);       // luz automatica para o time-lapse
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_LOGI(TAG, "iniciando");

    reles_inicia();                 // antes de tudo: pinos dos reles em estado seguro
    camera_inicia();
    sd_inicia();
    rede_inicia();
    web_inicia();
    imp_inicia();
    tl_inicia();
    imp_ouvintes(tl_camada, tl_foto, ciclo_impressao);
    marlin_inicia(marlin_linha);
    mostra_info();

    xTaskCreate(console, "console", 6144, NULL, 3, NULL);
}
