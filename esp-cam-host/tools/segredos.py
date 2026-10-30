"""Gera src/segredos.h a partir de secrets.ini (fora do Git).

Rodado pelo PlatformIO antes de compilar (extra_scripts = pre:). Sem o
secrets.ini, as credenciais ficam vazias e o firmware sobe sem Wi-Fi.
"""
import configparser
import os

Import("env")  # noqa: F821  (fornecido pelo PlatformIO)

PROJ = env.subst("$PROJECT_DIR")  # noqa: F821
CHAVES = ("wifi_ssid", "wifi_senha", "web_senha")


def literal_c(valor):
    return '"' + valor.replace("\\", "\\\\").replace('"', '\\"') + '"'


cp = configparser.ConfigParser()
cp.read(os.path.join(PROJ, "secrets.ini"), encoding="utf-8")
sec = cp["segredos"] if cp.has_section("segredos") else {}

linhas = ["// Gerado por tools/segredos.py a partir de secrets.ini. Nao versionar.",
          "#pragma once"]
for k in CHAVES:
    linhas.append(f"#define {k.upper()} {literal_c(sec.get(k, '').strip())}")
novo = "\n".join(linhas) + "\n"

saida = os.path.join(PROJ, "src", "segredos.h")
try:
    atual = open(saida, encoding="utf-8").read()
except OSError:
    atual = None
if atual != novo:            # so reescreve se mudou: evita recompilar tudo
    with open(saida, "w", encoding="utf-8") as f:
        f.write(novo)
