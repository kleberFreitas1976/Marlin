# ESP32-S3-CAM — central da impressora (especificação)

Placa ESP32-S3-CAM ligada à TinyBee pela USB, com servidor web, câmera,
time-lapse, envio de impressões e nuvem. Este documento registra as decisões
**antes do código**. O que ainda está em aberto fica na seção final.

## 1. Objetivo

| Recurso | Resumo |
|---|---|
| Painel | Status, temperaturas com gráfico, posição, progresso, camada, tempo restante, câmera ao vivo |
| Console | Terminal G-code com histórico, jog, temperaturas, filamento, ventoinhas (o console Python na web) |
| Nivelamento | A tela de nivelamento (mapa, molas, diagnóstico, G30 rápido), com a análise em JavaScript no navegador |
| Arquivos | Upload, miniatura, visualização 3D do caminho, imprimir/pausar/retomar/cancelar, envio direto pelo OrcaSlicer |
| Time-lapse | Uma foto por camada com o bico estacionado, vídeo AVI no SD e envio para a nuvem |
| Nuvem | Galeria, página de status e comando dos dois relés pela hospedagem (HTTPS) |
| Relés | "Desligar impressora" e "Desligar tudo" |

**Quem imprime: os dois.** A TFT24 continua imprimindo pelos arquivos dela, e o
ESP também imprime pelo SD dele. O painel completo (linha, %, tempo restante)
só existe quando o ESP imprime. Numa impressão pela TFT, o ESP sabe a camada e o
nome do arquivo pelas linhas `M118 P1` do G-code (seção 5.3).

## 2. Hardware — confirmado em 29–30/10/2026

### ESP32-S3-CAM (dupla USB-C, desenho Freenove ESP32-S3-WROOM CAM)

Lido com `esptool flash_id`:

| Item | Valor |
|---|---|
| Chip | ESP32-S3 (QFN56) rev. v0.2, cristal de 40 MHz |
| PSRAM | 8 MB octal, embutida no chip (AP Memory, 3,3 V) |
| Flash | 16 MB, quad, 3,3 V |
| USB-UART | CH343, COM8 no PC: gravação e log |
| USB-OTG | USB nativa (GPIO19/20): host para a TinyBee. **VBUS medido: 5,108 V** (a placa alimenta o cabo) |
| Pino 5V | ligado direto ao VBUS das USB, **sem diodo** |

### Pinos (padrão Freenove, a confirmar na fase 1)

| Uso | GPIO |
|---|---|
| Câmera | XCLK 15, SDA 4, SCL 5, D0–D7 = 11, 9, 8, 10, 12, 18, 17, 16, VSYNC 6, HREF 7, PCLK 13 (sem PWDN e RESET) |
| SD (SDMMC 1 bit) | CMD 38, CLK 39, D0 40 |
| USB OTG | 19 (D−), 20 (D+) |
| PSRAM octal (não usar) | 35, 36, 37 |
| LED comum / LED RGB | 2 / 48 |
| **Relé "desligar impressora"** | **21** |
| **Relé "desligar tudo"** | **47** |
| **Iluminação (PWM para MOSFET)** | **14** |
| Reserva (UART plano B para a TinyBee) | 41 (TX), 42 (RX) |
| Evitar (pinos de boot) | 0, 3, 45, 46 |

### Câmera

Módulo SMT131-JCAH0001-004-01. O sensor é identificado pelo ID lido na fase 1
(OV2640, OV3660 ou OV5640), e isso define a resolução do time-lapse. Ela fica
**no quadro, na altura do bico**: no CoreXY é a mesa que desce, então a camada
atual fica sempre na mesma altura da imagem.

### TinyBee

| Item | Valor |
|---|---|
| Chip USB | **CH340** (VID 1A86, PID 7523), COM18 no PC. Driver do ESP-IDF: `usb_host_ch34x_vcp` |
| Porta do Marlin | `SERIAL_PORT 0` = porta 1 (USB, 250000). A TFT24 é a `SERIAL_PORT_2` = porta 2 (115200) |

