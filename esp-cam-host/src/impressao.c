#include "impressao.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "arquivos.h"
#include "cartao_sd.h"
#include "diario.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"

static const char *TAG = "impressao";

// Estacionamento na pausa e no cancelamento: fundo-esquerda, longe da camera
// (que fica na frente) e fora da peca na maioria dos casos.
#define PARK_X 5.0f
#define PARK_Y 176.0f
#define PARK_Z_SOBE 10.0f
#define RETRACAO 2.0f

static SemaphoreHandle_t s_mtx;
static imp_info_t s_i;
// Leitura antecipada: uma tarefa propria le o arquivo do SD para uma reserva na PSRAM,
// e quem alimenta a TinyBee (proxima, na tarefa do marlin.c) so le da memoria. Assim a
// impressao nunca espera o cartao: gravar uma foto do time-lapse ou receber um arquivo
// no mesmo SD seguraria a leitura por decimos de segundo, o Marlin esvaziaria o buffer
// de movimentos e o bico pararia sobre a peca (caroco).
#define RESERVA_BYTES (128 * 1024)   // ~2-3 min de impressao
#define LEITURA_BLOCO 4096
static StreamBufferHandle_t s_reserva;
static char s_caminho[200];          // para reabrir o arquivo se a leitura do SD falhar
static volatile bool s_erro_sd, s_fim_arquivo, s_leitor_parar, s_leitor_ativo;
static uint32_t s_lido;              // bytes do arquivo ja postos na reserva
static uint32_t s_reserva_vazia;     // vezes que a TinyBee teve de esperar o SD (diagnostico)
// montagem das linhas a partir da reserva (so a tarefa do marlin.c mexe)
static uint8_t s_ped[512];
static size_t s_ped_i, s_ped_n;
static char s_lin[256];
static size_t s_lin_n;
static bool s_lin_longa;
static int64_t s_inicio_us, s_pausado_us, s_pausa_desde_us;
static int64_t s_tft_inicio_us;
static imp_camada_cb_t s_cb_camada;
static imp_foto_cb_t s_cb_foto;
static imp_ciclo_cb_t s_cb_ciclo;

// Modo e ultima posicao pedidos pelo arquivo: a retomada volta exatamente ali
static bool s_rel_xyz, s_rel_e;
static float s_x, s_y, s_z, s_e, s_f;
static bool s_tem_xy, s_tem_z;

static void trava(void) { xSemaphoreTake(s_mtx, portMAX_DELAY); }
static void solta(void) { xSemaphoreGive(s_mtx); }

void imp_ouvintes(imp_camada_cb_t camada, imp_foto_cb_t foto, imp_ciclo_cb_t ciclo)
{
    s_cb_camada = camada;
    s_cb_foto = foto;
    s_cb_ciclo = ciclo;
}

bool imp_ativa(void)
{
    return s_i.estado == IMP_IMPRIMINDO || s_i.estado == IMP_PAUSANDO ||
           s_i.estado == IMP_PAUSADA || s_i.estado == IMP_CANCELANDO;
}

// ------------------------------------------------------------------ G-code do arquivo
static void acompanha(const char *l)
{
    if (!strncmp(l, "G90", 3)) s_rel_xyz = false;
    else if (!strncmp(l, "G91", 3)) s_rel_xyz = true;
    else if (!strncmp(l, "M82", 3)) s_rel_e = false;
    else if (!strncmp(l, "M83", 3)) s_rel_e = true;
    else if (!strncmp(l, "G92", 3)) {
        const char *p = strchr(l, 'E');
        if (p) s_e = strtof(p + 1, NULL);
    } else if (l[0] == 'G' && l[1] >= '0' && l[1] <= '3' && (l[2] == ' ' || l[2] == '\0')) {
        // G2/G3 (arc fitting do fatiador): X/Y e o fim do arco; I/J (centro) nao mudam a posicao
        for (const char *p = l + 2; *p; p++) {
            if (p[-1] != ' ') continue;
            float v = strtof(p + 1, NULL);
            switch (*p) {
            case 'X': s_x = s_rel_xyz ? s_x + v : v; s_tem_xy = true; break;
            case 'Y': s_y = s_rel_xyz ? s_y + v : v; s_tem_xy = true; break;
            case 'Z': s_z = s_rel_xyz ? s_z + v : v; s_tem_z = true; break;
            case 'E': s_e = s_rel_e ? s_e + v : v; break;
            case 'F': s_f = v; break;
            default: break;
            }
        }
    }
}

