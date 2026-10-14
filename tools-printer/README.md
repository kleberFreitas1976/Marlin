# Console de testes Marlin — MKS TinyBee / CoreXY

Ferramenta de bancada para a fase inicial de montagem e calibração.

## Rodar

```bash
python tools-printer/marlin_console.py
```

Requer Python 3 com `tkinter` (já vem no instalador oficial do Windows) e `pyserial`:

```bash
pip install pyserial
```

> Use o Python do sistema, **não** o do PlatformIO — o venv do PlatformIO
> não tem `tkinter`.

## O que faz

**Conexão** — o botão *Localizar placa* varre as portas seriais, dá prioridade
a conversores USB conhecidos (CH340, CP210x, FTDI), testa 250000/115200/500000/57600
e identifica a placa pela resposta do `M115`. Conecta sozinho ao encontrar.

**Card de status** — temperatura do bico e da mesa (atual/alvo, com cor indicando
se atingiu o alvo), posição XYZE e o estado de cada fim de curso e sensor, em
verde/vermelho. Com *Monitorar continuamente* ligado, usa o auto-report de
temperatura do Marlin (`M155`) e consulta o `M119` a cada 3 s.

**Movimento** — jog por eixo com passo de 0,1 / 1 / 10 / 50 mm e velocidade
ajustável, homing por eixo individual (com confirmação) e liga/desliga motores.

**Temperatura** — aquecer e desligar bico e mesa, ventoinha de peça, auto-ajuste
de PID (`M303`) e extrusão manual com aviso de bico frio.

**Probe / Nivelamento** — aciona e recolhe o servo nos ângulos do firmware
(45° ativo, 0° recolhido), ajusta o offset Z (`M851`), sonda um ponto (`G30`),
testa repetibilidade (`M48`), roda o nivelamento (`G29`) e mostra a malha (`M420 V`).

**Calibração** — lê e grava passos/mm (`M92`), operações de EEPROM
(`M500`/`M501`/`M502`/`M503`) e atalhos de informação. Inclui os comandos
`[ESP111]` e `[ESP420]` do ESP3D para ver IP e status do Wi-Fi.

**Console** — todo o tráfego com marcação de hora e cores por tipo (enviado,
recebido, erro, `ok`). Campo de envio direto de G-code com histórico pelas
setas ↑ e ↓. As linhas de auto-report de temperatura atualizam o card sem
poluir o log.

**Parada de emergência** — botão vermelho sempre visível, envia `M112`.

## Ajustado para esta máquina

Os valores padrão refletem o firmware desta impressora:

| Item | Valor |
|---|---|
| Cinemática | CoreXY |
| Passos/mm | X 80, Y 80, Z 400, E 415 |
| Ângulos do servo | 45° ativo, 0° recolhido |
| Baud | 250000 |
| Extrusão a frio | bloqueada abaixo de 170 °C |

Se você alterar o firmware, ajuste as constantes no topo de
`marlin_console.py`.

## Ordem sugerida na primeira vez

1. *Localizar placa* → conecta e já roda `M115` e `M119`.
2. Confira no card: os três fins de curso em **verde** (`open`) com tudo em repouso.
3. Acione cada sensor com a mão e veja a linha correspondente ficar vermelha.
4. Aba **Movimento**, passo 10 mm: teste `X+` (deve ir para a direita) e `Y+`
   (para trás). **Em CoreXY, movimento na diagonal significa motor invertido** —
   pare e corrija `INVERT_X_DIR` / `INVERT_Y_DIR` no firmware.
5. Só depois disso, homing de um eixo por vez.
