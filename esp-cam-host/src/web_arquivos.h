#pragma once
#include <stddef.h>
#include <stdint.h>

// Interface web embutida (gerada por tools/gera_web.py em web_arquivos.c)
typedef struct {
    const char *caminho;     // "/index.html"
    const char *tipo;        // Content-Type
    const uint8_t *dados;    // conteudo comprimido com gzip
    size_t tamanho;
    const char *etag;        // entre aspas, para If-None-Match
} web_arquivo_t;

extern const web_arquivo_t WEB_ARQUIVOS[];
extern const size_t WEB_N_ARQUIVOS;
