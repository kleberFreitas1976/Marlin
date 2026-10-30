# ESP32-S3-CAM — central da impressora

Firmware (PlatformIO + ESP-IDF 5.3.1) da placa ESP32-S3-CAM ligada à TinyBee
pela USB OTG. Especificação e decisões: [ESPECIFICACAO.md](ESPECIFICACAO.md).

## O que faz

| Aba | Recursos |
|---|---|
| Painel | câmera (foto e ao vivo), bico e mesa, gráfico de 10 min, posição, cartão da impressão (barra, camada, restante, término, pausar/retomar/cancelar) |
| Controle | jog XY/Z, home, filamento, ventoinha, M220/M221, malha, **energia e luz** (relés) |
| Console | G-code com histórico e cores, comandos rápidos, parar fila |
| Nivelamento | a tela de nivelamento do console Python: mapa, molas, diagnóstico, G29/G30, histórico no SD |
| Arquivos | envio de G-code (ou direto do OrcaSlicer como OctoPrint), miniatura, pré-visualização por camada, imprimir |
| Time-lapse | configuração, gravações, reprodutor, download do vídeo AVI |
| Sistema | TinyBee, placa, Wi-Fi, nuvem (site na hospedagem), atualização do firmware |

Sem rede Wi-Fi, a placa cria a rede **Impressora-CAM** (senha 12345678) com a
página de configuração em `http://192.168.71.1/wifi`.

## Gravar

Primeira vez (ou se a placa não subir): cabo na porta **USB-UART** (COM8).

```
pio run -t upload
```

Depois, pela página: **Sistema → Atualizar firmware…** com o arquivo
`.pio/build/esp32s3cam/firmware.bin`.

Credenciais (Wi-Fi inicial e senha da página) em `secrets.ini`, fora do Git
(modelo em `secrets.exemplo.ini`). A rede Wi-Fi pode ser trocada pela página.

## Ferramentas (pasta `tools`)

| Arquivo | Para quê |
|---|---|
| `servidor_teste.py` | a interface em `http://localhost:8765` com uma TinyBee simulada, para desenvolver sem a impressora |
| `nuvem_teste.py` | imita o `api.php` da hospedagem na rede local |
| `foto_serial.py` | tira uma foto pelo console serial |
| `gera_web.py`, `segredos.py` | rodam sozinhos na compilação |

Console serial (921600): `ajuda` lista os comandos. Os úteis para diagnóstico:
`info`, `tb`, `imp`, `m <G-code>`, `foto`, `sd`, `gerateste` + `imprime teste_seguro.gcode`
(uma "impressão" que só lê temperaturas e espera: não move nem aquece).

## O que foi testado e o que falta

Testado na placa (30/10/2026), sem mover nem aquecer a impressora:
- USB com a TinyBee sem reiniciá-la (inclusive com a placa reiniciando);
- fila com janela de 4 linhas; impressão do G-code inofensivo do SD até o fim;
- miniatura e dados extraídos do arquivo; time-lapse de 13 fotos e o AVI
  (estrutura conferida); nuvem com o simulador (status, comando, foto inteira);
- senha em todas as rotas (sem senha: 401).

Falta testar com alguém olhando a impressora:
- **pausar, retomar e cancelar** uma impressão real (movem o bico);
- uma impressão real do Benchy pela placa, com time-lapse;
- o envio direto pelo OrcaSlicer (cadastro da impressora OctoPrint);
- os relés (ainda não instalados) e a luz;
- o site na hospedagem (o PHP não roda neste PC);
- atualizar o firmware pela página;
- desligar a placa da tomada com a TinyBee ligada (o cabo USB perde os 5 V).
