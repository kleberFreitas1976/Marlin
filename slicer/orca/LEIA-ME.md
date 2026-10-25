# Perfis do OrcaSlicer — CoreXY MKS TinyBee

O Orca **não importa** o bundle `.ini` do PrusaSlicer: o "Importar configurações"
dele aceita só `.json`, `.zip`, `.orca_printer`, `.orca_filament` e
`.orca_bundle`. Estes arquivos são a conversão do bundle do PrusaSlicer, que
continua sendo a fonte de verdade.

| Arquivo | Tipo |
|---|---|
| `CoreXY MKS TinyBee.json` | impressora |
| `CoreXY TinyBee 0.20mm.json` | processo (perfil de impressão) |
| `PLA Speed Plus GTMax3D.json` | filamento — o rolo em uso, diâmetro medido 1,88 |
| `PETG CoreXY TinyBee.json` | filamento |
| `PLA CoreXY TinyBee.json` | filamento |
| `ABS CoreXY TinyBee.json` | filamento |

## Importar

0. A impressora genérica **Generic Marlin Printer** (MyMarlin 0.4 nozzle) precisa
   estar ativada no Orca: os perfis herdam dela e dos filamentos genéricos.
1. Orca → **Arquivo → Importar → Importar configurações**.
2. Selecione os **seis `.json`** desta pasta de uma vez.
3. Selecione a impressora **CoreXY MKS TinyBee**, o processo **CoreXY TinyBee
   0.20mm** e o filamento.

O tipo de placa da mesa não importa: os filamentos têm a mesma temperatura de
mesa em todos os tipos de placa do Orca.

## Como foram gerados

`gerar_presets_orca.py` lê o bundle do PrusaSlicer e escreve os seis arquivos.
**Depois de mudar o bundle, rode de novo** para os dois fatiadores ficarem iguais:

```
python slicer/orca/gerar_presets_orca.py
```

Cada perfil sai com **todos os valores** (a cadeia de herança do Orca é
resolvida pelo script) e herda do perfil de sistema correspondente
(`MyMarlin 0.4 nozzle`, `0.20mm Standard @MyMarlin`, `Generic PLA/PETG/ABS
@System`), como os presets que o próprio Orca grava. O que o bundle não define
fica com o padrão do Orca.

**Todo preset precisa da chave `version`.** Sem ela o Orca recusa a importação
em silêncio: a janela diz "Foram importadas 0 configurações" e o log não
registra motivo. A primeira versão destes arquivos não tinha a chave, porque foi
copiada do formato dos presets de sistema, que não a usam.

A linha de comando do Orca (`--load-settings`) **não serve** para testar estes
arquivos: ela recusa até os presets de sistema do próprio Orca.

O script confere que todas as chaves escritas existem nos perfis desta versão
do Orca (2.4). Se uma versão futura renomear alguma, ele avisa.

## Diferenças em relação ao PrusaSlicer

- **Saliências:** os nomes têm sentidos opostos. No Prusa, `overhang_speed_N` é
  a velocidade para N × 25% de **apoio**. No Orca, `overhang_N_4_speed` é para
  N/4 da linha **no ar**. A conversão inverte a ordem: 25 / 20 / 15 / 15 mm/s,
  de pouco no ar a muito no ar.
- **M73:** vários perfis do Orca vêm com `disable_m73` ligado. Aqui fica
  desligado, porque a TFT usa o M73 para o progresso.
- **Paredes:** `wall_generator = arachne`, o padrão do PrusaSlicer, que é o que
  imprimiu o benchy.
- **Limites da máquina:** o Orca não tem a opção do Prusa "só para estimar o
  tempo". Os valores do perfil batem com o firmware (140 mm/s, 3000 mm/s²,
  E 1000 mm/s², junction deviation 0,013), então tanto faz se forem escritos
  no G-code.

- **Skirt:** o padrão do Orca é 50 mm/s, mesmo na primeira camada. Aqui usa a
  velocidade da primeira camada (20 mm/s), como no PrusaSlicer.
- **Comandos no início do arquivo:** o Orca escreve `M201`, `M203`, `M204` e
  `M205` antes do G-code de início. O `M205 X8 Y8 Z0.3 E5` (jerk) é **ignorado**
  pelo firmware, porque `CLASSIC_JERK` está desligado (M200-M205.cpp só lê X/Y/Z/E
  dentro de `#if ENABLED(CLASSIC_JERK)`). O `M205 J0.013` vale e é igual ao
  firmware. O `M203 X140 Y140` garante o limite de velocidade mesmo que a EEPROM
  ainda guarde os 300 antigos.

## Estado

Importados e conferidos pelo G-code de um benchy completo (25/10/2026): os três
perfis aplicados, bico a 150 °C no home e purga em três linhas, `M73` (192
ocorrências), 215 → 205 °C e mesa 60 → 55 °C, ventoinha a 100% a partir da
camada 2, sem arcos `G2/G3` e sem `M486`, saliências a 15 mm/s e 31% do
perímetro externo desacelerado, fluxo máximo de 6,7 mm³/s. Ainda falta imprimir.
