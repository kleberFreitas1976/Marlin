"""Embute a interface web (pasta web/) no firmware.

Rodado pelo PlatformIO antes de compilar (extra_scripts = pre:). Comprime cada
arquivo com gzip e gera src/web_arquivos.c (fora do Git) com uma tabela
caminho -> conteudo. O navegador descomprime sozinho (Content-Encoding: gzip).
"""
import gzip
import hashlib
import os

try:
    Import("env")  # noqa: F821  (fornecido pelo PlatformIO)
    PROJ = env.subst("$PROJECT_DIR")  # noqa: F821
except NameError:  # rodando avulso: python tools/gera_web.py
    PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PASTA = os.path.join(PROJ, "web")
SAIDA = os.path.join(PROJ, "src", "web_arquivos.c")
TIPOS = {
    ".html": "text/html; charset=utf-8", ".js": "application/javascript; charset=utf-8",
    ".css": "text/css; charset=utf-8", ".svg": "image/svg+xml", ".png": "image/png",
    ".ico": "image/x-icon", ".json": "application/json", ".woff2": "font/woff2",
}

arquivos = []
for raiz, _, nomes in os.walk(PASTA):
    for nome in sorted(nomes):
        caminho = os.path.join(raiz, nome)
        rel = "/" + os.path.relpath(caminho, PASTA).replace(os.sep, "/")
        dados = open(caminho, "rb").read()
        arquivos.append((rel, TIPOS.get(os.path.splitext(nome)[1].lower(),
                                         "application/octet-stream"), dados))
arquivos.sort()

linhas = ["// Gerado por tools/gera_web.py a partir de web/. Nao editar nem versionar.",
          '#include "web_arquivos.h"', ""]
tabela = []
for i, (rel, tipo, dados) in enumerate(arquivos):
    gz = gzip.compress(dados, compresslevel=9, mtime=0)
    etag = hashlib.sha1(dados).hexdigest()[:16]
    linhas.append(f"static const uint8_t a{i}[] = {{  // {rel}: {len(dados)} -> {len(gz)} bytes")
    for k in range(0, len(gz), 24):
        linhas.append("    " + ",".join(str(b) for b in gz[k:k + 24]) + ",")
    linhas.append("};")
    tabela.append(f'    {{"{rel}", "{tipo}", a{i}, sizeof a{i}, "\\"{etag}\\""}},')
linhas.append("")
linhas.append("const web_arquivo_t WEB_ARQUIVOS[] = {")
linhas += tabela
linhas.append("};")
linhas.append(f"const size_t WEB_N_ARQUIVOS = {len(arquivos)};")
novo = "\n".join(linhas) + "\n"

try:
    atual = open(SAIDA, encoding="utf-8").read()
except OSError:
    atual = None
if atual != novo:
    with open(SAIDA, "w", encoding="utf-8") as f:
        f.write(novo)
    total = sum(len(gzip.compress(d, 9, mtime=0)) for _, _, d in arquivos)
    print(f"web: {len(arquivos)} arquivos, {total // 1024} KB comprimidos")
