// Wi-Fi: rede de casa (STA) com AP de configuracao quando ela falta.
//
// O AP roda junto com a STA (modo APSTA): enquanto o AP esta ligado a placa
// continua tentando a rede de casa, so que mais devagar, porque cada tentativa
// troca de canal e derruba quem esta conectado no AP configurando.

#include "rede.h"

#include <stdio.h>
#include <string.h>

#include "dns_cativo.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "segredos.h"

static const char *TAG = "rede";

static esp_netif_t *s_sta, *s_ap;
static bool s_conectado, s_ap_ativo;
static char s_ip[16];
static char s_ssid[33];
static char s_senha[65];
static esp_timer_handle_t s_t_espera, s_t_ap_sobra, s_t_reconecta;

// ------------------------------------------------------------------ NVS
static void le_credenciais(void)
{
    nvs_handle_t h;
    size_t n;
    s_ssid[0] = s_senha[0] = '\0';
    if (nvs_open("rede", NVS_READONLY, &h) == ESP_OK) {
        n = sizeof s_ssid;
        nvs_get_str(h, "ssid", s_ssid, &n);
        n = sizeof s_senha;
        nvs_get_str(h, "senha", s_senha, &n);
        nvs_close(h);
    }
    if (s_ssid[0] == '\0' && WIFI_SSID[0] != '\0') {   // primeira vez: secrets.ini
        strlcpy(s_ssid, WIFI_SSID, sizeof s_ssid);
        strlcpy(s_senha, WIFI_SENHA, sizeof s_senha);
    }
}

static esp_err_t grava_credenciais(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("rede", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_set_str(h, "ssid", s_ssid);
    nvs_set_str(h, "senha", s_senha);
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

// ------------------------------------------------------------------ AP
static void liga_ap(void)
{
    if (s_ap_ativo) return;
    wifi_config_t ap = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = sizeof(AP_SSID) - 1,
            .password = AP_SENHA,
            .channel = 6,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    s_ap_ativo = true;
    dns_cativo_inicia();
    ESP_LOGW(TAG, "AP de configuracao ligado: rede \"%s\", senha \"%s\", pagina http://%s/wifi",
             AP_SSID, AP_SENHA, AP_IP);
}

static void desliga_ap(void)
{
    if (!s_ap_ativo) return;
    dns_cativo_para();
    esp_wifi_set_mode(WIFI_MODE_STA);
    s_ap_ativo = false;
    ESP_LOGI(TAG, "AP de configuracao desligado");
}

// ------------------------------------------------------------------ timers
static void tenta_conectar(void)
{
    if (s_ssid[0]) esp_wifi_connect();
}

static void cb_espera(void *arg)
{
    if (!s_conectado) {
        ESP_LOGW(TAG, "sem conexao com \"%s\" em %d s", s_ssid, REDE_ESPERA_S);
        liga_ap();
    }
}

static void cb_ap_sobra(void *arg)
{
    if (s_conectado) desliga_ap();
}

static void cb_reconecta(void *arg) { tenta_conectar(); }

// ------------------------------------------------------------------ eventos
static void evento(void *arg, esp_event_base_t base, int32_t id, void *dados)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        tenta_conectar();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_conectado) ESP_LOGW(TAG, "Wi-Fi caiu");
        s_conectado = false;
        s_ip[0] = '\0';
        // Com o AP ligado, tenta menos: cada tentativa atrapalha quem configura
        esp_timer_stop(s_t_reconecta);
        esp_timer_start_once(s_t_reconecta, (s_ap_ativo ? 30 : 5) * 1000000LL);
        if (!s_ap_ativo && !esp_timer_is_active(s_t_espera))
            esp_timer_start_once(s_t_espera, REDE_ESPERA_S * 1000000LL);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)dados;
        snprintf(s_ip, sizeof s_ip, IPSTR, IP2STR(&e->ip_info.ip));
        s_conectado = true;
        esp_timer_stop(s_t_espera);
        ESP_LOGI(TAG, "conectado a \"%s\": abra http://%s/", s_ssid, s_ip);
        if (s_ap_ativo) {
            esp_timer_stop(s_t_ap_sobra);
            esp_timer_start_once(s_t_ap_sobra, REDE_AP_SOBRA_S * 1000000LL);
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "aparelho conectou no AP");
    }
}

