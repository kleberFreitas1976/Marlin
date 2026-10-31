#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_camera.h"

// A camera so captura enquanto alguem usa (foto, video, time-lapse) e entra em
// standby sozinha CAMERA_DESLIGA_S depois do ultimo uso: o OV5640 esquenta
// bastante capturando sem parar, e o calor piora a imagem.
#define CAMERA_DESLIGA_S 20

// Detecta o sensor e deixa a camera desligada.
esp_err_t camera_inicia(void);
bool camera_ok(void);          // sensor detectado
bool camera_ligada(void);

// Nome e ID do sensor detectado (ex.: "OV5640", 0x5640)
const char *camera_nome(void);
int camera_pid(void);

// Maior resolucao do sensor, em pixels
int camera_largura_max(void);
int camera_altura_max(void);

// Resolucao das proximas fotos (FRAMESIZE_*). ESP_ERR_INVALID_ARG se o sensor nao suporta.
esp_err_t camera_resolucao(framesize_t tam);

// Imagem desvirada (camera de cabeca para baixo). Gravado na NVS; padrao: sim.
bool camera_girada(void);
esp_err_t camera_girar(bool sim);

// Mantem a camera ligada (time-lapse durante a impressao), sem esperar uso.
void camera_manter_ligada(bool sim);

// Uso continuo (video ao vivo): liga a camera e impede que desligue ate camera_solta().
esp_err_t camera_usa(void);
void camera_solta(void);

// Foto JPEG posterior ao pedido: liga a camera se preciso e descarta os quadros
// que ja estavam no buffer (importante no time-lapse). Devolver com camera_devolve().
camera_fb_t *camera_foto(void);
void camera_devolve(camera_fb_t *fb);
