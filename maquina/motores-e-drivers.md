# Motores e drivers — CoreXY MKS TinyBee

Medições feitas na máquina em 24/10/2026.

> Esta pasta fica fora dos diretórios do Marlin original (`Marlin/`, `docs/`,
> `buildroot/`) de propósito, para não conflitar quando o fork for atualizado a
> partir do upstream.

---

## 1. O que está montado

| Item | |
|---|---|
| Motores | Minebea **17PM-K140-04V** (NEMA 17) |
| Drivers | TMC2209 em **standalone** |
| Fonte | 12 V |
| Microstepping | 1/16 (80 passos/mm em X e Y) |

A TinyBee **não tem UART para os drivers** — o `Marlin/src/pins/esp32/pins_MKS_TINYBEE.h`
não define pinos de serial para eles. Consequências:

- a corrente é definida **só pelo trimpot** de cada módulo, não por G-code;
  `PWM_MOTOR_CURRENT` e `DIGIPOT_MOTOR_CURRENT` estão desativados e não se aplicam;
- não há `hybrid threshold`: o Marlin não consegue trocar de StealthChop para
  SpreadCycle acima de uma velocidade. O driver fica no modo que o pino SPREAD
  do módulo determinar, sempre.

---

## 2. Medições

Resistência medida por fase, com o motor **desligado da placa**, entre os dois
fios de um mesmo par. Lembre de encostar as pontas de prova uma na outra antes e
subtrair a leitura — cabo de multímetro tem 0,5 a 2 Ω, e isso pesa quando o
motor tem 1,6.

| Eixo | R por fase | Nominal estimado | Vref medido | Corrente atual | % do nominal |
|---|---|---|---|---|---|
| X | 1,6 Ω | ~1,5 A | ~~0,733 V~~ → **1,45 V** | 1,03 A | 68% |
| Y | 1,6 Ω | ~1,5 A | **1,46 V** | 1,03 A | 69% |
| Z | 4,3 Ω | ~1,0 A | **1,00 V** | 0,71 A | 71% |
| Z2 (driver E1) | *não medido* | ~1,0 A | **1,00 V** | 0,71 A | 71% |
| E0 | 2,5 Ω | ~1,2 A | **1,20 V** | 0,85 A | 71% |

### De onde vem o "nominal estimado"

Não é de datasheet. A folha de dados da série 17PM-K existe, mas a linha do
**K140-04V** não foi localizada, e a corrente varia bastante dentro dessa
família. O valor aqui é inferido da resistência, assumindo que um NEMA 17 é
dimensionado para cerca de 3 W por fase na corrente nominal:

```
I_nominal ≈ raiz(3 / R)
```

É estimativa, não medida. Se a etiqueta completa de algum motor aparecer com a
corrente impressa, substitua.

---

## 3. Fórmula da corrente (TMC2209)

```
I_RMS = (Vref / 2,5) × (325mV / (Rsense + 20mΩ)) / raiz(2)
```

O `Rsense` depende do módulo do driver e **não foi verificado**. Nos dois valores
usuais:

| Rsense | I_RMS | Vref para uma corrente alvo |
|---|---|---|
| 0,11 Ω (assumido aqui) | Vref × 0,707 | I × 1,414 |
| 0,10 Ω | Vref × 0,766 | I × 1,305 |

Se o módulo for 0,10 Ω, as correntes reais são ~8% maiores que as da tabela
acima — o que não muda nenhuma conclusão, só torna os alvos um pouco
conservadores.

---

## 4. Ajustes sugeridos

Alvo: **70 a 75% da corrente nominal**. Acima disso o ganho de torque é pequeno
e o calor cresce rápido.

| Eixo | Vref alvo | Corrente resultante | % do nominal |
|---|---|---|---|
| X, Y | **1,45 V** | 1,03 A | 69% |
| Z, Z2 | **1,00 V** | 0,71 A | 71% |
| E0 | **1,20 V** | 0,85 A | 71% |

**Os três valores são diferentes de propósito.** Os motores não são iguais;
colocar todos em 1,45 V faz o Z e o E esquentarem sem ganho.

### Procedimento

1. Placa **ligada**, motores parados.
2. Gire o trimpot **1/8 de volta por vez**, medindo a cada passo. O sentido
   varia entre módulos — confirme com o multímetro, não suponha.
3. Em X e Y, suba em dois tempos: primeiro **1,2 V**, teste, depois 1,45 V se
   ainda roncar.

### Verificações depois de cada ajuste