static int64_t s_fim_us = -1;     // esp_timer do ultimo termino

static void marca_fim(bool ok)
{
    s_fim_us = esp_timer_get_time();
    s_i.fim_ok = ok;
}

static void termina(imp_estado_t fim, const char *aviso)
{
    // chamada com a trava tomada
    s_leitor_parar = true;           // o leitor fecha o arquivo e termina sozinho
    marlin_fonte(NULL);
    s_i.estado = fim;
    strlcpy(s_i.aviso, aviso, sizeof s_i.aviso);
    s_i.decorrido_s = (int)((esp_timer_get_time() - s_inicio_us - s_pausado_us) / 1000000);
    s_i.restante_s = 0;
    marca_fim(fim == IMP_CONCLUIDA);
    diario_evento("=== %s: %s (camada %d de %d, %d min)", s_i.nome, aviso, s_i.camada, s_i.camadas,
                  s_i.decorrido_s / 60);
    ESP_LOGI(TAG, "%s: %s (a TinyBee esperou o SD %lu vezes)", s_i.nome, aviso,
             (unsigned long)s_reserva_vazia);
}

// Abre o arquivo na posicao pos. Uma falha de leitura do SD nao e fim de arquivo:
// visto numa impressao real, a leitura falhou no byte 49152 de 560047 e a placa deu a
// impressao por "concluida" na camada 2, com o bico e a mesa ligados.
static FILE *abre_em(uint32_t pos)
{
    FILE *f = fopen(s_caminho, "r");
    if (f && fseek(f, pos, SEEK_SET) != 0) {
        fclose(f);
        f = NULL;
    }
    if (f) setvbuf(f, NULL, _IONBF, 0);   // le em blocos proprios: sem buffer do stdio
    return f;
}

static void leitor(void *arg)
{
    FILE *f = (FILE *)arg;
    uint8_t *buf = heap_caps_malloc(LEITURA_BLOCO, MALLOC_CAP_SPIRAM);
    int falhas = 0;
    while (buf && !s_leitor_parar) {
        size_t n = f ? fread(buf, 1, LEITURA_BLOCO, f) : 0;
        if (n > 0) {
            falhas = 0;
            s_lido += n;
            size_t env = 0;
            while (env < n && !s_leitor_parar)
                env += xStreamBufferSend(s_reserva, buf + env, n - env, pdMS_TO_TICKS(100));
            continue;
        }
        if (f && feof(f) && !ferror(f) && s_lido >= s_i.total) {
            s_fim_arquivo = true;
            break;
        }
        // erro de leitura: reabre na mesma posicao e tenta de novo
        if (++falhas > 6) {
            ESP_LOGE(TAG, "SD: desisto de ler %s no byte %lu", s_caminho, (unsigned long)s_lido);
            s_erro_sd = true;
            break;
        }
        ESP_LOGE(TAG, "falha lendo o SD no byte %lu de %lu (tentativa %d): reabrindo",
                 (unsigned long)s_lido, (unsigned long)s_i.total, falhas);
        if (f) fclose(f);
        vTaskDelay(pdMS_TO_TICKS(300 * falhas));
        f = abre_em(s_lido);
    }
    if (!buf) s_erro_sd = true;
    if (f) fclose(f);
    free(buf);
    s_leitor_ativo = false;
    vTaskDelete(NULL);
}

