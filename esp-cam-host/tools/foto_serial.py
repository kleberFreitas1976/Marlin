#!/usr/bin/env python3
"""Pede uma foto a ESP32-S3-CAM pelo console serial e salva em JPEG.

Uso:  python tools/foto_serial.py [resolucao] [arquivo.jpg] [--porta COM8]
      resolucao: vga, svga, xga, hd, sxga, uxga, qxga, qsxga (padrao: a atual)

Serve para testar a camera sem Wi-Fi. A porta abre com DTR/RTS desligados, para
nao reiniciar a placa.
"""
import argparse
import base64
import re
import sys
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("resolucao", nargs="?", default="")
ap.add_argument("arquivo", nargs="?", default="foto.jpg")
ap.add_argument("--porta", default="COM8")
ap.add_argument("--baud", type=int, default=921600)
a = ap.parse_args()

s = serial.Serial()
s.port, s.baudrate, s.timeout = a.porta, a.baud, 0.5
s.dtr = s.rts = False
s.open()
s.reset_input_buffer()
s.write(f"foto {a.resolucao}\n".encode())

B64 = re.compile(r"^[A-Za-z0-9+/]{1,76}={0,2}$")
dados, dentro, info, intrusas = [], False, "", []
fim = time.time() + 90
while time.time() < fim:
    linha = s.readline().decode("ascii", "ignore").strip()
    if not linha:
        continue
    if linha.startswith("@@FOTO INICIO"):
        dentro, info = True, linha[14:]
    elif linha.startswith("@@FOTO FIM"):
        break
    elif dentro:
        if B64.match(linha):
            dados.append(linha)
        else:
            intrusas.append(linha)     # log da placa no meio da foto
    elif "nao" in linha or "falha" in linha or "desconhecida" in linha:
        sys.exit("placa: " + linha)
s.close()

if not dados:
    sys.exit("nenhuma foto recebida")
jpg = base64.b64decode("".join(dados))
for l in intrusas[:5]:
    print("linha ignorada:", l[:100])
esperado = int(info.split()[0]) if info else len(jpg)
if len(jpg) != esperado:
    print(f"AVISO: recebidos {len(jpg)} bytes, a placa mandou {esperado}")
open(a.arquivo, "wb").write(jpg)
print(f"salvo {a.arquivo}: {len(jpg)} bytes ({info})")
