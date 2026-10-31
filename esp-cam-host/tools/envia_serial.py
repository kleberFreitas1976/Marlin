#!/usr/bin/env python3
"""Manda um G-code para o cartao SD da ESP32-S3-CAM pela serial (sem Wi-Fi).

Uso:  python tools/envia_serial.py arquivo.gcode [--porta COM8]

Uma linha de base64 por vez, cada uma confirmada pela placa ("+"): lento
(~10 KB/s), mas serve para arquivos de teste e quando a rede nao ajuda.
"""
import argparse
import base64
import os
import sys
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("arquivo")
ap.add_argument("--porta", default="COM8")
ap.add_argument("--baud", type=int, default=921600)
a = ap.parse_args()

dados = open(a.arquivo, "rb").read()
nome = os.path.basename(a.arquivo)
s = serial.Serial()
s.port, s.baudrate, s.timeout = a.porta, a.baud, 2
s.dtr = s.rts = False
s.open()
s.reset_input_buffer()


def espera(*marcas):
    fim = time.time() + 10
    while time.time() < fim:
        l = s.readline().decode("ascii", "ignore").strip()
        for m in marcas:
            if l.startswith(m):
                return l
    sys.exit("sem resposta da placa")


s.write(f"recebe {nome}\n".encode())
r = espera("@@PRONTO", "@@FALHOU")
if r.startswith("@@FALHOU"):
    sys.exit("placa: " + r)
for i in range(0, len(dados), 57):
    s.write(base64.b64encode(dados[i:i + 57]) + b"\n")
    if espera("+", "@@FALHOU").startswith("@@FALHOU"):
        sys.exit("a placa recusou o trecho no byte %d" % i)
s.write(b"@@FIM\n")
print(nome, len(dados), "bytes:", espera("@@RECEBIDO", "@@FALHOU"))
s.close()
