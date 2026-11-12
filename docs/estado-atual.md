# Estado atual da impressora CoreXY TinyBee (atualizado em 12/11/2026)

Ponto de partida para continuar o trabalho em outro computador/conversa. O `AGENTS.md`
descreve o Marlin em geral; este arquivo descreve **esta impressora** e o que ficou pendente.

## Hardware

- CoreXY, placa **MKS TinyBee** (ESP32), firmware Marlin deste branch (`tinybee-corexy`).
- Hotend **MK8**, cartucho **12V/50W** (trocado em 08/11/2026; o antigo era 30W/4,5 ohm).
- Sonda **BLTouch clone** (sinal em `Z_MIN_PROBE_PIN 18`).
- Ventoinha de peca **5015** (PWM por software no expansor I2S, 1000 Hz).
- **ESP32-S3-CAM** (`esp-cam-host/`) faz o papel de host: recebe o G-code (pagina web / API
  estilo OctoPrint), imprime pela serial com numero de linha e checksum, tira fotos e
  time-lapse, guarda o diario em `/sd/logs/AAAA-MM-DD.txt`.
- Display MKS TFT24 v1.1 sem funcao (o firmware de fabrica dele nao mostra impressoes da ESP).

### Portas USB (um cabo so, troca de lado)

| Placa | Chip | Porta | Gravar |
|---|---|---|---|
| TinyBee | CH340 | COM3 | `pio run -e mks_tinybee -t upload --upload-port COM3` (na raiz) |
| ESP32-S3-CAM | CH343 | COM8 (921600) | `pio run -e esp32s3cam -t upload --upload-port COM8` (em `esp-cam-host/`) |

Pelo console da ESP, `m <G-code>` manda uma linha para a TinyBee (vai pela mesma fila
numerada da impressao, pode ser usado durante a impressao).

### Pausa para troca manual de filamento (12/11/2026)

O Marlin da TinyBee **nao tem `M600`** (`ADVANCED_PAUSE_FEATURE` desligado: sem LCD nem
`EMERGENCY_PARSER`). Quem pausa e a ESP: ao ler `M600` ou `M601` no arquivo
(`esp-cam-host/src/impressao.c`, em `proxima()`), ela nao manda a linha para o Marlin,
termina os movimentos ja enviados, recolhe 6 mm, sobe 10 mm e estaciona em X5 Y176,
**mantendo bico e mesa quentes**. Retomar pela pagina (ou `retoma` no console): volta
ao ponto, devolve os 6 mm e segue o arquivo.

- No Orca: pre-visualizacao, barra de camadas na camada desejada, botao direito no `+`,
  **Adicionar pausa**. O perfil da impressora ja gera `M601` (`machine_pause_gcode`).
- Na troca: puxar o filamento velho com o bico quente, empurrar o novo ate sair a cor
  limpa, esperar parar de pingar, tirar a gota com pinca e retomar logo.

## Firmware (valores que importam)

- PID do bico (50W, `M303 E0 S230 C8`, ventoinha desligada): **P 23.15 I 2.44 D 54.87**,
  salvo na EEPROM e em `Configuration.h`.
- `PID_FAN_SCALING` com **Kf 16** (`M301 F16`): o bico segura 230 usando ~50% sem ventoinha
  e ~57% com ventoinha a 100%.
- Thermal runaway: periodo 40 s, histerese 4 C.
- Ventoinha 5015: `FAN_KICKSTART_TIME 200` / `POWER 255` (tranco ao sair do zero) e
  **`FAN_MIN_PWM 50`** (08/11/2026): em 35-50% ela parava no meio da impressao e so voltava
  com `M106 S255`. Agora 1-255 vira 50-255 de PWM.
- `ARC_SUPPORT` ligado (G2/G3). `CANCEL_OBJECTS` desligado.

## Fatiadores

