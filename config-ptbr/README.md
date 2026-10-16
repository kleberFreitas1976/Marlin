# Configurações traduzidas (pt-BR) — somente leitura

Estes arquivos são **cópias traduzidas** de `Marlin/Configuration.h` e
`Marlin/Configuration_adv.h`, feitas para facilitar o entendimento.

**A compilação NÃO usa estes arquivos.** O PlatformIO lê exclusivamente os
originais em `Marlin/`. Esta pasta é material de referência.

Os valores (`#define`) são idênticos aos dos originais no momento da tradução —
só os comentários estão em português. Se você alterar algo aqui, nada muda no
firmware; edite o arquivo original em `Marlin/`.

Como esta pasta contém apenas arquivos novos, ela não conflita com atualizações
futuras do Marlin (`git merge`). Para o Git ignorá-la por completo, adicione a
linha `config-ptbr/` em `.git/info/exclude` — isso é local e não altera nenhum
arquivo versionado.

| Arquivo | Original | Estado |
|---|---|---|
| `Configuration.pt-BR.h` | `Marlin/Configuration.h` | **completo** |
| `Configuration_adv.pt-BR.h` | `Marlin/Configuration_adv.h` | ainda não iniciado |

## Verificação de fidelidade

O `Configuration.pt-BR.h` foi conferido automaticamente contra o original:
**477 defines ativos nos dois arquivos, sem nenhum faltando nem divergente.**
Os valores refletem o firmware gravado na placa.

Se você alterar o `Marlin/Configuration.h`, a cópia traduzida fica defasada.
Para reconferir, compare os `#define` ativos dos dois arquivos.

No catálogo de displays (a última parte do arquivo) foram traduzidas as notas
e instruções, mantendo nomes de produto, modelos e URLs no original — não há
o que traduzir neles.

Traduzido a partir do Marlin bugfix-2.1.x, `STRING_DISTRIBUTION_DATE 2026-10-01`.
