// Pinos da ESP32-S3-CAM (desenho Freenove ESP32-S3-WROOM CAM).
// Ver ESPECIFICACAO.md, secao 2. GPIO 35-37 sao da PSRAM octal: nao usar.
#pragma once

// Camera (conector de 24 vias)
#define CAM_PIN_PWDN   -1
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK   15
#define CAM_PIN_SDA    4
#define CAM_PIN_SCL    5
#define CAM_PIN_D0     11
#define CAM_PIN_D1     9
#define CAM_PIN_D2     8
#define CAM_PIN_D3     10
#define CAM_PIN_D4     12
#define CAM_PIN_D5     18
#define CAM_PIN_D6     17
#define CAM_PIN_D7     16
#define CAM_PIN_VSYNC  6
#define CAM_PIN_HREF   7
#define CAM_PIN_PCLK   13

// Cartao SD (SDMMC, 1 bit)
#define SD_PIN_CMD     38
#define SD_PIN_CLK     39
#define SD_PIN_D0      40

// LEDs da placa
#define LED_PIN        2
#define LED_RGB_PIN    48

// Reles e iluminacao (fase 7)
#define RELE_IMPRESSORA_PIN  21
#define RELE_TUDO_PIN        47
#define LUZ_PIN              14
