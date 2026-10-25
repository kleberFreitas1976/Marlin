# Estimativa de tempo do PrusaSlicer x tempo real

A estimativa do PrusaSlicer conta só os movimentos. Não conta as esperas de
temperatura (`M190`, `M109`) nem a duração do `G28`, que sonda.

## Medidas

| Peça | Estimado | Real (TFT) | Diferença | Partida |
|---|---|---|---|---|
| Cubo de calibração de fluxo, 1 perímetro | 11 min | 15 min 18 s | **4 min 18 s** | mesa próxima de fria, 55 °C |

A TFT mostrou `T: 00:15:18` ao fim, com o arquivo em 100%.

## Regra prática

Some **uns 4 a 5 minutos** à estimativa quando a máquina parte fria. A
diferença é o aquecimento da mesa, o home com sondagem, a subida do bico até a
temperatura de impressão e a purga em três linhas. Se a mesa já estiver morna
da impressão anterior, o aquecimento é bem menor.

## Observações

- A estimativa em si está certa. Nada no perfil precisa mudar por causa dela.
- A barra de progresso da TFT não é proporcional ao tempo: foram observados 13
  minutos em "60%" com final em 15 min 18 s. Provavelmente anda por bytes do
  arquivo. Não serve para prever o fim.
- Para medir só o tempo de impressão, marcar o relógio quando a primeira camada
  começar (depois da purga).
- `machine_max_junction_deviation = 0.013,0.013` foi acrescentado ao perfil da
  impressora para o estimador tratar as curvas como o firmware, que usa
  junction deviation e tem o jerk clássico desligado. O efeito é de centésimos
  de segundo por curva, e não explicava a diferença.
