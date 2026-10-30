#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

// Serial da TinyBee pela USB OTG (a placa e o host; a TinyBee tem um CH340).
//
// REGRA FIXA: DTR e RTS nunca sao acionados. No CH340 da TinyBee eles resetam o
// ESP32 do Marlin, e isso mata a impressao em andamento. O driver nao mexe neles
// ao abrir a porta, e este modulo nunca chama cdc_acm_host_set_control_line_state.

#define USB_SERIAL_BAUD 250000     // BAUDRATE do Marlin

// Chamada para cada bloco recebido, na tarefa do driver USB: deve ser rapida.
typedef void (*usb_serial_rx_t)(const uint8_t *dados, size_t n);
// Chamada quando a TinyBee conecta (true) ou desconecta (false).
typedef void (*usb_serial_estado_t)(bool conectado);

esp_err_t usb_serial_inicia(usb_serial_rx_t rx, usb_serial_estado_t estado);
bool usb_serial_conectado(void);

// Envia bytes. Bloqueia ate 1 s; erro se a TinyBee nao estiver conectada.
esp_err_t usb_serial_envia(const void *dados, size_t n);