### Alimentação

- O ESP **não** pode ser alimentado pela fonte que o relé "desligar impressora"
  corta. Se for, ao cortar a impressora o ESP desliga junto, o relé solta e a
  impressora volta a ligar. Ele precisa de um **5 V próprio**, ligado antes do relé.
- Como o pino 5V não tem diodo: com o ESP no regulador, **não ligue ao mesmo
  tempo** o cabo do PC na USB-UART. Para gravar com a placa montada, desligue o
  regulador ou use um cabo sem o fio de 5 V.

## 3. Testes já feitos (30/10/2026)

| Teste | Resultado |
|---|---|
| Abrir a USB da TinyBee **com DTR e RTS desligados** | **Não reinicia** (o marcador `M220 S97` sobreviveu) |
| Abrir a USB do jeito comum (DTR/RTS ligados) | **Reinicia** (aparece a mensagem de boot do ESP32) |
| `M118 P1 texto` | Chega na porta USB |
| `M154 S1` (posição automática) | Funciona, depois do firmware de 30/10 |
| `M115` com a área da máquina | `full` X 0–196, Y −15–181, Z 0–250 |

**Regra fixa do firmware do ESP: DTR e RTS ficam sempre desligados.** Isso vale
ao abrir a porta, ao reconectar e ao reiniciar. Um descuido aí mata a
impressão em andamento.

## 4. Mudanças no Marlin (gravadas em 30/10/2026, commit c60a0eb9d0)

- `BUFSIZE` 4 → 16.
- `AUTO_REPORT_POSITION` (M154).
- `M115_GEOMETRY_REPORT`.
- `ADVANCED_OK` **continua desligado**: ele muda o formato do `ok`, e a TFT24
  tem firmware fechado.

## 5. Ligação com o Marlin

### 5.1 Envio

- Uma tarefa no núcleo 1, com prioridade alta, cuida só da serial.
- Cada linha vai com **número de linha e checksum** (`N123 G1 X10*45`), e o
  ESP trata `Resend:`.
- **Janela de envio:** conta os `ok` recebidos e limita os bytes em trânsito ao
  que cabe no buffer de recepção da TinyBee (começa em 200 bytes, a ajustar por
  teste). Sem `ADVANCED_OK`, quem controla a janela é o ESP.
- **Comandos manuais** do console e da tela de nivelamento passam na frente da
  impressão, entre duas linhas dela.
- **`busy: processing`** zera o tempo de espera. Sem nenhuma resposta por 30 s,
  o painel mostra alerta, mas não cancela a impressão sozinho.

### 5.2 Leitura do estado

- `M155 S2`: temperaturas. `M154 S2`: posição, só durante a impressão.
- `M31`: tempo de impressão. `M220`/`M221`: velocidade e fluxo. `M420 V`: malha.
- **Detecção de travamento:** se a placa parar de responder, o painel mostra
  "TinyBee sem resposta" e **não** reconecta mexendo em DTR/RTS.

### 5.3 Impressão pela TFT24

O G-code dos fatiadores ganha estas linhas, que o Marlin repassa para a porta USB:

| Onde | Linha |
|---|---|
| Início | `M118 P1 TL:INICIO {nome do arquivo}` |
| Troca de camada | `M118 P1 TL:CAMADA {camada}/{total}` |
| Time-lapse (ver 8) | `M118 P1 TL:FOTO` |
| Fim | `M118 P1 TL:FIM` |

Sem essas linhas, o ESP ainda deduz que há uma impressão pelos alvos de
temperatura e pelo movimento, mas sem camada e sem nome.

## 6. Impressão pelo ESP

- **Leitura:** o arquivo vem do SD e é lido em blocos. Comentários e linhas
  vazias não são enviados.
- **Metadados** lidos na hora do upload: miniatura (blocos `; thumbnail begin`),
  tempo estimado, número de camadas, filamento e alturas.