// Proxima linha da reserva: 1 = linha em l; 0 = nada ainda (o leitor esta atras);
// -1 = fim do arquivo ou erro do SD. Linha maior que o buffer (o bloco de configuracao
// do fatiador no fim do arquivo tem comentarios de milhares de caracteres) e descartada
// inteira: um pedaco dela viraria uma "linha de G-code" de lixo.
static int pega_linha(char *l, size_t tam)
{
    while (true) {
        if (s_ped_i == s_ped_n) {
            s_ped_i = 0;
            s_ped_n = xStreamBufferReceive(s_reserva, s_ped, sizeof s_ped, 0);
            if (s_ped_n == 0) {
                if (s_erro_sd) return -1;
                if (s_fim_arquivo && xStreamBufferIsEmpty(s_reserva)) {
                    if (s_lin_n && !s_lin_longa) {     // ultima linha sem '\n'
                        s_lin[s_lin_n] = '\0';
                        strlcpy(l, s_lin, tam);
                        s_lin_n = 0;
                        return 1;
                    }
                    return -1;
                }
                return 0;
            }
        }
        char c = (char)s_ped[s_ped_i++];
        s_i.bytes++;
        if (c == '\n') {
            bool longa = s_lin_longa;
            s_lin[s_lin_n] = '\0';
            s_lin_n = 0;
            s_lin_longa = false;
            if (longa) continue;
            strlcpy(l, s_lin, tam);
            return 1;
        }
        if (s_lin_n < sizeof s_lin - 1) s_lin[s_lin_n++] = c;
        else s_lin_longa = true;
    }
}

// Fonte de linhas para o marlin.c (roda na tarefa dele)
static bool proxima(char *saida, size_t tam)
{
    bool camada_nova = false;
    int camada = 0, total = 0;
    bool ok = false;
    trava();
    while (s_i.estado == IMP_IMPRIMINDO) {
        char l[256];
        int r = pega_linha(l, sizeof l);
        if (r == 0) {                     // a reserva esvaziou: o SD esta atrasado
            s_reserva_vazia++;
            break;
        }
        if (r < 0) {
            if (s_erro_sd) {
                // como um cancelamento: espera a fila esvaziar, estaciona e desliga o aquecimento
                s_i.estado = IMP_CANCELANDO;
                strlcpy(s_i.aviso, "Falha lendo o cartao SD: parando", sizeof s_i.aviso);
            } else {
                termina(IMP_CONCLUIDA, "Impressao concluida");
            }
            break;
        }
        char *p = l;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';') {
            if (strstr(p, "LAYER_CHANGE")) {
                s_i.camada++;
                camada_nova = true;
                camada = s_i.camada;
                total = s_i.camadas;
            }
            continue;
        }
        char *c = strchr(p, ';');
        if (c) *c = '\0';
        size_t n = strcspn(p, "\r\n");
        while (n && (p[n - 1] == ' ' || p[n - 1] == '\t')) n--;
        p[n] = '\0';
        if (!n) continue;
        // Pausa pedida pelo arquivo (troca manual de filamento): M601 e o "Pause G-code" do
        // Orca; M600 vem da troca de cor do Orca. O Marlin desta placa nao tem a pausa
        // avancada (sem LCD nem EMERGENCY_PARSER), entao a pausa e a da ESP: termina os
        // movimentos ja enviados, estaciona mantendo o aquecimento e espera "Retomar".
        if ((p[0] == 'M' || p[0] == 'm') && (!strcmp(p + 1, "600") || !strcmp(p + 1, "601"))) {
            s_i.estado = IMP_PAUSANDO;
            strlcpy(s_i.aviso, "Pausa do arquivo (troca de filamento): estacionando", sizeof s_i.aviso);
            s_pausa_desde_us = esp_timer_get_time();
            diario_evento("pausa do arquivo %s (camada %d)", p, s_i.camada);
            break;
        }
        if (n >= tam) n = tam - 1;   // linha absurda: o Marlin recusaria de qualquer jeito
        memcpy(saida, p, n);
        saida[n] = '\0';
        acompanha(saida);
        diario_gcode(saida);
        s_i.linhas++;
        ok = true;
        break;
    }
    bool acabou = s_i.estado == IMP_CONCLUIDA;
    char nome[84];
    strlcpy(nome, s_i.nome, sizeof nome);
    solta();
    if (camada_nova && s_cb_camada) s_cb_camada(camada, total);
    if (acabou && !ok && s_cb_ciclo) s_cb_ciclo(false, nome);
    return ok;
}

