#!/usr/bin/env python3
"""Servidor de teste da interface web, sem placa e sem impressora.

Serve a pasta web/ e imita a API da ESP32-S3-CAM (/api/sessao, /api/estado,
/foto.jpg, WebSocket /ws) com uma TinyBee simulada: temperaturas que sobem ate
o alvo, posicao, respostas a G-code. So usa a biblioteca padrao do Python.

Uso:  python tools/servidor_teste.py [porta]      (padrao 8765)
      abra http://localhost:8765/
"""
import base64
import hashlib
import json
import os
import random
import re
import socket
import struct
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PASTA = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "web")
TOKEN = "teste0000000000000000000000000000"
TIPOS = {".html": "text/html; charset=utf-8", ".js": "application/javascript",
         ".css": "text/css", ".svg": "image/svg+xml", ".png": "image/png", ".jpg": "image/jpeg"}


# ---------------------------------------------------------------- TinyBee simulada
class Marlin:
    def __init__(self):
        self.bico, self.bico_alvo, self.mesa, self.mesa_alvo = 25.0, 0.0, 24.0, 0.0
        self.x, self.y, self.z, self.e = 196.0, -15.0, 0.0, 0.0
        self.vel, self.fluxo, self.comp = 100, 100, 0
        self.relativo = False
        self.malha = [[-0.235, 0.195, 0.008, 0.263, 0.263], [-0.305, 0.190, -0.075, 0.245, 0.278],
                      [-0.367, 0.077, -0.122, 0.195, 0.235], [-0.467, 0.023, -0.237, 0.113, 0.182],
                      [-0.562, -0.133, -0.293, 0.000, 0.120]]

    def passo(self, dt):
        for nome in ("bico", "mesa"):
            atual, alvo = getattr(self, nome), getattr(self, nome + "_alvo")
            amb = 24.0
            destino = alvo if alvo > 0 else amb
            taxa = 3.0 if nome == "bico" else 0.6
            novo = atual + max(-taxa, min(taxa, (destino - atual) * 0.15)) * dt
            setattr(self, nome, novo + random.uniform(-0.15, 0.15))

    def pwm(self, nome):
        alvo = getattr(self, nome + "_alvo")
        return 0 if alvo <= 0 else max(0, min(127, int((alvo - getattr(self, nome)) * 12 + 30)))

    def temps(self):
        return (f"T:{self.bico:.2f} /{self.bico_alvo:.2f} B:{self.mesa:.2f} /{self.mesa_alvo:.2f} "
                f"@:{self.pwm('bico')} B@:{self.pwm('mesa')}")

    def pos(self):
        return f"X:{self.x:.2f} Y:{self.y:.2f} Z:{self.z:.2f} E:{self.e:.2f} Count A:0B:0 Z:0"

    def comando(self, linha):
        """Devolve a lista de respostas (a ultima e o ok)."""
        g = linha.split(";")[0].strip().upper()
        if not g:
            return []
        p = dict((m[0], float(m[1:])) for m in re.findall(r"[A-Z]-?\d+\.?\d*", g[g.find(" "):])) \
            if " " in g else {}
        c = g.split()[0]
        out = []
        if c == "M105":
            return ["ok " + self.temps()]
        if c == "M114":
            out.append(self.pos())
        elif c in ("M104", "M109"):
            self.bico_alvo = p.get("S", 0)
        elif c in ("M140", "M190"):
            self.mesa_alvo = p.get("S", 0)
        elif c == "G28":
            self.x, self.y, self.z = 196.0, -15.0, 4.16
            out.append(self.pos())
        elif c == "G91":
            self.relativo = True
        elif c == "G90":
            self.relativo = False
        elif c in ("G0", "G1"):
            for k in "XYZE":
                if k in p:
                    v = getattr(self, k.lower())
                    setattr(self, k.lower(), v + p[k] if self.relativo or k == "E" else p[k])
        elif c == "M220":
            if "S" in p:
                self.vel = int(p["S"])
            out.append(f"FR:{self.vel}%")
        elif c == "M221":
            if "S" in p:
                self.fluxo = int(p["S"])
            out.append(f"echo:E0 Flow: {self.fluxo}%")
        elif c == "M420":
            if "S" in p:
                self.comp = int(p["S"])
            if "V" in g:
                out.append("Bilinear Leveling Grid:")
                out.append("      0      1      2      3      4")
                for j, l in enumerate(self.malha):
                    out.append(f" {j} " + " ".join(f"{v:+.3f}" for v in l))
                out.append("")
            out.append("echo:Bed Leveling " + ("ON" if self.comp else "OFF"))
            out.append("echo:Fade Height 10.00")
        elif c == "M119":
            out += ["Reporting endstop status", "x_max: open", "y_min: open",
                    "z_probe: TRIGGERED", "filament: open"]
        elif c == "M115":
            out.append("FIRMWARE_NAME:Marlin simulado (servidor_teste.py) SOURCE_CODE_URL:x")
            out.append("area:{full:{min:{x:0.00,y:-15.00,z:0.00},max:{x:196.00,y:181.00,z:250.00}}}")
        elif c in ("M155", "M154", "M84", "M106", "M107", "M83", "M82", "M500", "M501", "G4"):
            pass
        else:
            out.append(f"echo:Unknown command: \"{linha.strip()}\"")
        return out + ["ok"]


MARLIN = Marlin()
CLIENTES = set()
TRAVA = threading.Lock()


