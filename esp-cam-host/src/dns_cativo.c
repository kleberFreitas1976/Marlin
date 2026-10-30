#include "dns_cativo.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "rede.h"

static const char *TAG = "dns";
static TaskHandle_t s_tarefa;
static volatile bool s_rodando;

static void servidor(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in end = {.sin_family = AF_INET, .sin_port = htons(53),
                              .sin_addr.s_addr = htonl(INADDR_ANY)};
    struct timeval tv = {.tv_sec = 1};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    if (bind(sock, (struct sockaddr *)&end, sizeof end) < 0) {
        ESP_LOGE(TAG, "porta 53 ocupada");
        close(sock);
        s_tarefa = NULL;
        vTaskDelete(NULL);
    }
    uint8_t buf[512];
    while (s_rodando) {
        struct sockaddr_in de;
        socklen_t tam = sizeof de;
        int n = recvfrom(sock, buf, sizeof buf - 16, 0, (struct sockaddr *)&de, &tam);
        if (n < 12) continue;
        // so perguntas (QR=0) com uma pergunta
        if ((buf[2] & 0x80) || buf[5] != 1) continue;
        // pula o nome pedido ate o fim da pergunta (nome + tipo + classe)
        int p = 12;
        while (p < n && buf[p]) p += buf[p] + 1;
        p += 5;
        if (p > n) continue;
        buf[2] = 0x81;          // resposta, recursao desejada
        buf[3] = 0x80;          // recursao disponivel, sem erro
        buf[7] = 1;             // 1 resposta
        buf[8] = buf[9] = buf[10] = buf[11] = 0;
        // resposta: ponteiro para o nome, tipo A, classe IN, TTL 60, 4 bytes de IP
        const uint8_t resp[] = {0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4, AP_IP_BYTES};
        memcpy(buf + p, resp, sizeof resp);
        sendto(sock, buf, p + sizeof resp, 0, (struct sockaddr *)&de, tam);
    }
    close(sock);
    s_tarefa = NULL;
    vTaskDelete(NULL);
}

void dns_cativo_inicia(void)
{
    if (s_tarefa) return;
    s_rodando = true;
    xTaskCreate(servidor, "dns_cativo", 3072, NULL, 3, &s_tarefa);
}

void dns_cativo_para(void) { s_rodando = false; }
