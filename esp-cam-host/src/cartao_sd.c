#include "cartao_sd.h"

#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "pinos.h"
#include "sdmmc_cmd.h"

static const char *TAG = "sd";
static sdmmc_card_t *s_card;

static sdmmc_slot_config_t slot_config(void)
{
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = SD_PIN_CLK;
    slot.cmd = SD_PIN_CMD;
    slot.d0 = SD_PIN_D0;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    return slot;
}

// Quando a montagem falha por falta de FAT: le a tabela de particoes e o setor
// de boot e diz o que ha no cartao (exFAT e NTFS nao sao lidos pelo ESP-IDF).
static void diagnostica(void)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    sdmmc_slot_config_t slot = slot_config();
    sdmmc_card_t card;
    uint8_t *setor = heap_caps_malloc(512, MALLOC_CAP_DMA);
    if (!setor) return;
    if (sdmmc_host_init() != ESP_OK ||
        sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot) != ESP_OK ||
        sdmmc_card_init(&host, &card) != ESP_OK) {
        ESP_LOGW(TAG, "diagnostico: o cartao nao responde");
        goto fim;
    }
    sdmmc_card_print_info(stdout, &card);
    if (sdmmc_read_sectors(&card, setor, 0, 1) != ESP_OK) {
        ESP_LOGW(TAG, "diagnostico: nao leu o setor 0");
        goto fim;
    }
    uint32_t inicio = 0;
    if (setor[510] == 0x55 && setor[511] == 0xAA && setor[0x1C2] != 0 &&
        memcmp(setor + 3, "EXFAT", 5) && memcmp(setor + 3, "NTFS", 4) &&
        memcmp(setor + 0x52, "FAT32", 5)) {
        inicio = setor[0x1C6] | setor[0x1C7] << 8 | setor[0x1C8] << 16 | (uint32_t)setor[0x1C9] << 24;
        ESP_LOGI(TAG, "diagnostico: particao 1 tipo 0x%02x no setor %lu", setor[0x1C2],
                 (unsigned long)inicio);
        if (sdmmc_read_sectors(&card, setor, inicio, 1) != ESP_OK) goto fim;
    }
    char oem[9] = {0}, fat32[9] = {0};
    memcpy(oem, setor + 3, 8);
    memcpy(fat32, setor + 0x52, 8);
    const char *tipo = !memcmp(oem, "EXFAT", 5) ? "exFAT (nao suportado: formate em FAT32)"
                     : !memcmp(oem, "NTFS", 4) ? "NTFS (nao suportado: formate em FAT32)"
                     : !memcmp(fat32, "FAT32", 5) ? "FAT32 (deveria montar: problema de leitura)"
                     : "desconhecido (sem formatacao?)";
    ESP_LOGW(TAG, "diagnostico: setor de boot \"%s\" / \"%s\" -> %s", oem, fat32, tipo);
fim:
    free(setor);
    sdmmc_host_deinit();
}

static esp_err_t monta(int khz, bool formatar)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = khz;
    sdmmc_slot_config_t slot = slot_config();
    esp_vfs_fat_sdmmc_mount_config_t mcfg = {
        .format_if_mount_failed = formatar,  // so com o comando "formatar SIM"
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };
    return esp_vfs_fat_sdmmc_mount(SD_RAIZ, &host, &slot, &mcfg, &s_card);
}

esp_err_t sd_inicia(void)
{
    esp_err_t err = monta(SDMMC_FREQ_HIGHSPEED, false);
    if (err == ESP_FAIL) {
        ESP_LOGW(TAG, "nao montou a 40 MHz; tentando a 20 MHz");
        err = monta(SDMMC_FREQ_DEFAULT, false);
    }
    if (err != ESP_OK) {
        s_card = NULL;
        ESP_LOGW(TAG, "sem cartao SD (0x%x): %s", err,
                 err == ESP_FAIL ? "sem sistema FAT no cartao" : "cartao ausente ou mal encaixado");
        if (err == ESP_FAIL) diagnostica();
        return err;
    }
    sdmmc_card_print_info(stdout, s_card);
    ESP_LOGI(TAG, "SD montado em %s: %llu MB livres de %llu MB", SD_RAIZ,
             sd_livre_bytes() >> 20, sd_total_bytes() >> 20);
    mkdir(SD_RAIZ "/gcodes", 0775);
    mkdir(SD_RAIZ "/timelapse", 0775);
    return ESP_OK;
}

bool sd_ok(void) { return s_card != NULL; }

esp_err_t sd_formata(void)
{
    // So formata cartao que NAO monta (sem FAT, GPT, exFAT...): um cartao FAT32
    // valido nunca e apagado por aqui. Cria tabela MBR e FAT32.
    if (s_card) {
        ESP_LOGW(TAG, "o cartao ja esta montado em FAT: nada a formatar");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGW(TAG, "formatando o cartao (MBR + FAT32)...");
    esp_err_t err = monta(SDMMC_FREQ_DEFAULT, true);
    if (err != ESP_OK) {
        s_card = NULL;
        ESP_LOGE(TAG, "formatacao falhou (0x%x)", err);
        return err;
    }
    ESP_LOGI(TAG, "cartao formatado: %llu MB livres", sd_livre_bytes() >> 20);
    mkdir(SD_RAIZ "/gcodes", 0775);
    mkdir(SD_RAIZ "/timelapse", 0775);
    return ESP_OK;
}

static void sd_espaco(uint64_t *total, uint64_t *livre)
{
    *total = *livre = 0;
    FATFS *fs;
    DWORD clusters_livres;
    if (!s_card || f_getfree("0:", &clusters_livres, &fs) != FR_OK) return;
    uint64_t setor = fs->ssize;
    *total = (uint64_t)(fs->n_fatent - 2) * fs->csize * setor;
    *livre = (uint64_t)clusters_livres * fs->csize * setor;
}

uint64_t sd_total_bytes(void) { uint64_t t, l; sd_espaco(&t, &l); return t; }
uint64_t sd_livre_bytes(void) { uint64_t t, l; sd_espaco(&t, &l); return l; }

void sd_lista(const char *pasta)
{
    DIR *d = opendir(pasta);
    if (!d) {
        printf("nao abriu %s\n", pasta);
        return;
    }
    struct dirent *e;
    char caminho[300];
    while ((e = readdir(d)) != NULL) {
        struct stat st;
        snprintf(caminho, sizeof caminho, "%s/%s", pasta, e->d_name);
        if (stat(caminho, &st) == 0)
            printf("  %s%s  %ld\n", e->d_name, S_ISDIR(st.st_mode) ? "/" : "", (long)st.st_size);
    }
    closedir(d);
}