// ------------------------------------------------------------------ API
esp_err_t rede_inicia(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta = esp_netif_create_default_wifi_sta();
    s_ap = esp_netif_create_default_wifi_ap();
    esp_netif_set_hostname(s_sta, "impressora-cam");
    esp_netif_ip_info_t ip_ap;
    esp_netif_set_ip4_addr(&ip_ap.ip, AP_IP_BYTES);
    esp_netif_set_ip4_addr(&ip_ap.gw, AP_IP_BYTES);
    esp_netif_set_ip4_addr(&ip_ap.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(s_ap);
    esp_netif_set_ip_info(s_ap, &ip_ap);
    esp_netif_dhcps_start(s_ap);

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    esp_wifi_set_storage(WIFI_STORAGE_RAM);     // as credenciais ficam no nosso NVS
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, evento, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, evento, NULL);

    const esp_timer_create_args_t a1 = {.callback = cb_espera, .name = "rede_espera"};
    const esp_timer_create_args_t a2 = {.callback = cb_ap_sobra, .name = "rede_ap"};
    const esp_timer_create_args_t a3 = {.callback = cb_reconecta, .name = "rede_recon"};
    esp_timer_create(&a1, &s_t_espera);
    esp_timer_create(&a2, &s_t_ap_sobra);
    esp_timer_create(&a3, &s_t_reconecta);

    le_credenciais();
    wifi_config_t sta = {0};
    strlcpy((char *)sta.sta.ssid, s_ssid, sizeof sta.sta.ssid);
    strlcpy((char *)sta.sta.password, s_senha, sizeof sta.sta.password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    if (s_ssid[0] == '\0') {
        ESP_LOGW(TAG, "nenhuma rede configurada");
        liga_ap();
    } else {
        ESP_LOGI(TAG, "conectando a \"%s\"...", s_ssid);
        esp_timer_start_once(s_t_espera, REDE_ESPERA_S * 1000000LL);
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);       // video sem engasgos
    return ESP_OK;
}

esp_err_t rede_configura(const char *ssid, const char *senha)
{
    if (!ssid || !ssid[0] || strlen(ssid) > 32 || strlen(senha) > 64) return ESP_ERR_INVALID_ARG;
    strlcpy(s_ssid, ssid, sizeof s_ssid);
    strlcpy(s_senha, senha ? senha : "", sizeof s_senha);
    esp_err_t err = grava_credenciais();
    if (err != ESP_OK) return err;

    wifi_config_t sta = {0};
    strlcpy((char *)sta.sta.ssid, s_ssid, sizeof sta.sta.ssid);
    strlcpy((char *)sta.sta.password, s_senha, sizeof sta.sta.password);
    esp_timer_stop(s_t_reconecta);
    s_conectado = false;
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &sta);
    ESP_LOGI(TAG, "rede gravada: \"%s\"; conectando", s_ssid);
    esp_wifi_connect();
    return ESP_OK;
}

esp_err_t rede_procura(char *json, size_t tam)
{
    wifi_scan_config_t cfg = {.show_hidden = false};
    esp_err_t err = esp_wifi_scan_start(&cfg, true);   // bloqueia ~2 s
    if (err != ESP_OK) {
        snprintf(json, tam, "[]");
        return err;
    }
    uint16_t n = 20;
    wifi_ap_record_t lista[20];
    esp_wifi_scan_get_ap_records(&n, lista);

    size_t p = snprintf(json, tam, "[");
    for (int i = 0; i < n && p < tam - 120; i++) {
        if (!lista[i].ssid[0]) continue;
        bool repetida = false;            // a mesma rede em varios roteadores
        for (int k = 0; k < i; k++)
            if (!strcmp((char *)lista[k].ssid, (char *)lista[i].ssid)) repetida = true;
        if (repetida) continue;
        char nome[70];
        size_t q = 0;
        for (const char *c = (char *)lista[i].ssid; *c && q < sizeof nome - 3; c++) {
            if (*c == '"' || *c == '\\') nome[q++] = '\\';
            nome[q++] = *c;
        }
        nome[q] = '\0';
        p += snprintf(json + p, tam - p, "%s{\"ssid\":\"%s\",\"rssi\":%d,\"aberta\":%s}",
                      p > 1 ? "," : "", nome, lista[i].rssi,
                      lista[i].authmode == WIFI_AUTH_OPEN ? "true" : "false");
    }
    snprintf(json + p, tam - p, "]");
    return ESP_OK;
}

bool rede_conectado(void) { return s_conectado; }
bool rede_ap_ativo(void) { return s_ap_ativo; }
const char *rede_ip(void) { return s_ip; }
const char *rede_ssid(void) { return s_ssid; }

int rede_rssi(void)
{
    wifi_ap_record_t ap;
    if (!s_conectado || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return 0;
    return ap.rssi;
}
