#include "usb_serial.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "usb/vcp_ch34x.h"

static const char *TAG = "usb";

static usb_serial_rx_t s_rx;
static usb_serial_estado_t s_estado;
static cdc_acm_dev_hdl_t s_hdl;
static SemaphoreHandle_t s_caiu;
static SemaphoreHandle_t s_envio;

// Eventos da biblioteca USB host (enumeracao, desconexao...)
static void tarefa_usb(void *arg)
{
    while (true) {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
    }
}

static bool recebe(const uint8_t *dados, size_t n, void *arg)
{
    if (s_rx) s_rx(dados, n);
    return true;          // dados consumidos
}

static void evento(const cdc_acm_host_dev_event_data_t *e, void *arg)
{
    switch (e->type) {
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGW(TAG, "TinyBee desconectada");
        xSemaphoreTake(s_envio, portMAX_DELAY);
        s_hdl = NULL;
        xSemaphoreGive(s_envio);
        cdc_acm_host_close(e->data.cdc_hdl);
        xSemaphoreGive(s_caiu);
        break;
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "erro no CH340: %d", e->data.error);
        break;
    default:
        break;
    }
}

// Abre a porta assim que a TinyBee aparece; volta a esperar quando ela some.
static void tarefa_conexao(void *arg)
{
    while (true) {
        const cdc_acm_host_device_config_t cfg = {
            .connection_timeout_ms = 5000,
            .out_buffer_size = 512,
            .in_buffer_size = 512,
            .event_cb = evento,
            .data_cb = recebe,
            .user_arg = NULL,
        };
        cdc_acm_dev_hdl_t hdl = NULL;
        if (ch34x_vcp_open(CH34X_PID_AUTO, 0, &cfg, &hdl) != ESP_OK) continue;  // tenta de novo

        // So a velocidade e o formato. DTR/RTS ficam como estao (desligados).
        cdc_acm_line_coding_t lc = {
            .dwDTERate = USB_SERIAL_BAUD,
            .bCharFormat = 0,     // 1 bit de parada
            .bParityType = 0,     // sem paridade
            .bDataBits = 8,
        };
        esp_err_t err = cdc_acm_host_line_coding_set(hdl, &lc);
        if (err != ESP_OK) ESP_LOGE(TAG, "nao ajustou %d baud (0x%x)", USB_SERIAL_BAUD, err);

        xSemaphoreTake(s_envio, portMAX_DELAY);
        s_hdl = hdl;
        xSemaphoreGive(s_envio);
        ESP_LOGI(TAG, "TinyBee conectada (CH340, %d baud)", USB_SERIAL_BAUD);
        if (s_estado) s_estado(true);

        xSemaphoreTake(s_caiu, portMAX_DELAY);     // ate desconectar
        if (s_estado) s_estado(false);
    }
}

esp_err_t usb_serial_inicia(usb_serial_rx_t rx, usb_serial_estado_t estado)
{
    s_rx = rx;
    s_estado = estado;
    s_caiu = xSemaphoreCreateBinary();
    s_envio = xSemaphoreCreateMutex();

    const usb_host_config_t host = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t err = usb_host_install(&host);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "USB host nao instalou (0x%x)", err);
        return err;
    }
    xTaskCreatePinnedToCore(tarefa_usb, "usb_host", 4096, NULL, 10, NULL, 1);

    const cdc_acm_host_driver_config_t drv = {
        .driver_task_stack_size = 4096,
        .driver_task_priority = 9,
        .xCoreID = 1,
        .new_dev_cb = NULL,
    };
    err = cdc_acm_host_install(&drv);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "driver CDC nao instalou (0x%x)", err);
        return err;
    }
    xTaskCreatePinnedToCore(tarefa_conexao, "usb_conexao", 4096, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "USB host pronto: aguardando a TinyBee na porta USB-OTG");
    return ESP_OK;
}

bool usb_serial_conectado(void) { return s_hdl != NULL; }

esp_err_t usb_serial_envia(const void *dados, size_t n)
{
    xSemaphoreTake(s_envio, portMAX_DELAY);
    esp_err_t err = s_hdl ? cdc_acm_host_data_tx_blocking(s_hdl, dados, n, 1000)
                          : ESP_ERR_INVALID_STATE;
    xSemaphoreGive(s_envio);
    return err;
}
