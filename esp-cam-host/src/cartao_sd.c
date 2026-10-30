#include "cartao_sd.h"

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "pinos.h"
#include "sdmmc_cmd.h"

static const char *TAG = "sd";
static sdmmc_card_t *s_card;

esp_err_t sd_inicia(void)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = SD_PIN_CLK;
    slot.cmd = SD_PIN_CMD;
    slot.d0 = SD_PIN_D0;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mcfg = {
        .format_if_mount_failed = false,     // nunca apagar o cartao do usuario
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };
    esp_err_t err = esp_vfs_fat_sdmmc_mount(SD_RAIZ, &host, &slot, &mcfg, &s_card);
    if (err != ESP_OK) {
        s_card = NULL;
        ESP_LOGW(TAG, "sem cartao SD (0x%x): %s", err,
                 err == ESP_FAIL ? "cartao nao esta em FAT32" : "cartao ausente ou mal encaixado");
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