- **Pausar:** o próprio Marlin não tem `ADVANCED_PAUSE` neste HAL, então o ESP
  faz a pausa. Ele para de enviar, espera o `M400`, guarda a posição e a
  extrusão, recolhe o filamento, sobe o Z e estaciona o bico. **Retomar** faz o
  caminho inverso.
- **Cancelar:** desliga o bico e a mesa, recolhe o filamento, sobe o Z e estaciona.
- **Reinício do ESP no meio da impressão:** a impressão para, como no OctoPrint.
  A retomada com recuperação fica para depois (exigiria saber a linha exata).

## 7. Arquivos e envio pelo OrcaSlicer

- **Upload pelo navegador** para o SD, em `/gcodes`.
- **Imitação mínima da API do OctoPrint**, para o Orca enviar direto:
  `GET /api/version`, `POST /api/files/local` (com `print=true` para imprimir em
  seguida) e cabeçalho `X-Api-Key`.
  - No Orca, cadastrar uma impressora física do tipo OctoPrint com o IP do ESP.
- **Miniaturas:** ligar no perfil da impressora as miniaturas no G-code (PNG 300×300).
- **Visualização 3D:** o navegador lê o G-code e desenha o caminho por camada
  em WebGL. A biblioteca fica gravada na flash, sem CDN.

## 8. Time-lapse

- **Modo escolhido: estacionar a cada camada.** O G-code de time-lapse do Orca faz:
  1. retração;
  2. leva o bico ao ponto de estacionamento, fora do enquadramento;
  3. `M400`;
  4. `M118 P1 TL:FOTO`;
  5. `G4 P500` (a pausa em que a foto é tirada);
  6. volta ao ponto anterior;
  7. desfaz a retração.

  O custo estimado é de 2 a 3 s por camada.
- **Foto:** o ESP descarta os quadros antigos e usa um quadro posterior ao
  `TL:FOTO`. Exposição e balanço de branco ficam **travados** durante o vídeo, e
  a iluminação (GPIO14) fica ligada.
- **Gravação:** JPEG em `/timelapse/<data>_<nome>/00001.jpg`. No fim, o ESP
  monta `timelapse.avi` (MJPEG), que abre no VLC e no Windows.
- **Nuvem:** envia as fotos ao longo da impressão, ou o AVI no fim (a definir
  pelo limite de upload da hospedagem).

## 9. Interface web

- **Arquivos da interface:** HTML/JS/CSS comprimidos na partição de dados da
  flash. Tudo é local: funciona sem internet.
- **Comunicação:** WebSocket para os dados ao vivo e o log; API REST para as ações.
- **Páginas:** Painel, Console, Controle, Nivelamento, Arquivos, Time-lapse,
  Relés, Configuração.
- **Acesso:** a senha da página local fica gravada na NVS do ESP e **não vai para
  o repositório**. Credenciais (Wi-Fi, senha, chave da hospedagem) ficam num
  `secrets.ini` fora do Git, lido só na primeira gravação.

## 10. Nuvem (hospedagem compartilhada com HTTPS)

Hospedagem compartilhada não mantém conexão aberta. Por isso **o ESP é quem
procura a hospedagem**, a cada 10–30 s, por HTTPS:

- **ESP → hospedagem:** status (temperaturas, %, camada), última foto e fotos
  do time-lapse.
- **Hospedagem → ESP:** a mesma requisição devolve os comandos pendentes (por
  enquanto, só os dois relés), com uma chave secreta e confirmação na página.
- **Páginas na hospedagem:** login, status com foto, galeria dos time-lapses e
  botões dos relés.
- **Arquivos:** `hosting/api.php` (recebe status, fotos e devolve comandos) e
  `hosting/index.php` (páginas), em PHP sem framework.

## 11. Relés

