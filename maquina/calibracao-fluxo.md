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
| GTMax3D PLA Speed+ | 210 °C | *não medido* | *não medido* | *não aplicado* |
| PETG | 230 °C | — | — | — |

## Depois de aplicar

Reimprima o benchy. A ponta (proa) é o ponto mais sensível ao excesso de
material e serve de comparação direta com a falha anterior.