// ------------------------------------------------------------------ comandos
esp_err_t imp_imprime(const char *nome, char *erro, size_t tam)
{
    marlin_estado_t m;
    marlin_estado(&m);
    char c[200];
    struct stat st;
    FILE *arq = NULL;
    // o leitor da impressao anterior ainda pode estar fechando o arquivo
    for (int i = 0; i < 40 && s_leitor_ativo; i++) vTaskDelay(pdMS_TO_TICKS(50));
    trava();
    const char *motivo = NULL;
    if (imp_ativa()) motivo = "ja ha uma impressao em andamento";
    else if (s_i.tft) motivo = "a TFT24 esta imprimindo";
    else if (!m.conectada || !m.respondendo) motivo = "a TinyBee nao esta respondendo";
    else if (!sd_ok()) motivo = "sem cartao SD";
    else if (!arq_nome_valido(nome)) motivo = "nome de arquivo invalido";
    else if (s_leitor_ativo) motivo = "a impressao anterior ainda esta fechando o arquivo";
    else if (!s_reserva) motivo = "sem memoria para a leitura antecipada";
    if (!motivo) {
        arq_caminho(nome, c, sizeof c);
        strlcpy(s_caminho, c, sizeof s_caminho);
        if (stat(c, &st) != 0 || !(arq = abre_em(0))) motivo = "arquivo nao encontrado";
    }
    if (motivo) {
        solta();
        strlcpy(erro, motivo, tam);
        return ESP_FAIL;
    }
    memset(&s_i, 0, sizeof s_i);
    s_erro_sd = s_fim_arquivo = s_leitor_parar = false;
    s_lido = 0;
    s_reserva_vazia = 0;
    s_ped_i = s_ped_n = s_lin_n = 0;
    s_lin_longa = false;
    xStreamBufferReset(s_reserva);
    strlcpy(s_i.nome, nome, sizeof s_i.nome);
    s_i.total = st.st_size;
    s_i.estimado_s = arq_tempo_estimado(nome);
    s_i.restante_s = s_i.estimado_s;
    // camadas: dos dados gravados na chegada do arquivo
    char meta[220];
    snprintf(meta, sizeof meta, ARQ_META "/%s.json", nome);
    FILE *f = fopen(meta, "r");
    if (f) {
        char d[600];
        size_t k = fread(d, 1, sizeof d - 1, f);
        d[k] = '\0';
        fclose(f);
        const char *p = strstr(d, "\"camadas\":");
        if (p) s_i.camadas = atoi(p + 10);
    }
    s_rel_xyz = false;
    s_rel_e = false;
    s_x = s_y = s_z = s_e = 0;
    s_f = 3000;
    s_tem_xy = s_tem_z = false;
    s_inicio_us = esp_timer_get_time();
    s_pausado_us = 0;
    s_i.estado = IMP_IMPRIMINDO;
    strlcpy(s_i.aviso, "Imprimindo", sizeof s_i.aviso);
    s_leitor_ativo = true;
    if (xTaskCreatePinnedToCore(leitor, "imp_leitor", 4096, arq, 4, NULL, 0) != pdPASS) {
        s_leitor_ativo = false;
        fclose(arq);
        s_i.estado = IMP_ERRO;
        strlcpy(s_i.aviso, "sem memoria para o leitor do arquivo", sizeof s_i.aviso);
        solta();
        strlcpy(erro, s_i.aviso, tam);
        return ESP_FAIL;
    }
    solta();
    ESP_LOGI(TAG, "imprimindo %s (%u bytes)", nome, (unsigned)st.st_size);
    diario_evento("=== impressao pela placa: %s (%u bytes, %d camadas)", nome, (unsigned)st.st_size, s_i.camadas);
    if (s_cb_ciclo) s_cb_ciclo(true, nome);
    marlin_fonte(proxima);
    return ESP_OK;
}

void imp_pausa(void)
{
    trava();
    if (s_i.estado == IMP_IMPRIMINDO) {
        s_i.estado = IMP_PAUSANDO;          // proxima() para de entregar linhas
        strlcpy(s_i.aviso, "Pausando: terminando os movimentos ja enviados", sizeof s_i.aviso);
        s_pausa_desde_us = esp_timer_get_time();
        diario_evento("pausa pedida (camada %d)", s_i.camada);
    }
    solta();
}

