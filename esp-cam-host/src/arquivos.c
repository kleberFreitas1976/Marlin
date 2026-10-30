#include "arquivos.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "cartao_sd.h"
#include "esp_log.h"
#include "mbedtls/base64.h"

static const char *TAG = "arquivos";

struct arq_envio {
    FILE *f;
    char nome[ARQ_NOME_MAX + 1];
    char linha[512];            // linha em montagem (so para ler os dados)
    size_t n_linha;
    size_t bytes;
    // miniatura em andamento
    FILE *mini;
    bool mini_jpg;
    int mini_area, melhor_area;
    bool melhor_jpg;
    unsigned char b64[4];       // sobra de base64 entre linhas
    int n_b64;
    // dados
    int tempo_s;                // estimativa do fatiador
    int camadas;                // ;LAYER_CHANGE contados
    float filamento_g, filamento_mm, altura_camada;
    char material[24];
    int temp_bico, temp_mesa;
};

bool arq_nome_valido(const char *nome)
{
    size_t n = strlen(nome);
    if (!n || n > ARQ_NOME_MAX || nome[0] == '.') return false;
    for (const char *c = nome; *c; c++)
        if (*c == '/' || *c == '\\' || *c == ':' || *c == '*' || *c == '?' || *c == '"' ||
            *c == '<' || *c == '>' || *c == '|' || (unsigned char)*c < 0x20)
            return false;
    const char *p = strrchr(nome, '.');
    return p && (!strcasecmp(p, ".gcode") || !strcasecmp(p, ".gco") || !strcasecmp(p, ".g"));
}

void arq_caminho(const char *nome, char *saida, size_t tam)
{
    snprintf(saida, tam, ARQ_PASTA "/%s", nome);
}

static void caminho_meta(const char *nome, const char *ext, char *saida, size_t tam)
{
    snprintf(saida, tam, ARQ_META "/%s%s", nome, ext);
}

// ------------------------------------------------------------------ leitura das linhas
// "1h 32m 10s", "2d 3h 4m 5s", "45m 3s"
static int le_duracao(const char *t)
{
    int total = 0;
    while (*t) {
        char *fim;
        long v = strtol(t, &fim, 10);
        if (fim == t) { t++; continue; }
        switch (*fim) {
        case 'd': total += v * 86400; break;
        case 'h': total += v * 3600; break;
        case 'm': total += v * 60; break;
        case 's': total += v; break;
        default: break;
        }
        t = *fim ? fim + 1 : fim;
    }
    return total;
}

static const char *valor_apos(const char *l, const char *chave)
{
    const char *p = strstr(l, chave);
    if (!p) return NULL;
    p += strlen(chave);
    while (*p == ' ' || *p == '=' || *p == ':') p++;
    return p;
}

static void mini_fecha(arq_envio_t *e, bool vale)
{
    if (!e->mini) return;
    fclose(e->mini);
    e->mini = NULL;
    char tmp[160], fim[160];
    caminho_meta(e->nome, ".mini.tmp", tmp, sizeof tmp);
    if (vale && e->mini_area > e->melhor_area) {      // guarda so a maior
        caminho_meta(e->nome, ".png", fim, sizeof fim);
        remove(fim);
        caminho_meta(e->nome, ".jpg", fim, sizeof fim);
        remove(fim);
        caminho_meta(e->nome, e->mini_jpg ? ".jpg" : ".png", fim, sizeof fim);
        rename(tmp, fim);
        e->melhor_area = e->mini_area;
        e->melhor_jpg = e->mini_jpg;
    } else {
        remove(tmp);
    }
}

