#include "timelapse.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "camera.h"
#include "cartao_sd.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

static const char *TAG = "timelapse";

typedef enum { EV_FOTO, EV_COMECA, EV_TERMINA, EV_MONTA } evento_t;
typedef struct {
    evento_t ev;
    char texto[96];
} msg_t;

static QueueHandle_t s_fila;
static SemaphoreHandle_t s_mtx;
static tl_config_t s_cfg = {.ativo = true, .modo = TL_AUTO, .resolucao = FRAMESIZE_UXGA, .fps = 25};
static char s_pasta[160];          // gravacao em andamento ("" = nenhuma)
static int s_fotos;
static bool s_viu_tl_foto;         // o arquivo usa TL:FOTO
static bool s_montando;
static int s_larg, s_alt;
// Pedido de foto pendente: camadas rapidas nao enchem a fila (uma foto em UXGA leva
// ~0,3 s), e os avisos de inicio e fim sempre encontram lugar.
static volatile bool s_foto_pendente;

// ------------------------------------------------------------------ configuracao
void tl_config(tl_config_t *saida) { *saida = s_cfg; }

esp_err_t tl_grava_config(const tl_config_t *c)
{
    s_cfg = *c;
    if (s_cfg.fps < 1 || s_cfg.fps > 60) s_cfg.fps = 25;
    nvs_handle_t h;
    esp_err_t err = nvs_open("timelapse", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_set_u8(h, "ativo", s_cfg.ativo);
    nvs_set_u8(h, "modo", s_cfg.modo);
    nvs_set_i32(h, "res", s_cfg.resolucao);
    nvs_set_i32(h, "fps", s_cfg.fps);
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static void le_config(void)
{
    nvs_handle_t h;
    if (nvs_open("timelapse", NVS_READONLY, &h) != ESP_OK) return;
    uint8_t u;
    int32_t i;
    if (nvs_get_u8(h, "ativo", &u) == ESP_OK) s_cfg.ativo = u;
    if (nvs_get_u8(h, "modo", &u) == ESP_OK) s_cfg.modo = u;
    if (nvs_get_i32(h, "res", &i) == ESP_OK) s_cfg.resolucao = i;
    if (nvs_get_i32(h, "fps", &i) == ESP_OK) s_cfg.fps = i;
    nvs_close(h);
}

// ------------------------------------------------------------------ ouvintes (outras tarefas)
static void posta(evento_t ev, const char *texto)
{
    if (ev == EV_FOTO) {
        if (s_foto_pendente) return;      // ja ha uma na fila: esta seria a mesma cena
        s_foto_pendente = true;
    }
    msg_t m = {.ev = ev};
    if (texto) strlcpy(m.texto, texto, sizeof m.texto);
    // inicio e fim nao podem se perder; quem chama e a tarefa do Marlin, entao a
    // espera e curta (a fila so tem avisos raros e no maximo uma foto)
    if (xQueueSend(s_fila, &m, ev == EV_FOTO ? 0 : pdMS_TO_TICKS(500)) != pdTRUE && ev == EV_FOTO)
        s_foto_pendente = false;
}

void tl_camada(int camada, int total)
{
    // em AUTO, a camada so vale enquanto o arquivo nao mostrou que usa TL:FOTO
    if (s_pasta[0] && (s_cfg.modo == TL_CAMADA || (s_cfg.modo == TL_AUTO && !s_viu_tl_foto)))
        posta(EV_FOTO, NULL);
}

void tl_foto(void)
{
    s_viu_tl_foto = true;
    if (s_pasta[0] && s_cfg.modo != TL_CAMADA) posta(EV_FOTO, NULL);
}

void tl_ciclo(bool comecou, const char *nome) { posta(comecou ? EV_COMECA : EV_TERMINA, nome); }

esp_err_t tl_monta_video(const char *pasta)
{
    if (!tl_pasta_valida(pasta)) return ESP_ERR_INVALID_ARG;
    posta(EV_MONTA, pasta);
    return ESP_OK;
}

bool tl_gravando(void) { return s_pasta[0] != '\0'; }

void tl_status(char *pasta, size_t tam, int *fotos, bool *montando)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    const char *b = strrchr(s_pasta, '/');
    strlcpy(pasta, b ? b + 1 : "", tam);
    *fotos = s_fotos;
    *montando = s_montando;
    xSemaphoreGive(s_mtx);
}

// ------------------------------------------------------------------ gravacao
static void comeca(const char *nome)
{
    if (!s_cfg.ativo || !sd_ok() || !camera_ok()) return;
    mkdir(TL_PASTA, 0775);
    char data[24];
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    if (tm.tm_year > 120) strftime(data, sizeof data, "%Y%m%d_%H%M", &tm);
    else snprintf(data, sizeof data, "sem_data_%lld", (long long)(t % 100000));   // sem NTP
    // nome sem extensao e sem caracteres estranhos
    char limpo[48];
    size_t k = 0;
    for (const char *c = nome; *c && *c != '.' && k < sizeof limpo - 1; c++)
        limpo[k++] = (isalnum((unsigned char)*c) || *c == '-' || *c == '_') ? *c : '_';
    limpo[k] = '\0';
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    snprintf(s_pasta, sizeof s_pasta, TL_PASTA "/%s_%s", data, limpo);
    s_fotos = 0;
    s_viu_tl_foto = false;
    xSemaphoreGive(s_mtx);
    mkdir(s_pasta, 0775);
    camera_resolucao((framesize_t)s_cfg.resolucao);
    camera_manter_ligada(true);
    ESP_LOGI(TAG, "gravando em %s", s_pasta);
}

static void fotografa(void)
{
    if (!s_pasta[0]) return;
    camera_fb_t *fb = camera_foto();
    if (!fb) {
        ESP_LOGW(TAG, "foto falhou");
        return;
    }
    char c[200];
    snprintf(c, sizeof c, "%s/%05d.jpg", s_pasta, s_fotos + 1);
    FILE *f = fopen(c, "wb");
    if (f) {
        fwrite(fb->buf, 1, fb->len, f);
        fclose(f);
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        s_fotos++;
        xSemaphoreGive(s_mtx);
        s_larg = fb->width;
        s_alt = fb->height;
    }
    camera_devolve(fb);
}

// ------------------------------------------------------------------ video AVI (MJPEG)
static void w32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void w16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }
static void cc(FILE *f, const char *s) { fwrite(s, 1, 4, f); }

