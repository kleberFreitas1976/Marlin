#!/usr/bin/env python3
"""Imita o api.php da hospedagem na rede local, para testar a nuvem da placa.

Uso:  python tools/nuvem_teste.py [porta]       (padrao 8090)
      no console serial da placa:  nuvemteste http://<IP deste PC>:8090/api.php

Recebe o status, guarda a ultima foto em .pio/nuvem_teste.jpg e, no primeiro
contato, devolve um comando "foto" para conferir o caminho de volta.
"""
import json
import os
import sys
import time
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PASTA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".pio")
PENDENTES = [{"id": "teste1", "acao": "foto", "forcar": False}]


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def responde(self, d):
        b = json.dumps(d).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def do_POST(self):
        q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
        corpo = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        acao = q.get("acao", [""])[0]
        chave = self.headers.get("X-Chave", "")
        hora = time.strftime("%H:%M:%S")
        if acao == "status":
            e = json.loads(corpo)
            tb = e.get("tb", {})
            print(f"{hora} status (chave {chave!r}): bico {tb.get('bico')} mesa {tb.get('mesa')} "
                  f"imp {e.get('imp', {}).get('estado')} rele {e.get('rl', {})}", flush=True)
            cmds = PENDENTES[:]
            PENDENTES.clear()
            return self.responde({"ok": True, "comandos": cmds})
        if acao == "foto":
            open(os.path.join(PASTA, "nuvem_teste.jpg"), "wb").write(corpo)
            inteiro = corpo[:2] == bytes([0xFF, 0xD8]) and corpo[-2:] == bytes([0xFF, 0xD9])
            print(f"{hora} foto recebida: {len(corpo)} bytes, JPEG inteiro={inteiro}", flush=True)
            return self.responde({"ok": True})
        if acao == "feito":
            print(f"{hora} comando confirmado: {q}", flush=True)
            return self.responde({"ok": True})
        self.responde({"erro": "acao"})


porta = int(sys.argv[1]) if len(sys.argv) > 1 else 8090
print(f"api de teste em http://0.0.0.0:{porta}/api.php", flush=True)
ThreadingHTTPServer(("0.0.0.0", porta), H).serve_forever()
