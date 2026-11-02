# Ondulação no eixo X (02/11/2026)

As seis malhas 5×5 gravadas de 21 a 29/10 tinham um zigue-zague por coluna: as
colunas 2 e 4 ficavam ~0,3 mm acima ou abaixo das vizinhas, sempre, e no sentido
Y a malha era lisa. Um vidro não ondula assim, então medimos uma linha em X.

## Teste

`G28`, `M420 S0` e `G30` a cada 4 mm em X (posição da sonda), de X15 a X139, em
duas linhas: Y=90 e Y=40. Mesa e bico frios. Valores em mm (Z do disparo).

| X da sonda | Y=90 | Y=40 |
|---|---|---|
| 15 | 0,122 | 0,222 |
| 19 | 0,165 | 0,245 |
| 23 | 0,170 | 0,273 |
| 27 | 0,220 | 0,280 |
| 31 | 0,273 | 0,325 |
| 35 | 0,320 | 0,327 |
| 39 | 0,367 | 0,335 |
| 43 | 0,352 | 0,327 |
| 47 | 0,352 | 0,327 |
| 51 | 0,325 | 0,303 |
| 55 | 0,273 | 0,293 |
| 59 | 0,220 | 0,245 |
| 63 | 0,170 | 0,213 |
| 67 | 0,142 | 0,168 |
| 71 | 0,082 | 0,113 |
| 75 | 0,013 | 0,048 |
| 79 | -0,062 | -0,030 |
| 83 | -0,082 | -0,048 |
| 87 | -0,108 | -0,082 |
| 91 | -0,100 | -0,057 |
| 95 | -0,040 | -0,032 |
| 99 | 0,010 | 0,013 |
| 103 | 0,073 | 0,073 |
| 107 | 0,117 | 0,130 |
| 111 | 0,187 | 0,150 |
| 115 | 0,190 | 0,148 |
| 119 | 0,138 | 0,173 |
| 123 | 0,088 | *falhou* |
| 127 | 0,043 | *falhou* |
| 131 | 0,015 | 0,065 |
| 135 | 0,000 | 0,055 |
| 139 | -0,035 | -0,037 |

## Resultado

Ajuste de senoide + inclinação:

| Linha | Amplitude | Período | Inclinação | Resíduo (rms) |
|---|---|---|---|---|
| Y=90 | 0,163 mm | 72,5 mm | -0,28 mm/100 mm | 0,027 mm |
| Y=40 | 0,140 mm | 76,6 mm | -0,29 mm/100 mm | 0,025 mm |

- **A onda é real e acompanha o X:** mesma fase, mesmo período e mesma amplitude
  nas duas linhas, 50 mm uma da outra (diferença ponto a ponto 0,037 mm rms). Não
  é ruído da sonda (M48 dá sigma 0,008 a 0,011) nem o vidro.
- **Período de ~74 mm = uma volta de algo com ~23,5 mm de diâmetro.** É o tamanho
  de uma roldana em V (24 mm por fora) rolando no perfil. Suspeita principal:
  roldana empenada ou com o furo fora de centro no carro do X, ou a porca
  excêntrica dela.
- **Por que a malha mostrava zigue-zague:** a malha 5×5 tem pontos a cada 31,75 mm
  em X, quase meio período. Cada coluna cai alternadamente perto de um pico e de
  um vale, e a interpolação liga esses pontos em retas: a compensação não segue a
  onda real. Uma malha 4×4 (42 mm) amostraria ainda pior.
- **Falhas da sonda:** 2 dos 64 pontos falharam ("Probing Failed") em X123 e X127
  com Y=40, com a mesa fria e os vizinhos normais. Na impressão de 02/11 a falha
  deixou o bico em Z0,56: a sonda desceu até o limite sem disparar (o vidro estava
  lá). É falha intermitente da sonda (haste ou sensor ótico), não mesa baixa.

## Próximos passos (presencial)

1. Girar cada roldana do carro do X com a mão e procurar a que bambeia ou tem ponto
   duro; marcar com caneta e andar 74 mm em X: a marca deve dar uma volta por onda.
2. Trocar ou recentrar a roldana (porca excêntrica) e repetir as duas linhas.
3. Ver a sonda nos pontos que falham: haste raspando, sensor ótico, cabo.
4. Se a onda não sair: malha mais densa em X que em Y (por exemplo 9×4,
   pontos a cada 15,9 mm em X), viável depois de reduzir as folgas do Z.
