# Teste de retracao: 4 pilares ocos de 8x8 mm, 40 mm entre centros. Em cada camada o
# bico visita os 4 com retracao, z-hop e avanco iguais aos do fatiador, ~1 retracao a
# cada 3 s (tao denso quanto a primeira camada do soldado). A retracao sobe em faixas
# de 5 mm: se o bico travar, a faixa diz em que comprimento; se nao travar, o fio entre
# os pilares diz o menor comprimento que basta.
#
# Uso: python gera_teste_retracao.py <saida.gcode> [diametro] [faixas mm, ex.: 2,3,4,5,6]
import math, sys

SAIDA = sys.argv[1]
DIA = float(sys.argv[2]) if len(sys.argv) > 2 else 1.88
FAIXAS = [float(v) for v in sys.argv[3].split(",")] if len(sys.argv) > 3 else [2, 3, 4, 5, 6]

BICO, MESA = 205, 60
BICO_1 = 215
H1, H = 0.3, 0.2                 # primeira camada e demais
W1, W = 0.5, 0.45                # largura da linha
FAIXA_MM = 5.0
BASE_MM = 1.0                    # camadas de base com a retracao do fatiador (6 mm)
LADO, PERIM = 8.0, 3
CX, CY, PASSO = 100.0, 90.0, 40.0
V1, V, VIAGEM = 20, 35, 95       # mm/s
RET_V, AVA_V, ZHOP = 40, 30, 0.2

AREA_FIL = math.pi * DIA * DIA / 4
def e_por_mm(w, h):
    return ((w - h) * h + math.pi * (h / 2) ** 2) / AREA_FIL

g = []
def o(s=""): g.append(s)

centros = [(CX - PASSO / 2, CY - PASSO / 2), (CX + PASSO / 2, CY - PASSO / 2),
           (CX + PASSO / 2, CY + PASSO / 2), (CX - PASSO / 2, CY + PASSO / 2)]
altura = BASE_MM + FAIXA_MM * len(FAIXAS)
n_camadas = 1 + round((altura - H1) / H)

o(f"; Teste de retracao: faixas {FAIXAS} mm, {FAIXA_MM:g} mm cada, base de {BASE_MM:g} mm com 6 mm")
o(f"; filamento {DIA} mm, bico {BICO_1}/{BICO}, mesa {MESA}, {n_camadas} camadas")
o("M118 P1 TL:INICIO teste_retracao ; avisa a ESP32-S3-CAM")
o("M201 E1000 ; limita aceleracao do extrusor (o firmware usa 10000 e a BMG patina)")
o("M83 ; extrusao relativa")
o(f"M140 S{MESA}")
o("M104 S150 ; bico em espera durante o home")
o("G28")
o("M420 S1 ; usa a malha que ja esta na memoria (mesa nao mexida)")
o("G1 Z5 F600")
o("G1 X10 Y10 F3000 ; espera no canto")
o(f"M190 S{MESA} ; espera a mesa com o bico ainda a 150")
o(f"M104 S{BICO_1}")
o(f"M109 S{BICO_1}")
o("G1 X40 Y2 F1500")
o(f"G1 X55 Y2 Z{H1} F900 ; rampa ate a purga")
o("G1 X140 Y2 E8 F1000 ; purga")
o("G1 Y2.6 F3000")
o("G1 X70 E4 F1000")
o("G1 Y3.2 F3000")
o("G1 X140 E4 F1000")
o("G1 Z2 F600")
o("M106 S0")

z = 0.0
ret_atual = None
for n in range(n_camadas):
    h = H1 if n == 0 else H
    z = round(z + h, 3)
    w = W1 if n == 0 else W
    v = V1 if n == 0 else V
    epm = e_por_mm(w, h)
    if z <= BASE_MM + 1e-6:
        ret = 6.0
    else:
        ret = FAIXAS[min(int((z - BASE_MM - 1e-6) // FAIXA_MM), len(FAIXAS) - 1)]
    if ret != ret_atual:
        o(f"M117 Retracao {ret:g} mm")
        o(f"; ===== retracao {ret:g} mm a partir de Z{z:g} =====")
        ret_atual = ret
    if n == 1:
        o("M106 S255")
        o(f"M104 S{BICO}")
    o(f";LAYER_CHANGE\n;Z:{z:g}")
    o(f"M118 P1 TL:CAMADA {n + 1}/{n_camadas}")
    for i, (cx, cy) in enumerate(centros):
        # viagem com retracao, z-hop e avanco
        x0 = cx - LADO / 2 + w / 2
        y0 = cy - LADO / 2 + w / 2
        o(f"G1 E-{ret:g} F{RET_V * 60}")
        o(f"G1 Z{z + ZHOP:.3f} F600")
        o(f"G1 X{x0:.3f} Y{y0:.3f} F{VIAGEM * 60}")
        o(f"G1 Z{z:.3f} F600")
        o(f"G1 E{ret:g} F{AVA_V * 60}")
        o(f"G1 F{v * 60}")
        for p in range(PERIM):
            a = p * w
            x1, y1 = cx - LADO / 2 + w / 2 + a, cy - LADO / 2 + w / 2 + a
            x2, y2 = cx + LADO / 2 - w / 2 - a, cy + LADO / 2 - w / 2 - a
            if p: o(f"G1 X{x1:.3f} Y{y1:.3f}")
            for (xa, ya, xb, yb) in ((x1, y1, x2, y1), (x2, y1, x2, y2), (x2, y2, x1, y2), (x1, y2, x1, y1)):
                o(f"G1 X{xb:.3f} Y{yb:.3f} E{epm * math.hypot(xb - xa, yb - ya):.5f}")

o("; ===== FIM =====")
o("M104 S0")
o("M140 S0")
o("M107")
o("G91")
o("G1 E-2 F1800")
o("G1 Z10 F600")
o("G90")
o("G1 X5 Y176 F3000")
o("M84")
o("M118 P1 TL:FIM")
open(SAIDA, "w", encoding="utf-8", newline="\n").write("\n".join(g) + "\n")
print(f"{SAIDA}: {n_camadas} camadas, altura {z:g} mm, faixas {FAIXAS}")