def envia_ws(sock, texto):
    dados = texto.encode()
    n = len(dados)
    cab = bytes([0x81]) + (bytes([n]) if n < 126 else
                           bytes([126]) + struct.pack(">H", n) if n < 65536 else
                           bytes([127]) + struct.pack(">Q", n))
    sock.sendall(cab + dados)


def difunde(obj):
    txt = json.dumps(obj, ensure_ascii=False)
    with TRAVA:
        for c in list(CLIENTES):
            try:
                envia_ws(c, txt)
            except OSError:
                CLIENTES.discard(c)


def linha(k, s):
    difunde({"t": "l", "k": k, "ms": int(time.time() * 1000), "s": s})


def estado():
    m = MARLIN
    return {"t": "e", "tb": {"usb": True, "resp": True, "fw": "Marlin simulado (servidor_teste.py)",
                             "ocupada": False, "fila": 0, "bico": round(m.bico, 1),
                             "bicoAlvo": m.bico_alvo, "bicoPwm": m.pwm("bico"),
                             "mesa": round(m.mesa, 1), "mesaAlvo": m.mesa_alvo,
                             "mesaPwm": m.pwm("mesa"), "x": m.x, "y": m.y, "z": m.z, "e": m.e,
                             "vel": m.vel, "fluxo": m.fluxo, "comp": m.comp,
                             "min": [0, -15, 0], "max": [196, 181, 250]},
            "cam": {"sensor": "OV5640", "ligada": False, "larg": 2560, "alt": 1920},
            "sd": {"ok": True, "livreMB": 30421, "totalMB": 30421},
            "rede": {"ssid": "Simulada", "ip": "127.0.0.1", "rssi": -55, "ap": False},
            "sis": {"seg": int(time.time() - INICIO), "ram": 150, "psram": 7800}}


INICIO = time.time()


def relogio():
    while True:
        time.sleep(1)
        MARLIN.passo(1)
        difunde(estado())


def executa(texto):
    for l in texto.split("\n"):
        if l == "!m112":
            linha("err", "M112: PARADA DE EMERGENCIA (simulada)")
            continue
        if l == "!limpa":
            linha("sis", "Fila limpa (0 comandos descartados)")
            continue
        if not l.strip():
            continue
        linha("tx", l.strip())
        time.sleep(0.05)
        for r in MARLIN.comando(l):
            k = "ok" if r.startswith("ok") else "err" if r.startswith("Error") else \
                "sis" if r.startswith("echo:") else "rx"
            if r:
                linha(k, r)


# ---------------------------------------------------------------- HTTP
class Manipulador(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def responde(self, codigo, tipo, corpo):
        self.send_response(codigo)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(corpo)

    def do_GET(self):
        caminho = self.path.split("?")[0]
        if caminho == "/ws":
            return self.websocket()
        if caminho == "/api/sessao":
            return self.responde(200, "application/json", json.dumps({"token": TOKEN}).encode())
        if caminho == "/api/estado":
            return self.responde(200, "application/json", json.dumps(estado()).encode())
        if caminho == "/foto.jpg":
            foto = os.environ.get("FOTO_TESTE", "")
            if foto and os.path.exists(foto):
                return self.responde(200, "image/jpeg", open(foto, "rb").read())
            return self.responde(404, "text/plain", b"defina FOTO_TESTE=arquivo.jpg")
        if caminho == "/":
            caminho = "/index.html"
        arq = os.path.normpath(os.path.join(PASTA, caminho.lstrip("/")))
        if not arq.startswith(PASTA) or not os.path.isfile(arq):
            return self.responde(404, "text/plain", b"nao encontrado")
        self.responde(200, TIPOS.get(os.path.splitext(arq)[1], "application/octet-stream"),
                      open(arq, "rb").read())

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        corpo = self.rfile.read(n).decode()
        if self.path == "/api/gcode":
            threading.Thread(target=executa, args=(corpo,), daemon=True).start()
            return self.responde(200, "application/json", b'{"enfileirados":1}')
        self.responde(404, "text/plain", b"nao encontrado")

    def websocket(self):
        if TOKEN not in self.path:
            return self.responde(403, "text/plain", b"token")
        chave = self.headers["Sec-WebSocket-Key"]
        aceite = base64.b64encode(hashlib.sha1(
            (chave + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
        self.send_response(101)
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", aceite)
        self.end_headers()
        sock = self.connection
        envia_ws(sock, json.dumps(estado()))
        with TRAVA:
            CLIENTES.add(sock)
        try:
            while True:
                cab = sock.recv(2)
                if len(cab) < 2:
                    break
                op, n = cab[0] & 0x0F, cab[1] & 0x7F
                if n == 126:
                    n = struct.unpack(">H", sock.recv(2))[0]
                elif n == 127:
                    n = struct.unpack(">Q", sock.recv(8))[0]
                masc = sock.recv(4)
                dados = b""
                while len(dados) < n:
                    dados += sock.recv(n - len(dados))
                dados = bytes(b ^ masc[i % 4] for i, b in enumerate(dados))
                if op == 8:
                    break
                if op == 1:
                    threading.Thread(target=executa, args=(dados.decode(),), daemon=True).start()
        except OSError:
            pass
        with TRAVA:
            CLIENTES.discard(sock)


if __name__ == "__main__":
    porta = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    threading.Thread(target=relogio, daemon=True).start()
    srv = ThreadingHTTPServer(("127.0.0.1", porta), Manipulador)
    print(f"interface de teste em http://localhost:{porta}/  (Ctrl+C para sair)")
    srv.serve_forever()
