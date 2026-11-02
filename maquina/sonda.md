# Sonda de haste (servo + sensor ótico), testes de 02/11/2026

## Falha "Probing Failed"

O G30 falhava ~1 em cada 5 sondagens, em pontos variados e com a mesa fria. A falha
levava só ~4 s, contra 16 s de uma sondagem normal: o bico mal começava a descer e
o sensor "disparava" no ar (mais de 3 mm acima do esperado, `Z_PROBE_ERROR_TOLERANCE`).
Visualmente: a haste baixa e o bico sobe logo, como um pulso de ruído. Empurrando a
haste com a mão, o sensor respondeu a todos os 17 toques (sensor e cabo bons).

Correção (Configuration.h):

- `ENDSTOP_NOISE_THRESHOLD 5`: o sinal tem que ficar igual por 5 leituras (~5 ms a 1 kHz).
- `DELAY_BEFORE_PROBING 200`: a haste assenta antes de descer.

Resultado em X123 Y40, onde falhava: 20 de 20 sem falha, e a malha 5×5 inteira sem falha.

## Folga da haste

Com o bico em Z8 e a sonda acionada, a ponta fica a menos de 2 mm do vidro (medido
com paquímetro). O sensor só corta com o bico em Z4,16, então a haste sobe ~2 mm
antes de disparar. Folgas da sonda (`Z_CLEARANCE_*`) em 10 mm, que deixa ~4 mm.

## Velocidade de descida

| Descida | 20× G30 em X123 Y40 | Desvio | Malha 5×5 |
|---|---|---|---|
| 2 mm/s (folgas 10 mm) | 20/20, média 0,183 | 0,012 mm | 4 min, sem falha |
| 4 mm/s | 20/20, média 0,142, derivando | 0,017 mm | 4m24s, 2 falhas (retry) |

A 4 mm/s piorou: ficou em 2 mm/s. Antes das mudanças (folgas 15/12) a malha levava ~4m47s.
O tempo por ponto (~9,6 s) é dominado por acionar e recolher a haste em cada ponto
(`Z_SERVO_INTERMEDIATE_STOW`, 500 ms cada), subir e andar até o próximo.