- Fonte da verdade: `slicer/PrusaSlicer_CoreXY_TinyBee_bundle.ini`.
- `slicer/orca/gerar_presets_orca.py` gera os presets do OrcaSlicer (2.5) a partir dele;
  veja `slicer/orca/LEIA-ME.md`. Para instalar: Orca **fechado**, backup dos presets do
  usuario em `%APPDATA%\OrcaSlicer\user\<id>\`, e aumentar `updated_time` nos `.info`
  (sincronizacao com a nuvem).
- Retracao **6 mm** (3 mm dava fios).
- Time-lapse: so `M118 P1 TL:FOTO` na troca de camada, sem estacionar o carro.
- Orca: `high_precision_gcode` ligado e `enable_arc_fitting = sparse_infill` (arcos so no
  preenchimento; a ESP acompanha G2/G3 para a retomada da pausa).
- Ventoinha da saliencia a partir de **25%** sem apoio (o Generic PETG herdava 95%).
- PLA: ventoinha 60-100%, desligada nas 2 primeiras camadas, cheia na camada 4.
- **PETG: 235/230 C, ventoinha 100% a partir da camada 2** (desligada na 1a).
- ABS: sem ventoinha.

## Testes recentes e diagnosticos

- Erros de Thermal Runaway no PETG (camada 2): o cartucho de 30W nao vencia a ventoinha.
  Resolvido com o cartucho de 50W + PID novo + Kf.
- Peca de teste do PETG: `slicer/testes/gera_teste_petg.py` (bloco com giroide, rampa 45,
  pilares e ponte de 12 mm). Resultado de 08/11: superficie com bolhas e muitos fiapos
  (**filamento umido**), manchas marrons (**residuo queimado no bico**), rampa/ponte fracas
  (ventoinha parando). A rampa comeca com espessura zero; a ponta enrolada e defeito da peca.
- Na impressao aparece `Error:Serial status mismatch` de vez em quando, mesmo sem comandos
  manuais; a ESP reenvia e a impressao segue.

## Pendencias

1. **Secar o PETG** (65 C, 4-6 h), limpar o bico quente com escova de latao, colocar meia
   de silicone, e repetir a peca de teste fatiada de novo no Orca (perfil de ventoinha 100%).
2. Verificar a 5015 com a impressora desligada: fiapo nas pas ou encostando no duto.
3. **Cabecote novo** (Stealthburner para MK8 com cortador FilamAtrix/Filametrix, arquivos em
   `C:\tmp\3d\new\filamatrix...`): imprimir a versao **sem porca M3**
   (`sb-c-mk8-th-front_filamatrix.stl` e `sb-c-mk8-th-rear-cw2_filamatrix.stl`), em PETG
   (impressora aberta). Precisa ainda das demais pecas do Stealthburner, extrusora
   Clockwork 2 (NEMA14 36 mm, engrenagens BMG), 2x 5015 + 1x 4010, suporte do BLTouch,
   lamina/mola/batente do cortador. Depois: novo offset da sonda e area da malha no firmware.
4. **Duas cores**: o cortador nao basta; falta escolher a unidade de troca de filamento
   (a TinyBee so tem o E1 livre; alternativa: unidade com controlador proprio tipo MMU3 por
   serial). Levantar opcoes, custo e mudancas no firmware.
5. Opcional: display ILI9341 2,8" com touch nas EXP1/EXP2 + SD da propria TinyBee
   (mover a sonda para o GPIO13/EXP1-2 antes, confirmar que o sinal da sonda e 3,3 V direto;
   TFT/touch/SD compartilhando o SPI, ainda nao testado).
6. Opcional: limpador de bico (escova/silicone) numa extremidade e wipe no G-code inicial;
   `CANCEL_OBJECTS` (M486).

## Regras combinadas

- Nunca commitar a senha web (`esp-cam-host/secrets.ini`, fora do git) nem `hosting/config.php`.
- Nada de movimento ou aquecimento da impressora sem a pessoa presente e autorizando.
- Nao gravar placa durante impressao. A impressao tem prioridade absoluta na ESP.
- Presets do Orca so com o Orca fechado, sempre com backup.