static void linha_completa(arq_envio_t *e, char *l)
{
    if (l[0] != ';') return;
    if (e->mini) {
        if (strstr(l, "thumbnail") && strstr(l, " end")) {
            mini_fecha(e, true);
            return;
        }
        // "; iVBORw0KGgo..." -> bytes (base64 por linha, com sobra entre linhas)
        const char *p = l + 1;
        while (*p == ' ') p++;
        unsigned char tudo[600];
        size_t n = 0;
        for (int i = 0; i < e->n_b64; i++) tudo[n++] = e->b64[i];
        for (; *p && n < sizeof tudo; p++)
            if (isalnum((unsigned char)*p) || *p == '+' || *p == '/' || *p == '=') tudo[n++] = *p;
        size_t usar = n - n % 4;
        e->n_b64 = n - usar;
        memcpy(e->b64, tudo + usar, e->n_b64);
        unsigned char dec[450];
        size_t out = 0;
        if (usar && mbedtls_base64_decode(dec, sizeof dec, &out, tudo, usar) == 0)
            fwrite(dec, 1, out, e->mini);
        return;
    }
    // "; thumbnail begin 300x300 12345", "; thumbnail_JPG begin ...", "; thumbnail_QOI ..."
    const char *tb = strstr(l, "thumbnail");
    if (tb && strstr(tb, " begin")) {
        if (strstr(tb, "QOI")) return;             // o navegador nao mostra QOI
        int w = 0, h = 0;
        const char *d = strstr(tb, "begin");
        sscanf(d + 5, " %dx%d", &w, &h);
        char tmp[160];
        caminho_meta(e->nome, ".mini.tmp", tmp, sizeof tmp);
        e->mini = fopen(tmp, "wb");
        e->mini_jpg = strstr(tb, "JPG") != NULL;
        e->mini_area = w * h;
        e->n_b64 = 0;
        return;
    }
    const char *v;
    if (strstr(l, "LAYER_CHANGE")) e->camadas++;
    else if ((v = valor_apos(l, "estimated printing time (normal mode)")) != NULL) e->tempo_s = le_duracao(v);
    else if ((v = valor_apos(l, "filament used [g]")) != NULL) e->filamento_g = strtof(v, NULL);
    else if ((v = valor_apos(l, "filament used [mm]")) != NULL) e->filamento_mm = strtof(v, NULL);
    else if ((v = valor_apos(l, "; layer_height")) != NULL) e->altura_camada = strtof(v, NULL);
    else if ((v = valor_apos(l, "; filament_type")) != NULL && !e->material[0]) {
        size_t k = 0;
        while (v[k] && v[k] != ';' && v[k] != ',' && k < sizeof e->material - 1) k++;
        memcpy(e->material, v, k);
        e->material[k] = '\0';
    } else if ((v = valor_apos(l, "; nozzle_temperature_initial_layer")) != NULL && !e->temp_bico)
        e->temp_bico = atoi(v);
    else if ((v = valor_apos(l, "; first_layer_temperature")) != NULL && !e->temp_bico)
        e->temp_bico = atoi(v);
    else if ((v = valor_apos(l, "; first_layer_bed_temperature")) != NULL && !e->temp_mesa)
        e->temp_mesa = atoi(v);
    else if ((v = valor_apos(l, "; hot_plate_temp_initial_layer")) != NULL && !e->temp_mesa)
        e->temp_mesa = atoi(v);
}

// ------------------------------------------------------------------ envio
arq_envio_t *arq_envio_abre(const char *nome)
{
    if (!sd_ok() || !arq_nome_valido(nome)) return NULL;
    mkdir(ARQ_PASTA, 0775);
    mkdir(ARQ_META, 0775);
    arq_envio_t *e = calloc(1, sizeof *e);
    if (!e) return NULL;
    strlcpy(e->nome, nome, sizeof e->nome);
    e->tempo_s = -1;
    char c[160];
    arq_caminho(nome, c, sizeof c);
    strlcat(c, ".parte", sizeof c);
    e->f = fopen(c, "wb");
    if (!e->f) {
        free(e);
        return NULL;
    }
    setvbuf(e->f, NULL, _IOFBF, 16 * 1024);
    return e;
}

esp_err_t arq_envio_dados(arq_envio_t *e, const uint8_t *dados, size_t n)
{
    if (fwrite(dados, 1, n, e->f) != n) return ESP_FAIL;
    e->bytes += n;
    for (size_t i = 0; i < n; i++) {
        char c = (char)dados[i];
        if (c == '\n' || c == '\r') {
            if (e->n_linha) {
                e->linha[e->n_linha] = '\0';
                linha_completa(e, e->linha);
                e->n_linha = 0;
            }
        } else if (e->n_linha < sizeof e->linha - 1) {
            e->linha[e->n_linha++] = c;
        }
    }
    return ESP_OK;
}