static bool jpeg_tamanho(const char *arq, int *w, int *h)
{
    // procura o marcador SOF0/SOF2 no comeco do JPEG
    FILE *f = fopen(arq, "rb");
    if (!f) return false;
    uint8_t *b = malloc(4096);           // fora da pilha da tarefa
    if (!b) {
        fclose(f);
        return false;
    }
    size_t n = fread(b, 1, 4096, f);
    fclose(f);
    bool achou = false;
    for (size_t i = 2; i + 9 < n && !achou; i++)
        if (b[i] == 0xFF && (b[i + 1] == 0xC0 || b[i + 1] == 0xC2)) {
            *h = b[i + 5] << 8 | b[i + 6];
            *w = b[i + 7] << 8 | b[i + 8];
            achou = true;
        }
    free(b);
    return achou;
}

static int conta_fotos(const char *pasta)
{
    int n = 0;
    char c[200];
    struct stat st;
    while (true) {
        snprintf(c, sizeof c, "%s/%05d.jpg", pasta, n + 1);
        if (stat(c, &st) != 0) return n;
        n++;
    }
}

static esp_err_t monta(const char *pasta)
{
    int n = conta_fotos(pasta);
    if (n < 2) return ESP_ERR_NOT_FOUND;
    char c[200], tmp[200];
    int w = 0, h = 0;
    snprintf(c, sizeof c, "%s/%05d.jpg", pasta, 1);
    if (!jpeg_tamanho(c, &w, &h)) w = s_larg, h = s_alt;
    snprintf(tmp, sizeof tmp, "%s/timelapse.avi.tmp", pasta);
    FILE *f = fopen(tmp, "wb");
    if (!f) return ESP_FAIL;
    setvbuf(f, NULL, _IOFBF, 32 * 1024);
    uint32_t *idx = heap_caps_malloc(n * 2 * sizeof(uint32_t), MALLOC_CAP_SPIRAM);
    uint8_t *buf = heap_caps_malloc(32 * 1024, MALLOC_CAP_SPIRAM);
    if (!idx || !buf) {
        fclose(f);
        free(idx);
        free(buf);
        return ESP_ERR_NO_MEM;
    }
    const int fps = s_cfg.fps;
    // ---- cabecalho (tamanhos corrigidos no fim)
    cc(f, "RIFF"); w32(f, 0); cc(f, "AVI ");
    cc(f, "LIST"); w32(f, 4 + 64 + 12 + 64 + 48); cc(f, "hdrl");
    cc(f, "avih"); w32(f, 56);
    w32(f, 1000000 / fps); w32(f, 0); w32(f, 0); w32(f, 0x10);   // AVIF_HASINDEX
    w32(f, n); w32(f, 0); w32(f, 1); w32(f, 0); w32(f, w); w32(f, h);
    w32(f, 0); w32(f, 0); w32(f, 0); w32(f, 0);
    cc(f, "LIST"); w32(f, 4 + 64 + 48); cc(f, "strl");
    cc(f, "strh"); w32(f, 56);
    cc(f, "vids"); cc(f, "MJPG"); w32(f, 0); w16(f, 0); w16(f, 0); w32(f, 0);
    w32(f, 1); w32(f, fps); w32(f, 0); w32(f, n); w32(f, 0); w32(f, 0xFFFFFFFF); w32(f, 0);
    w16(f, 0); w16(f, 0); w16(f, w); w16(f, h);
    cc(f, "strf"); w32(f, 40);
    w32(f, 40); w32(f, w); w32(f, h); w16(f, 1); w16(f, 24); cc(f, "MJPG");
    w32(f, w * h * 3); w32(f, 0); w32(f, 0); w32(f, 0); w32(f, 0);
    long pos_movi = ftell(f);
    cc(f, "LIST"); w32(f, 0); cc(f, "movi");
    // ---- quadros
    uint32_t desloc = 4;      // relativo ao "movi"
    for (int i = 0; i < n; i++) {
        snprintf(c, sizeof c, "%s/%05d.jpg", pasta, i + 1);
        FILE *j = fopen(c, "rb");
        uint32_t tam = 0;
        if (j) {
            fseek(j, 0, SEEK_END);
            tam = ftell(j);
            fseek(j, 0, SEEK_SET);
        }
        cc(f, "00dc");
        w32(f, tam);
        size_t r;
        while (j && (r = fread(buf, 1, 32 * 1024, j)) > 0) fwrite(buf, 1, r, f);
        if (j) fclose(j);
        if (tam & 1) fputc(0, f);
        idx[i * 2] = desloc;
        idx[i * 2 + 1] = tam;
        desloc += 8 + tam + (tam & 1);
        if (i % 20 == 0) vTaskDelay(1);   // deixa as outras tarefas respirarem
    }
    long fim_movi = ftell(f);
    // ---- indice
    cc(f, "idx1"); w32(f, n * 16);
    for (int i = 0; i < n; i++) {
        cc(f, "00dc"); w32(f, 0x10); w32(f, idx[i * 2]); w32(f, idx[i * 2 + 1]);
    }
    long total = ftell(f);
    fseek(f, 4, SEEK_SET);
    w32(f, total - 8);
    fseek(f, pos_movi + 4, SEEK_SET);
    w32(f, fim_movi - pos_movi - 8);
    fclose(f);
    free(idx);
    free(buf);
    snprintf(c, sizeof c, "%s/timelapse.avi", pasta);
    remove(c);
    rename(tmp, c);
    ESP_LOGI(TAG, "video montado: %s (%d quadros, %dx%d, %d fps, %ld KB)", c, n, w, h, fps, total / 1024);
    return ESP_OK;
}