**Motor** — rode 15 minutos e encoste a mão. Morno a ponto de segurar alguns
segundos está certo (40 a 60 °C). Se não der para encostar, desça.

**Driver** — dobrar a corrente mais que dobra a dissipação no TMC2209. Toque no
dissipador. Se estiver muito quente ele entra em proteção térmica no meio de uma
impressão, e aí os passos se perdem de verdade. Confira se há ventilação sobre
os módulos.

---

## 5. O que isso deve resolver

### Ronco em alta velocidade

Velocidades medidas em 24/10/2026, **com os motores a 35% da corrente**:

| Teste | Limpo até | Ronca a partir de | Velocidade do motor |
|---|---|---|---|
| X puro | 145 mm/s | 150 | ~147 |
| Diagonal 45° | 110 mm/s | 120 | ~155 |

Não houve perda de passo em nenhum ponto — o ruído apareceu antes da falha.
Torque é proporcional à corrente, e a 35% o motor simplesmente não acompanha em
rotação alta.

Depois de corrigir a corrente, **refaça o teste**. Se o limite subir, o
`DEFAULT_MAX_FEEDRATE` (hoje em 140) foi dimensionado para motores subalimentados
e merece ser revisto.

### BMG patinando na partida

O extrusor patinava com a aceleração padrão de 10000, e isso foi contornado
baixando `DEFAULT_MAX_ACCELERATION` do E para 1000. Se o driver do E também
estiver em ~0,73 V, o motor está com pouco mais de 40% do torque — e patinar na
partida é exatamente o sintoma.

Depois de ajustar o E, vale testar se a aceleração pode voltar a subir.

### Repetibilidade da sondagem

Z são **dois motores** carregando a mesa aquecida, e num G29 de 5×5 o conjunto
sobe e desce 25 vezes. Torque marginal ali pode virar assentamento diferente
entre pontos, e isso entra na malha como se fosse formato da mesa.

Referência de antes do ajuste, para comparar:

```
M48 P10   ->  sigma 0,0077 e 0,0111 em duas rodadas
```

Se o desvio melhorar depois de subir o Vref do Z, o torque estava limitando.

---

## 6. Por que não vale trocar de motor

Com 1,6 Ω, os motores de X e Y são da classe comum de impressora 3D (1,5 A).
Não são o gargalo — estão mal alimentados.

Se o ronco persistir **depois** de corrigir a corrente, aí a limitação é a
tensão: a corrente na bobina sobe a uma taxa de V/L, e a 12 V, acima de certa
rotação, ela não tem tempo de subir antes da próxima comutação. Passar para 24 V
praticamente dobra essa taxa.

Mas é obra: resistência do hotend, mesa aquecida e ventoinhas, todos trocados. E
é preciso confirmar na documentação da TinyBee se a entrada aceita 24 V.

A outra saída seria forçar **SpreadCycle** pelo pino SPREAD do módulo: mais
torque e estabilidade em alta, ao custo de ruído audível em *toda* velocidade,
inclusive nas de impressão, que é onde a máquina passa o tempo. Trocaria um
ruído ouvido 2% do tempo por outro ouvido 100%.

---

## 7. Pendências

- [x] X e Y ajustados em 24/10/2026: de 0,733 V para 1,45 e 1,46 V.
      A diferença de 0,01 V entre eles está dentro da precisão do
      multímetro e do trimpot; não vale perseguir.
- [x] Todos os eixos ajustados em 24/10/2026: X/Y 1,45 V, Z 1,00 V, E0 1,20 V
- [x] Z2 (driver E1) confirmado em 1,00 V, igual ao Z. Os dois precisam
      bater: se um tiver mais torque, o lado fraco fica para trás sob carga e
      a mesa torce a cada subida — sem erro, sem ruído, aparecendo só na malha
      como se fosse empeno.
- [ ] Conferir a temperatura dos motores e dos drivers após 15 min
- [ ] Refazer o teste de velocidade (referência: X 145, diagonal 110, a 35%)
- [ ] Refazer o `M48` (referência: sigma 0,0077 e 0,0111, a 35%)
- [ ] Testar se a aceleração do E pode subir de 1000 (era 10000 e patinava)
- [ ] Medir a resistência do Z2
- [ ] Confirmar o `Rsense` dos módulos (marca impressa na placa do driver)
- [ ] Achar a corrente nominal real na etiqueta dos motores, se estiver legível
- [ ] Aplicar os Vref alvo e refazer: teste de velocidade, `M48`, aceleração do E
- [ ] Verificar ventilação sobre os drivers antes de subir a corrente