void imp_retoma(void)
{
    trava();
    if (s_i.estado != IMP_PAUSADA) {
        solta();
        return;
    }
    char l[80];
    marlin_envia("G90", false);
    if (s_tem_xy) {
        snprintf(l, sizeof l, "G1 X%.3f Y%.3f F6000", s_x, s_y);
        marlin_envia(l, false);
    }
    if (s_tem_z) {
        snprintf(l, sizeof l, "G1 Z%.3f F600", s_z);
        marlin_envia(l, false);
    }
    marlin_envia("M83", false);
    snprintf(l, sizeof l, "G1 E%.2f F2400", RETRACAO);
    marlin_envia(l, false);
    if (!s_rel_e) {                     // o arquivo usa E absoluto: devolve o contador
        marlin_envia("M82", false);
        snprintf(l, sizeof l, "G92 E%.4f", s_e);
        marlin_envia(l, false);
    }
    if (s_rel_xyz) marlin_envia("G91", false);
    snprintf(l, sizeof l, "G1 F%.0f", s_f);
    marlin_envia(l, false);
    s_pausado_us += esp_timer_get_time() - s_pausa_desde_us;
    s_i.estado = IMP_IMPRIMINDO;
    strlcpy(s_i.aviso, "Imprimindo", sizeof s_i.aviso);
    diario_evento("retomada (camada %d)", s_i.camada);
    solta();
    marlin_fonte(proxima);
}

void imp_cancela(void)
{
    trava();
    if (imp_ativa() && s_i.estado != IMP_CANCELANDO) {
        if (s_i.estado == IMP_PAUSADA || s_i.estado == IMP_PAUSANDO)
            s_pausado_us += esp_timer_get_time() - s_pausa_desde_us;
        s_i.estado = IMP_CANCELANDO;
        strlcpy(s_i.aviso, "Cancelando", sizeof s_i.aviso);
        diario_evento("cancelamento pedido (camada %d)", s_i.camada);
        marlin_fonte(NULL);
        // M410 descarta os movimentos ja planejados: sem ele o bico termina ate 16
        // movimentos antes de parar. Na pausa nao: cortar movimentos deixaria falha
        // na peca, e a retomada volta ao fim do ultimo movimento enviado.
        marlin_envia("M410", true);
    }
    solta();
}

// Sequencias que dependem do Marlin ter esvaziado a janela
static void estaciona(bool desliga)
{
    char l[64];
    if (desliga) {
        marlin_envia("M104 S0", false);
        marlin_envia("M140 S0", false);
        marlin_envia("M107", false);
    }
    marlin_envia("M83", false);
    snprintf(l, sizeof l, "G1 E-%.2f F2400", RETRACAO);
    marlin_envia(l, false);
    marlin_envia("G91", false);
    snprintf(l, sizeof l, "G1 Z%.1f F600", PARK_Z_SOBE);
    marlin_envia(l, false);
    marlin_envia("G90", false);
    snprintf(l, sizeof l, "G1 X%.1f Y%.1f F6000", PARK_X, PARK_Y);
    marlin_envia(l, false);
    if (desliga) marlin_envia("M84", false);
}

static void tarefa(void *arg)
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(200));
        imp_ciclo_cb_t avisar = NULL;
        char nome[84];
        trava();
        if (s_i.estado == IMP_PAUSANDO && marlin_ocioso()) {
            marlin_fonte(NULL);
            estaciona(false);
            s_i.estado = IMP_PAUSADA;
            strlcpy(s_i.aviso, "Pausada: bico estacionado, aquecimento mantido", sizeof s_i.aviso);
        } else if (s_i.estado == IMP_CANCELANDO && marlin_ocioso()) {
            estaciona(true);
            if (s_erro_sd) termina(IMP_ERRO, "Parada: falha lendo o cartao SD (aquecimento desligado)");
            else termina(IMP_CANCELADA, "Impressao cancelada");
            avisar = s_cb_ciclo;
            strlcpy(nome, s_i.nome, sizeof nome);
        }
        if (s_i.estado == IMP_IMPRIMINDO) {
            int64_t agora = esp_timer_get_time();
            s_i.decorrido_s = (int)((agora - s_inicio_us - s_pausado_us) / 1000000);
            float p = s_i.total ? (float)s_i.bytes / s_i.total : 0;
            // No comeco vale a estimativa do fatiador; depois, o ritmo real
            if (p > 0.1f) s_i.restante_s = (int)(s_i.decorrido_s * (1 - p) / p);
            else if (s_i.estimado_s > 0) s_i.restante_s = (int)(s_i.estimado_s * (1 - p));
            else s_i.restante_s = -1;
        }
        if (s_i.tft) s_i.tft_decorrido_s = (int)((esp_timer_get_time() - s_tft_inicio_us) / 1000000);
        solta();
        if (avisar) avisar(false, nome);
    }
}

