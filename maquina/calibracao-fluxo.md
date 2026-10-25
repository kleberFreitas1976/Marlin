# Calibração do multiplicador de extrusão

O multiplicador de extrusão (`extrusion_multiplier`) corrige a diferença entre
o volume de plástico que o fatiador calcula e o que sai de fato pelo bico. Está
em **1,0** (o padrão) e nunca foi medido. O excesso de extrusão é a hipótese
principal para o acúmulo de plástico na ponta do benchy.

Os passos do extrusor já estão calibrados (100 mm pedidos, 100 mm reais). Isso
garante quanto filamento **entra**; o multiplicador acerta quanto **sai**, em
volume.

## 1. Antes de começar

- **Filamento do rolo que vai ser impresso**, carregado, bico a 210 °C.
- **Meça o diâmetro do filamento** com o paquímetro em uns 5 pontos ao longo
  de alguns metros e tire a média. O nominal é 1,75 mm, mas rolos reais variam
  de uns 1,70 a 1,78. Diâmetro errado e multiplicador errado fazem o mesmo
  efeito (o volume cresce com o quadrado do diâmetro), então acerte o diâmetro
  **primeiro**: Config. de filamento → Filamento → *Diâmetro*.

## 2. Fatiar o cubo

No PrusaSlicer:

1. Importe o perfil `slicer/PrusaSlicer_Calibracao_Fluxo.ini`
   (Arquivo → Importar → Importar Config Bundle). Ele cria o perfil de
   impressão **CoreXY TinyBee Calibracao Fluxo**. O mesmo perfil já está no
   bundle completo.
2. Selecione esse perfil de impressão, com o filamento e a impressora de
   sempre.
3. Adicione um cubo de **25 × 25 × 25 mm** (Galeria de formas, ou qualquer cubo
   em STL) e posicione perto do centro da mesa.
4. Fatie. A pré-visualização deve mostrar **uma única linha de perímetro**,
   sem fundo, sem topo e sem preenchimento: uma caixa oca aberta nas duas
   pontas.

O perfil usa parede única, tudo a 30 mm/s e 1000 mm/s² para que velocidade e
aceleração não entrem na conta, e largura do perímetro externo de **0,45 mm**,
que é o valor de referência da medida.

## 3. Medir

Depois de esfriar:

- **Ignore os 2 mm de baixo** (a primeira camada é esmagada de propósito).
- Meça a espessura da parede nas **quatro faces, em três alturas** (uns 8, 14
  e 20 mm), **longe dos cantos e da costura** (a costura forma um pequeno
  calombo, e os cantos engrossam).
- Feche as garras do paquímetro **devagar**: plástico fino deforma com pouca
  pressão e a medida sai menor que a real.
- São 12 medidas. Descarte uma ou duas muito fora do grupo e tire a média.

## 4. Calcular

```
novo multiplicador = 0,45 ÷ espessura média medida
```

| Espessura média | Multiplicador |
|---|---|
| 0,50 mm | 0,90 |
| 0,48 mm | 0,94 |
| 0,45 mm | 1,00 (já está certo) |
| 0,42 mm | 1,07 |

**Sanidade:** se der abaixo de **0,85** ou acima de **1,05**, desconfie de outra
coisa antes de usar o número: diâmetro do filamento não conferido, passos do
extrusor, ou temperatura muito diferente da usada nos outros testes.

## 5. Aplicar

Config. de filamento → Filamento → **Multiplicador de extrusão**. Salve o
perfil.

Depois me passe a espessura média e o multiplicador. O valor precisa entrar
no bundle do repositório, nas quatro seções de filamento ao mesmo tempo (a
diferença de chaves entre seções foi o que fez o PrusaSlicer descartar PETG e
ABS em silêncio da outra vez), e é **um valor por filamento**: o PETG e o
PLA Speed+ terão multiplicadores diferentes.

## Resultados

| Filamento | Bico | Diâmetro medido | Espessura média | Multiplicador |
|---|---|---|---|---|
| GTMax3D PLA Speed+ | 210 °C | 1,85–1,90 mm (média 1,875) | 0,60 mm, com 1,75 no perfil | — |
| GTMax3D PLA Speed+ | 210 °C | idem | **0,59 mm**, com 1,88 no perfil | *não aplicado: a parede não respondeu* |
| PETG | 230 °C | — | — | — |

