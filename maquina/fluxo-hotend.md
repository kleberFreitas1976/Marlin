# Fluxo do hotend

Medido com o teste de fluxo do console (aba Temperatura): extrusão de 50 mm no
ar a fluxos crescentes, até a extrusora escorregar.

O valor que vai para o perfil é `filament_max_volumetric_speed`, **um por
material e por temperatura**.

| Material | Bico | Limpo até | Falhou em | Perfil (80% do limpo) | Equivale a (camada 0,2 × 0,45) |
|---|---|---|---|---|---|
| PETG | 230 °C | 7 mm³/s | 8 mm³/s | **5,6 mm³/s** | ~69 mm/s |
| PLA comum | — | *não medido* | — | 8 (estimado) | — |
| **GTMax3D PLA Speed+** | 210 °C | *não medido* | — | 8 (placeholder) | — |
| ABS | — | *não medido* | — | 8 (estimado) | — |

## Observações

- Passos rodados em 24/10/2026, PETG a 230 °C: 4, 8, 10 e 7 mm³/s. Os botões
  de marcação não foram usados, então o resultado vem da leitura do log. O
  intervalo entre 7 e 8 não foi explorado (7,5 não foi testado).
- O teste é no ar, sem a contrapressão da camada. Por isso o valor de perfil
  desconta 20%.
- O teste mede o conjunto extrusora + hotend. Não separa um do outro: se a
  BMG estalar, pode ser o hotend sem derreter rápido o bastante ou o motor do
  extrusor no limite. Para a impressão, o que vale é o conjunto.
- O fluxo de 5,6 mm³/s dá 69 mm/s de impressão em camada de 0,2 e 48 mm/s em
  camada de 0,3. O preenchimento do perfil (65 mm/s) fica abaixo do teto.
- Corrente do driver do E0 na hora do teste: Vref 1,20 V (ver
  `motores-e-drivers.md`).

## Pendências

- [ ] **Medir o GTMax3D PLA Speed+** (o filamento em uso agora). PLA de alta
      velocidade costuma aguentar mais fluxo que o PLA comum, mas só vale
      depois de medido. Os 8 mm³/s do perfil são placeholder.
- [ ] Medir o PLA comum, se voltar a usar
- [ ] Medir o ABS, se for usar
- [ ] Refazer o PETG se a temperatura de impressão mudar