static void termina(void)
{
    if (!s_pasta[0]) return;
    camera_manter_ligada(false);
    camera_resolucao(FRAMESIZE_SVGA);   // volta a resolucao do video ao vivo
    char pasta[160];
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    strlcpy(pasta, s_pasta, sizeof pasta);
    s_pasta[0] = '\0';
    s_montando = true;
    xSemaphoreGive(s_mtx);
    ESP_LOGI(TAG, "fim da gravacao: %d fotos; montando o video", s_fotos);
    monta(pasta);
    s_montando = false;
}

static void tarefa(void *arg)
{
    msg_t m;
    while (true) {
        if (xQueueReceive(s_fila, &m, portMAX_DELAY) != pdTRUE) continue;
        switch (m.ev) {
        case EV_COMECA: comeca(m.texto); break;
        case EV_FOTO:
            s_foto_pendente = false;
            fotografa();
            break;
        case EV_TERMINA: termina(); break;
        case EV_MONTA: {
            char c[200];
            snprintf(c, sizeof c, TL_PASTA "/%s", m.texto);
            s_montando = true;
            monta(c);
            s_montando = false;
            break;
        }
        }
    }
}

void tl_inicia(void)
{
    s_mtx = xSemaphoreCreateMutex();
    s_fila = xQueueCreate(16, sizeof(msg_t));
    le_config();
    xTaskCreatePinnedToCore(tarefa, "timelapse", 8192, NULL, 4, NULL, 0);
}

