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

Cada perfil sai **completo e independente**: a cadeia de herança do Orca
(`MyMarlin 0.4 nozzle`, `0.20mm Standard @MyMarlin`, `Generic PLA/PETG/ABS
@System`) é resolvida pelo script, e o resultado tem `inherits` vazio. Assim a
importação não depende de a impressora genérica estar ativada no assistente do
Orca. O que o bundle não define fica com o padrão do próprio Orca.

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

## Ainda não testado

Nenhuma peça foi fatiada com estes perfis. No primeiro fatiamento, confira o
G-code como foi feito com o PrusaSlicer: início com bico a 150 °C no home e
purga em três linhas, `M73` presente, temperaturas do filamento e
desaceleração nas saliências.