## Primeiro resultado (25/10/2026)

Cubo de 20 mm no GTMax3D PLA Speed+ a 210 °C, com o diâmetro do perfil ainda em
1,75: parede de **0,60 mm** contra 0,45 esperado, ou seja 33% de excesso. Uma
primeira medida do rolo deu 1,70 a 1,95 mm; a medida refeita, com mais
cuidado, deu **1,85 a 1,90 mm (média 1,875)**. O rolo é consistentemente
grosso, e isso se corrige no perfil, não no multiplicador.

```
área com 1,875 mm = 2,761 mm²    área com 1,75 mm = 2,405 mm²
volume subestimado pelo fatiador = 14,8%
parede esperada só com o diâmetro certo = 0,60 / 1,148 = ~0,52 mm
```

O `filament_diameter` do perfil passou para **1,88**. Os ~0,52 mm esperados
ainda ficam acima de 0,45: parte é o paquímetro apoiado nas ondulações das
camadas (uns 0,03 mm), e o resto o multiplicador resolve. **O multiplicador
só deve ser calculado depois de refazer o cubo com 1,88.**

Rolo de 1,88 em hotend de 1,75 aumenta o atrito no tubo e na entrada do
hotend. Se a extrusora começar a esforçar ou o fluxo máximo ficar bem menor
que o esperado, isso entra na conta. O rolo está fora da especificação
(1,75 ± 0,05) e vale falar com o fornecedor.

## Segundo resultado (25/10/2026): a parede não respondeu

Com `filament_diameter` em 1,88, a estimativa do fatiador para o cubo foi de
0,23 m / 0,79 g (confere com 1,88; com 1,75 seriam 0,69 g), ou seja 13% menos
filamento comandado que antes. A parede medida foi **0,59 mm**, contra 0,60
com 1,75. Se a espessura acompanhasse o fluxo, cairia para ~0,52.

A explicação pelo diâmetro, sozinha, não se confirmou. Nenhum multiplicador
foi gravado. Próximos passos, em ordem:

1. Pesar o cubo (sem skirt e sem purga): o fatiador prevê 0,79 g, e
   `multiplicador = 0,79 / massa medida`. Independe do paquímetro.
2. Conferir a marca gravada no bico: 0,59 é quase 0,6, e um bico de 0,6 mm
   imprimiria essa largura mesmo com 0,45 no perfil.
3. Reimprimir com multiplicador 0,80: se a parede for a ~0,49, o fluxo
   comanda a espessura; se ficar em 0,59, a causa é outra.

## Terceira medida (25/10/2026): o benchy tambem pesa 59% do previsto

| Peca | Previsto | Pesado | Entregue |
|---|---|---|---|
| Cubo 20 mm, diametro 1,75 no perfil | ~0,79 g | 0,54 g | ~68% |
| Cubo 20 mm, diametro 1,88 no perfil | 0,79 g | 0,49 g | ~62% |
| Benchy reduzido (so o barco, sem purga e skirt) | 5,0 g | 2,93 g | **59%** |

Tres pecas, mesmo resultado. Mas o benchy tem superficies de topo fechadas,
o que nao combina com 40% de falta de material. As explicacoes possiveis:

1. A balanca le baixo. Conferir com peso conhecido (moeda de R$ 1 = 7,00 g).
2. O plastico depositado tem vazios (filamento umido formando bolhas): parede
   mais grossa e mais leve ao mesmo tempo, que foi exatamente o que o cubo
   mostrou (0,59 mm de parede com 62% da massa).
3. A extrusora entrega menos comprimento que o comandado sob carga.

Testes para separar, na ordem: moeda na balanca; pesar 1 m de filamento
(esperado 3,44 g para 1,875 mm); extrudar 2 x 100 mm no ar, pesar o fio
(esperado 0,69 g) e olhar se ele sai liso ou com bolhas.

## Depois de aplicar

Reimprima o benchy. A ponta (proa) é o ponto mais sensível ao excesso de
material e serve de comparação direta com a falha anterior.