// ------------------------------------------------------------------ lista
bool tl_pasta_valida(const char *p)
{
    if (!p[0] || p[0] == '.' || strlen(p) > 80) return false;
    for (; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return false;
    return true;
}

char *tl_lista_json(void)
{
    size_t tam = 4096, n = 0;
    char *j = malloc(tam);
    if (!j) return NULL;
    n += snprintf(j, tam, "[");
    DIR *d = opendir(TL_PASTA);
    bool primeiro = true;
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (!tl_pasta_valida(e->d_name)) continue;
            char c[200];
            snprintf(c, sizeof c, TL_PASTA "/%s", e->d_name);
            int fotos = conta_fotos(c);
            struct stat st;
            long avi = 0;
            snprintf(c, sizeof c, TL_PASTA "/%s/timelapse.avi", e->d_name);
            if (stat(c, &st) == 0) avi = st.st_size;
            snprintf(c, sizeof c, TL_PASTA "/%s", e->d_name);
            long long data = stat(c, &st) == 0 ? (long long)st.st_mtime : 0;
            if (n + 300 > tam) {
                char *nj = realloc(j, tam * 2);
                if (!nj) break;
                j = nj;
                tam *= 2;
            }
            n += snprintf(j + n, tam - n, "%s{\"pasta\":\"%s\",\"fotos\":%d,\"avi\":%ld,\"data\":%lld}",
                          primeiro ? "" : ",", e->d_name, fotos, avi, data);
            primeiro = false;
        }
        closedir(d);
    }
    snprintf(j + n, tam - n, "]");
    return j;
}

esp_err_t tl_apaga(const char *pasta)
{
    if (!tl_pasta_valida(pasta)) return ESP_ERR_INVALID_ARG;
    char c[200];
    snprintf(c, sizeof c, TL_PASTA "/%s", pasta);
    if (s_pasta[0] && !strcmp(s_pasta, c)) return ESP_ERR_INVALID_STATE;   // gravando
    DIR *d = opendir(c);
    if (!d) return ESP_ERR_NOT_FOUND;
    struct dirent *e;
    char a[300];
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        snprintf(a, sizeof a, "%s/%s", c, e->d_name);
        remove(a);
    }
    closedir(d);
    return rmdir(c) == 0 ? ESP_OK : ESP_FAIL;
}