esp_err_t arq_envio_fecha(arq_envio_t *e, bool ok)
{
    if (!e) return ESP_ERR_INVALID_ARG;
    if (e->n_linha) {
        e->linha[e->n_linha] = '\0';
        linha_completa(e, e->linha);
    }
    mini_fecha(e, false);
    bool gravou = fclose(e->f) == 0 && ok;
    char parte[180], fim[160];
    arq_caminho(e->nome, fim, sizeof fim);
    snprintf(parte, sizeof parte, "%s.parte", fim);
    if (!gravou) {
        remove(parte);
        free(e);
        return ESP_FAIL;
    }
    remove(fim);
    rename(parte, fim);

    char meta[160];
    caminho_meta(e->nome, ".json", meta, sizeof meta);
    FILE *m = fopen(meta, "w");
    if (m) {
        fprintf(m, "{\"tempo\":%d,\"camadas\":%d,\"filamentoG\":%.2f,\"filamentoMm\":%.0f,"
                   "\"altura\":%.2f,\"material\":\"%s\",\"bico\":%d,\"mesa\":%d,"
                   "\"miniatura\":\"%s\"}",
                e->tempo_s, e->camadas, e->filamento_g, e->filamento_mm, e->altura_camada,
                e->material, e->temp_bico, e->temp_mesa,
                e->melhor_area ? (e->melhor_jpg ? "jpg" : "png") : "");
        fclose(m);
    }
    ESP_LOGI(TAG, "recebido %s: %u bytes, %d camadas, %d s estimados%s", e->nome,
             (unsigned)e->bytes, e->camadas, e->tempo_s, e->melhor_area ? ", com miniatura" : "");
    free(e);
    return ESP_OK;
}

// ------------------------------------------------------------------ lista
static void anexa(char **buf, size_t *tam, size_t *n, const char *txt)
{
    size_t k = strlen(txt);
    if (*n + k + 1 > *tam) {
        size_t novo = (*tam + k) * 2;
        char *b = realloc(*buf, novo);
        if (!b) return;
        *buf = b;
        *tam = novo;
    }
    memcpy(*buf + *n, txt, k + 1);
    *n += k;
}

char *arq_lista_json(void)
{
    size_t tam = 2048, n = 0;
    char *buf = malloc(tam);
    if (!buf) return NULL;
    buf[0] = '\0';
    anexa(&buf, &tam, &n, "[");
    DIR *d = opendir(ARQ_PASTA);
    bool primeiro = true;
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (!arq_nome_valido(ent->d_name)) continue;
            char c[200], meta[200], item[1200], nome_json[ARQ_NOME_MAX * 2 + 1];
            arq_caminho(ent->d_name, c, sizeof c);
            struct stat st;
            if (stat(c, &st) != 0) continue;
            size_t q = 0;
            for (const char *p = ent->d_name; *p && q < sizeof nome_json - 2; p++) {
                if (*p == '"' || *p == '\\') nome_json[q++] = '\\';
                nome_json[q++] = *p;
            }
            nome_json[q] = '\0';
            char dados[600] = "{}";
            caminho_meta(ent->d_name, ".json", meta, sizeof meta);
            FILE *m = fopen(meta, "r");
            if (m) {
                size_t k = fread(dados, 1, sizeof dados - 1, m);
                dados[k] = '\0';
                fclose(m);
            }
            snprintf(item, sizeof item, "%s{\"nome\":\"%s\",\"tam\":%ld,\"data\":%lld,\"meta\":%s}",
                     primeiro ? "" : ",", nome_json, (long)st.st_size, (long long)st.st_mtime, dados);
            anexa(&buf, &tam, &n, item);
            primeiro = false;
        }
        closedir(d);
    }
    anexa(&buf, &tam, &n, "]");
    return buf;
}

esp_err_t arq_apaga(const char *nome)
{
    if (!arq_nome_valido(nome)) return ESP_ERR_INVALID_ARG;
    char c[200];
    arq_caminho(nome, c, sizeof c);
    if (remove(c) != 0) return ESP_ERR_NOT_FOUND;
    static const char *const EXT[] = {".json", ".png", ".jpg"};
    for (size_t i = 0; i < sizeof EXT / sizeof EXT[0]; i++) {
        caminho_meta(nome, EXT[i], c, sizeof c);
        remove(c);
    }
    return ESP_OK;
}

bool arq_miniatura(const char *nome, char *saida, size_t tam, const char **tipo)
{
    struct stat st;
    caminho_meta(nome, ".png", saida, tam);
    if (stat(saida, &st) == 0) {
        *tipo = "image/png";
        return true;
    }
    caminho_meta(nome, ".jpg", saida, tam);
    if (stat(saida, &st) == 0) {
        *tipo = "image/jpeg";
        return true;
    }
    return false;
}

int arq_tempo_estimado(const char *nome)
{
    char meta[200], dados[600];
    caminho_meta(nome, ".json", meta, sizeof meta);
    FILE *m = fopen(meta, "r");
    if (!m) return -1;
    size_t k = fread(dados, 1, sizeof dados - 1, m);
    fclose(m);
    dados[k] = '\0';
    const char *p = strstr(dados, "\"tempo\":");
    return p ? atoi(p + 8) : -1;
}