| Relé | Ligação | Comportamento |
|---|---|---|
| Desligar impressora (GPIO21) | contato **NF** em série com a entrada da fonte da impressora | Relé desligado = impressora ligada. Assim, um reinício do ESP **não** desliga a impressora. Para desligá-la, o ESP energiza o relé e mantém. O estado é guardado na NVS. |
| Desligar tudo (GPIO47) | contato NF em série com a **bobina de retenção** de um contator ou relé com botão de liga | Ao abrir o contato, tudo desliga, inclusive o ESP, e **fica desligado** até alguém apertar o botão de liga. Sem a retenção, o ESP desligaria, o relé soltaria e tudo voltaria a ligar. |

- **Proteção contra entupimento:** antes de cortar, o ESP espera o bico ficar
  abaixo de 50 °C, com a ventoinha do dissipador ainda ligada. Há a opção
  "forçar agora", com confirmação dupla.
- **Mão de obra:** a parte de 127/220 V deve ser montada por quem tenha prática
  com instalação elétrica, com módulos de relé com optoacoplador acionados por
  3,3 V.

## 12. Software

- **PlatformIO com `framework = espidf`**, motivos:
  - o host USB (driver do CH34x) é nativo do ESP-IDF;
  - câmera, SDMMC, PSRAM, servidor HTTP com WebSocket e HTTPS são componentes
    oficiais;
  - as tarefas têm prioridade por núcleo.
- **Versões fixadas** no `platformio.ini`.
- **Memória:** flash QIO e PSRAM octal (`qio_opi`). **Partições:** duas apps OTA
  de ~3 MB e ~4 MB para a interface web; o resto fica livre.
- **Estrutura de pastas:**

```
esp-cam-host/
  platformio.ini, partitions.csv, secrets.ini (fora do Git)
  components/
    usb_serial/   host USB, driver CH34x, DTR/RTS sempre desligados
    marlin_link/  fila, número de linha e checksum, reenvio, janela de envio, leitura do estado
    print_job/    envio do SD, pausar/retomar/cancelar, metadados e miniatura
    camera/       câmera, foto, transmissão MJPEG
    timelapse/    gatilhos, gravação, gerador de AVI
    web/          HTTP, WebSocket, REST, imitação da API do OctoPrint
    cloud/        envio e busca de comandos na hospedagem
    relays/       relés, iluminação, proteções
    settings/     NVS
  web/            interface (comprimida na gravação)
  hosting/        PHP da hospedagem
```

## 13. Mudanças nos fatiadores (na fase de cada recurso)

- **Miniaturas:** PNG 300×300 (fase 5).
- **G-code:** início, fim e troca de camada com as linhas `M118 P1 TL:` (fase 6).
- **Time-lapse:** G-code de estacionamento e modo tradicional do Orca (fase 6).
  O ponto de estacionamento depende de onde a câmera ficar.

## 14. Fases

| Fase | Entrega |
|---|---|
| 1 | ESP-IDF rodando: PSRAM, **ID da câmera**, foto no navegador, SD |
| 2 | USB host com a TinyBee: `M115`, `M105`, **confirmação de que não reinicia** |
| 3 | Painel e console web |
| 4 | Tela de nivelamento web |
| 5 | Arquivos, miniatura, visualização 3D, impressão pelo ESP, envio pelo Orca |
| 6 | Time-lapse e as linhas `M118` nos fatiadores |
| 7 | Relés e iluminação |
| 8 | Nuvem: PHP, status, galeria, relés remotos |
| 9 | Telas de calibração, atualização pelo Wi-Fi, acabamento |

## 15. Em aberto

- **Dados da hospedagem:**
  - versão do PHP;
  - limite de upload (`upload_max_filesize`, `post_max_size`);
  - espaço em disco;
  - se tem MySQL ou se o PHP grava em arquivos.
- **ID do sensor da câmera:** sai na fase 1.
- **Modelo dos relés e do contator**, e a tensão de rede (127 ou 220 V).
- **Iluminação:** tipo da fita (12 V ou 24 V) e se o ESP a liga e desliga
  (MOSFET no GPIO14).
- **Ponto de estacionamento** do time-lapse, depois de montar a câmera.
