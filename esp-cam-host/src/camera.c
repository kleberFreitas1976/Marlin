#include "camera.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "pinos.h"

static const char *TAG = "camera";

static bool s_ok, s_ligada, s_manter;
static const char *s_nome = "nenhum";
static int s_pid;
static int s_larg_max, s_alt_max;
static framesize_t s_maximo = FRAMESIZE_SVGA;
static framesize_t s_tam = FRAMESIZE_SVGA;   // resolucao das fotos e do video
static int s_usuarios;
static int64_t s_ultimo_uso;
static SemaphoreHandle_t s_mtx;

static camera_config_t s_cfg = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SDA,
    .pin_sccb_scl = CAM_PIN_SCL,
    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_SVGA,
    .jpeg_quality = 12,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,
};

static void descarta(int n)
{
    for (int i = 0; i < n; i++) {
        camera_fb_t *v = esp_camera_fb_get();
        if (v) esp_camera_fb_return(v);
    }
}

// Chamar com s_mtx tomado.
static esp_err_t liga(void)
{
    if (s_ligada) return ESP_OK;
    // O tamanho dos buffers sai da resolucao do esp_camera_init: inicia ja na
    // maxima (buffers na PSRAM) e desce para a resolucao de trabalho.
    s_cfg.frame_size = s_maximo;
    esp_err_t err = esp_camera_init(&s_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "camera nao ligou (0x%x)", err);
        return err;
    }
    sensor_t *s = esp_camera_sensor_get();
    s->set_framesize(s, s_tam);
    // exposicao e balanco de branco automaticos precisam de alguns quadros
    vTaskDelay(pdMS_TO_TICKS(1200));
    descarta(4);
    s_ligada = true;
    ESP_LOGI(TAG, "ligada");
    return ESP_OK;
}

static void desliga(void)
{
    if (!s_ligada) return;
    esp_camera_deinit();
    s_ligada = false;
    ESP_LOGI(TAG, "desligada (sem uso ha %d s)", CAMERA_DESLIGA_S);
}

static void vigia(void *arg)
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        if (s_ligada && !s_usuarios && !s_manter &&
            esp_timer_get_time() - s_ultimo_uso > CAMERA_DESLIGA_S * 1000000LL)
            desliga();
        xSemaphoreGive(s_mtx);
    }
}

esp_err_t camera_inicia(void)
{
    s_mtx = xSemaphoreCreateMutex();
    esp_err_t err = esp_camera_init(&s_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "camera nao iniciou (0x%x). Confira a fita no conector e a trava.", err);
        return err;
    }
    sensor_t *s = esp_camera_sensor_get();
    s_pid = s->id.PID;
    camera_sensor_info_t *info = esp_camera_sensor_get_info(&s->id);
    if (info) {
        s_nome = info->name;
        s_maximo = info->max_size;
        s_larg_max = resolution[s_maximo].width;
        s_alt_max = resolution[s_maximo].height;
    } else {
        s_nome = "desconhecido";
    }
    esp_camera_deinit();          // so liga quando alguem usar
    ESP_LOGI(TAG, "sensor %s (PID 0x%04x), resolucao maxima %dx%d; desligada ate o primeiro uso",
             s_nome, s_pid, s_larg_max, s_alt_max);
    s_ok = true;
    xTaskCreate(vigia, "camera_vigia", 3072, NULL, 2, NULL);
    return ESP_OK;
}

bool camera_ok(void) { return s_ok; }
bool camera_ligada(void) { return s_ligada; }
const char *camera_nome(void) { return s_nome; }
int camera_pid(void) { return s_pid; }
int camera_largura_max(void) { return s_larg_max; }
int camera_altura_max(void) { return s_alt_max; }

esp_err_t camera_resolucao(framesize_t tam)
{
    if (!s_ok) return ESP_ERR_INVALID_STATE;
    if (resolution[tam].width > s_larg_max || resolution[tam].height > s_alt_max)
        return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_tam = tam;
    esp_err_t r = ESP_OK;
    if (s_ligada) {
        sensor_t *s = esp_camera_sensor_get();
        r = s->set_framesize(s, tam) == 0 ? ESP_OK : ESP_FAIL;
    }
    xSemaphoreGive(s_mtx);
    return r;
}

void camera_manter_ligada(bool sim)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_manter = sim;
    s_ultimo_uso = esp_timer_get_time();
    if (sim && s_ok) liga();
    xSemaphoreGive(s_mtx);
}

esp_err_t camera_usa(void)
{
    if (!s_ok) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    esp_err_t r = liga();
    if (r == ESP_OK) s_usuarios++;
    s_ultimo_uso = esp_timer_get_time();
    xSemaphoreGive(s_mtx);
    return r;
}

void camera_solta(void)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (s_usuarios > 0) s_usuarios--;
    s_ultimo_uso = esp_timer_get_time();
    xSemaphoreGive(s_mtx);
}

camera_fb_t *camera_foto(void)
{
    if (camera_usa() != ESP_OK) return NULL;
    descarta(2);       // os dois buffers podem guardar quadros de antes do pedido
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) camera_solta();
    return fb;
}

void camera_devolve(camera_fb_t *fb)
{
    if (!fb) return;
    esp_camera_fb_return(fb);
    camera_solta();
}
