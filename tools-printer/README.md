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

**Tela de nivelamento** — botão azul *Tela de nivelamento* na barra de conexão
(ou na aba Probe). Abre uma janela própria, em `nivelamento.py`:

- **Mapa da mesa** visto de cima, com a frente embaixo: um card por ponto,
  colorido em cinco faixas (excelente ≤ 0,05 · bom ≤ 0,10 · aceitável ≤ 0,20 ·
  ruim ≤ 0,30 · crítico), e um mapa de calor entre os pontos. Passe o mouse
  num card para ver os detalhes. A cor pode mostrar o desvio da média, o que
  sobra depois de acertar as molas, ou a mudança desde a medição anterior.
- **Cards das quatro molas** com a ação: SUBIR/DESCER e quantas voltas
  (arredondado para ⅛), o sentido de giro (configure uma vez na aba Molas) e
  qual mola é a referência e fica parada.
- **Diagnóstico local**, sem internet: inclinação, ondulação que as molas não
  corrigem (e se ela acompanha o X, o Y ou é uma cúpula), pontos que destoam,
  repetibilidade contra a medição anterior (diz qual das duas leituras estava
  errada), deslocamento geral por temperatura e o próximo passo.
- **Medição**: malha `G29` 5×5 (a que vale para imprimir) ou medição rápida por
  `G30` em 3×3, 4×4 ou 5×5 (só para acertar molas). A área de sondagem (L, R, F, B)
  muda sem regravar o firmware. O número de pontos do `G29` é fixo na
  compilação (`GRID_MAX_POINTS_X`).
- **Ações**: Home, Medir, Ler malha, ligar/desligar a compensação, Gravar
  (`M500`), carregar da EEPROM, subir Z, liberar motores ou mantê-los ligados
  (`M84 S0`, o Home não se perde enquanto você gira as molas), aquecer mesa.
- **Histórico** guardado em `nivelamento.json`, com notas, e *Colar malha do
  log* para ler um `M420 V` copiado de um log.
- **Rendimento das molas**: 0,85 mm por volta, medido em 25/10 (a rosca M3 anda
  0,5 mm, mas o que importa é a inclinação da mesa). A aba Molas recalcula a
  partir de duas medições e das voltas dadas.
- **IA local opcional**: com o [Ollama](https://ollama.com) instalado, a aba
  *IA local* pede uma segunda opinião a um modelo rodando no próprio computador.
  Nada disso é necessário para nivelar.

Para ver a tela sem impressora: `python tools-printer/nivelamento.py`.

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
