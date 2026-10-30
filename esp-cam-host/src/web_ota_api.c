// Atualizacao do firmware pela pagina (OTA): POST /api/ota com o firmware.bin.
// Grava na particao OTA livre, confere a imagem e reinicia nela. Se a imagem
// nova nao subir, o bootloader continua na antiga (a troca so vale depois de
// gravada e conferida).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "impressao.h"
#include "web_interno.h"

static const char *TAG = "ota";

static esp_err_t erro(httpd_req_t *req, const char *status, const char *msg)
{
    char j[160];
    snprintf(j, sizeof j, "{\"erro\":\"%s\"}", msg);
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, j);
}

static void reinicia(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(1500));    // deixa a resposta chegar ao navegador
    esp_restart();
}

static esp_err_t ota(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    imp_info_t i;
    imp_info(&i);
    if (imp_ativa() || i.tft) return erro(req, "409 Conflict", "ha uma impressao em andamento");
    const esp_partition_t *destino = esp_ota_get_next_update_partition(NULL);
    if (!destino) return erro(req, "500 Internal Server Error", "sem particao OTA");
    if (req->content_len < 100000 || req->content_len > destino->size)
        return erro(req, "400 Bad Request", "arquivo de tamanho invalido (use .pio/build/esp32s3cam/firmware.bin)");

    esp_ota_handle_t h;
    if (esp_ota_begin(destino, req->content_len, &h) != ESP_OK)
        return erro(req, "500 Internal Server Error", "nao iniciou a gravacao");
    char *buf = malloc(4096);
    size_t falta = req->content_len;
    bool ok = buf != NULL;
    ESP_LOGW(TAG, "recebendo firmware: %u bytes para %s", (unsigned)falta, destino->label);
    while (ok && falta) {
        int r = httpd_req_recv(req, buf, falta < 4096 ? falta : 4096);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) ok = false;
        else {
            ok = esp_ota_write(h, buf, r) == ESP_OK;
            falta -= r;
        }
    }
    free(buf);
    // esp_ota_end confere o cabecalho e o hash da imagem
    if (!ok || esp_ota_end(h) != ESP_OK) {
        if (ok) ESP_LOGE(TAG, "imagem invalida");
        else esp_ota_abort(h);
        return erro(req, "400 Bad Request", ok ? "imagem invalida" : "envio interrompido");
    }
    if (esp_ota_set_boot_partition(destino) != ESP_OK)
        return erro(req, "500 Internal Server Error", "nao trocou a particao de boot");
    ESP_LOGW(TAG, "firmware gravado; reiniciando em %s", destino->label);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"reiniciando\":true}");
    xTaskCreate(reinicia, "reinicia", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static esp_err_t versao(httpd_req_t *req)
{
    if (!web_autorizado(req)) return ESP_OK;
    const esp_app_desc_t *d = esp_app_get_description();
    const esp_partition_t *p = esp_ota_get_running_partition();
    char j[200];
    snprintf(j, sizeof j, "{\"compilado\":\"%s %s\",\"idf\":\"%s\",\"particao\":\"%s\"}", d->date, d->time,
             d->idf_ver, p ? p->label : "?");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, j);
}

void web_rotas_ota(httpd_handle_t s)
{
    const httpd_uri_t r1 = {.uri = "/api/ota", .method = HTTP_POST, .handler = ota};
    const httpd_uri_t r2 = {.uri = "/api/firmware", .method = HTTP_GET, .handler = versao};
    httpd_register_uri_handler(s, &r1);
    httpd_register_uri_handler(s, &r2);
}