void imp_inicia(void)
{
    s_mtx = xSemaphoreCreateMutex();
    s_reserva = xStreamBufferCreateWithCaps(RESERVA_BYTES, 1, MALLOC_CAP_SPIRAM);
    xTaskCreate(tarefa, "impressao", 4096, NULL, 5, NULL);
}

void imp_info(imp_info_t *saida)
{
    trava();
    *saida = s_i;
    saida->fim_ha_s = s_fim_us < 0 ? -1 : (int)((esp_timer_get_time() - s_fim_us) / 1000000);
    solta();
}

// ------------------------------------------------------------------ linhas do Marlin
void imp_linha_marlin(marlin_tipo_t tipo, const char *l)
{
    if (tipo == MARLIN_ERRO && (strstr(l, "kill") || strstr(l, "halted") || strstr(l, "M112"))) {
        trava();
        if (imp_ativa()) termina(IMP_ERRO, "A TinyBee parou (erro ou parada de emergencia)");
        s_i.tft = false;
        solta();
        return;
    }
    if (tipo != MARLIN_RX || strncmp(l, "TL:", 3)) return;
    const char *c = l + 3;
    imp_ciclo_cb_t ciclo = NULL;
    bool comecou = false;
    char nome[84] = "";
    if (!strncmp(c, "INICIO", 6)) {
        trava();
        if (!imp_ativa()) {               // as linhas TL do proprio arquivo da placa tambem chegam
            s_i.tft = true;
            const char *n = c + 6;
            while (*n == ' ') n++;
            strlcpy(s_i.tft_nome, *n ? n : "impressao da TFT", sizeof s_i.tft_nome);
            s_i.tft_camada = s_i.tft_camadas = 0;
            s_tft_inicio_us = esp_timer_get_time();
            ciclo = s_cb_ciclo;
            comecou = true;
            strlcpy(nome, s_i.tft_nome, sizeof nome);
        }
        solta();
    } else if (!strncmp(c, "CAMADA", 6)) {
        int n = 0, t = 0;
        sscanf(c + 6, " %d/%d", &n, &t);
        trava();
        // A placa reiniciou (ou conectou) com a TFT ja imprimindo e perdeu o TL:INICIO:
        // a primeira camada que chegar retoma o acompanhamento
        if (!s_i.tft && !imp_ativa()) {
            s_i.tft = true;
            strlcpy(s_i.tft_nome, "impressao da TFT", sizeof s_i.tft_nome);
            s_tft_inicio_us = esp_timer_get_time();
            ESP_LOGI(TAG, "TL:CAMADA sem TL:INICIO: acompanhando a impressao da TFT");
        }
        bool da_tft = s_i.tft && !imp_ativa();
        if (da_tft) {
            s_i.tft_camada = n;
            s_i.tft_camadas = t;
        }
        solta();
        if (da_tft && s_cb_camada) s_cb_camada(n, t);
    } else if (!strncmp(c, "FOTO", 4)) {
        if (s_cb_foto) s_cb_foto();
    } else if (!strncmp(c, "FIM", 3)) {
        trava();
        if (s_i.tft) {
            s_i.tft = false;
            marca_fim(true);
            ciclo = s_cb_ciclo;
            strlcpy(nome, s_i.tft_nome, sizeof nome);
        }
        solta();
    }
    if (ciclo) ciclo(comecou, nome);
}
