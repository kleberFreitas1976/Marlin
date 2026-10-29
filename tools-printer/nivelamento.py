#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Tela de nivelamento da mesa — MKS TinyBee / CoreXY
==================================================
Janela aberta pelo console (marlin_console.py). Mostra a malha como um mapa da
mesa, com um card colorido em cada ponto, e calcula quantas voltas dar em cada
mola para tirar a inclinacao.

Toda a analise roda aqui, sem internet: ajuste de plano por minimos quadrados,
residuo (o que as molas nao corrigem), pontos suspeitos, repetibilidade entre
medicoes e conversao de altura em voltas. A IA local (Ollama) e opcional e so
complementa o diagnostico.

O que o firmware deixa mudar sem regravar
  - AREA da malha: sim. O G29 aceita L R F B (G29.cpp:426) e o M500 grava o
    inicio e o espacamento da grade junto com a malha (settings.cpp:1121).
  - NUMERO de pontos do G29: nao. No nivelamento bilinear a grade e fixa em
    GRID_MAX_POINTS_X x GRID_MAX_POINTS_Y (5 x 5); so o LINEAR aceita P/X/Y.
    Para ajustar as molas existe a medicao rapida por G30 (3x3, 4x4 ou 5x5),
    que nao substitui a malha usada na impressao.

Pode ser executado sozinho para ver a tela sem impressora:
    python nivelamento.py
"""

import json
import math
import queue
import re
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path
import tkinter as tk
from tkinter import messagebox, ttk

# ---------------------------------------------------------------------------
# Geometria da maquina (espelha Configuration.h / Configuration_adv.h)
# ---------------------------------------------------------------------------

MESA_X = 196.0           # X_BED_SIZE
MESA_Y = 181.0           # Y_BED_SIZE
Y_HOME = -15.0           # Y_MIN_POS: do home ate Y=0 ficam as presilhas do vidro

# Alcance do probe (probe.h:256-262): o mais restritivo entre a margem e o
# limite do carro somado ao offset do probe { -54, -10 }.
SONDA_X_MIN, SONDA_X_MAX = 15.0, 142.0
SONDA_Y_MIN, SONDA_Y_MAX = 15.0, 166.0

GRADE_FIRMWARE = 5       # GRID_MAX_POINTS_X / Y: fixo na compilacao
ALCANCE_BUSCA = 3.6      # -Z_PROBE_LOW_POINT: abaixo disso o probe desiste

ARQ_DADOS = Path(__file__).resolve().parent / "nivelamento.json"
MAX_HISTORICO = 60

# Rendimento das molas. Medido nas malhas 3x3 de 25/10/2026: uma volta no
# fundo-esquerda mudou a inclinacao em Y 0,0025 e 0,0020 mm/mm, e uma volta em
# cada parafuso da direita mudou a inclinacao em X 0,0049 mm/mm. Com os
# parafusos modelados nos cantos da area util (196 x 181), isso equivale a
# ~0,85 mm por volta -- mais que o passo do M3 (0,5 mm), porque os parafusos
# reais ficam mais para dentro e a mesa flexiona. O numero vale para as
# posicoes padrao abaixo; mudou as posicoes, recalibre na aba Molas.
MM_POR_VOLTA_PADRAO = 0.85
PASSO_M3 = 0.5

MOLAS_NOMES = {"FE": "Frente esquerda", "FD": "Frente direita",
               "TE": "Fundo esquerda", "TD": "Fundo direita"}
MOLAS_PADRAO = {"FE": [0.0, 0.0], "FD": [MESA_X, 0.0],
                "TE": [0.0, MESA_Y], "TD": [MESA_X, MESA_Y]}

# Faixas de cor pelo desvio absoluto (mm). A primeira camada tem 0,2 mm:
# ate 0,1 mm de erro ela ainda gruda bem; acima de 0,3 o erro passa de 1 camada.
FAIXAS = [
    (0.05, "#1f9d55", "excelente", "até 0,05"),
    (0.10, "#7bc043", "bom", "0,05 – 0,10"),
    (0.20, "#f2c12e", "aceitável", "0,10 – 0,20"),
    (0.30, "#f08a24", "ruim", "0,20 – 0,30"),
    (math.inf, "#e03b3b", "crítico", "acima de 0,30"),
]

# Paleta da janela
C_FUNDO = "#16191d"
C_PAINEL = "#1f242a"
C_PAINEL2 = "#272d34"
C_BORDA = "#3a424c"
C_TEXTO = "#e6e9ec"
C_SUAVE = "#9aa4ae"
C_ACENTO = "#4aa3ff"
C_VIDRO = "#2a3139"
C_OK = "#1f9d55"
C_AVISO = "#f2c12e"
C_ERRO = "#e03b3b"

FONTE = "Segoe UI"

RE_GRADE = re.compile(r"Bilinear Leveling Grid", re.I)
RE_CABECALHO = re.compile(r"^\s*0(\s+\d+)+\s*$")
RE_LINHA = re.compile(r"^\s*(\d+)((?:\s+(?:[+-]?\d+\.\d+|=+))+)\s*$")
RE_G30 = re.compile(r"Bed X:\s*(-?[\d.]+)\s+Y:\s*(-?[\d.]+)\s+Z:\s*(-?[\d.]+)")
RE_FALHA = re.compile(r"probing failed|probe past bed|z probe out|"
                      r"home xyz first|out of bounds|err.*probe", re.I)


# ---------------------------------------------------------------------------
# Utilidades numericas (sem numpy: o console so depende de pyserial)
# ---------------------------------------------------------------------------

def br(v, nd=2, sinal=False):
    """Numero no formato brasileiro."""
    s = f"{v:+.{nd}f}" if sinal else f"{v:.{nd}f}"
    return s.replace(".", ",")


def linspace(a, b, n):
    if n == 1:
        return [a]
    return [a + (b - a) * i / (n - 1) for i in range(n)]


def resolve(M, v):
    """Eliminacao de Gauss com pivoteamento parcial."""
    n = len(v)
    A = [list(M[i]) + [v[i]] for i in range(n)]
    for i in range(n):
        p = max(range(i, n), key=lambda k: abs(A[k][i]))
        if abs(A[p][i]) < 1e-12:
            raise ValueError("sistema singular")
        A[i], A[p] = A[p], A[i]
        for k in range(n):
            if k != i:
                f = A[k][i] / A[i][i]
                A[k] = [A[k][j] - f * A[i][j] for j in range(n + 1)]
    return [A[i][n] / A[i][i] for i in range(n)]


def ajusta_plano(pontos):
    """Plano z = a + b*x + c*y por minimos quadrados. Devolve (a, b, c)."""
    S = [[0.0] * 3 for _ in range(3)]
    v = [0.0] * 3
    for x, y, z in pontos:
        r = (1.0, x, y)
        for i in range(3):
            v[i] += r[i] * z
            for j in range(3):
                S[i][j] += r[i] * r[j]
    return tuple(resolve(S, v))


def mediana(vals):
    s = sorted(vals)
    n = len(s)
    if not n:
        return 0.0
    return s[n // 2] if n % 2 else (s[n // 2 - 1] + s[n // 2]) / 2


def faixa(desvio):
    a = abs(desvio)
    for i, (lim, cor, nome, _) in enumerate(FAIXAS):
        if a <= lim:
            return i, cor, nome
    return len(FAIXAS) - 1, FAIXAS[-1][1], FAIXAS[-1][2]


def _hex(c):
    return tuple(int(c[i:i + 2], 16) for i in (1, 3, 5))


def mistura(c1, c2, t):
    a, b = _hex(c1), _hex(c2)
    return "#%02x%02x%02x" % tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def cor_continua(desvio):
    """Cor interpolada entre as faixas, para o mapa de fundo."""
    a = abs(desvio)
    pontos = [(0.0, FAIXAS[0][1])] + [(FAIXAS[i][0], FAIXAS[i + 1][1])
                                      for i in range(len(FAIXAS) - 1)]
    pontos.append((0.45, FAIXAS[-1][1]))
    for (a0, c0), (a1, c1) in zip(pontos, pontos[1:]):
        if a <= a1:
            return mistura(c0, c1, (a - a0) / (a1 - a0) if a1 > a0 else 0)
    return FAIXAS[-1][1]


def texto_voltas(v):
    """0.34 -> '⅜ volta'; 1.27 -> '1 ¼ voltas'. Arredonda para 1/8."""
    oitavos = round(abs(v) * 8)
    inteiro, resto = divmod(oitavos, 8)
    frac = ["", "⅛", "¼", "⅜", "½", "⅝", "¾", "⅞"][resto]
    if inteiro and frac:
        txt = f"{inteiro} {frac}"
    elif inteiro:
        txt = str(inteiro)
    elif frac:
        txt = frac
    else:
        return "menos de ⅛ de volta"
    return txt + (" volta" if oitavos <= 8 else " voltas")


# ---------------------------------------------------------------------------
# Medicao e analise
# ---------------------------------------------------------------------------

def nova_medicao(tipo, xs, ys, z, temp_mesa=None, nota=""):
    """z[j][i]: j = indice em Y (0 = frente), i = indice em X (0 = esquerda)."""
    return {"quando": time.strftime("%Y-%m-%d %H:%M:%S"), "tipo": tipo,
            "xs": [round(x, 2) for x in xs], "ys": [round(y, 2) for y in ys],
            "z": z, "temp_mesa": temp_mesa, "nota": nota}


def mesmas_posicoes(m1, m2):
    if not m1 or not m2:
        return False
    if len(m1["xs"]) != len(m2["xs"]) or len(m1["ys"]) != len(m2["ys"]):
        return False
    return all(abs(a - b) < 0.6 for a, b in zip(m1["xs"] + m1["ys"], m2["xs"] + m2["ys"]))


def pontos_validos(m):
    return [(x, y, m["z"][j][i]) for j, y in enumerate(m["ys"])
            for i, x in enumerate(m["xs"]) if m["z"][j][i] is not None]


def plano_nas_molas(plano, molas):
    a, b, c = plano
    return {k: a + b * x + c * y for k, (x, y) in molas.items()}


def correcao_molas(plano, molas, mm_por_volta, referencia="auto"):
    """Quanto mexer em cada mola para tirar a inclinacao.

    Modelo: mesa rigida apoiada nos parafusos. A correcao de cada mola e a
    diferenca entre a altura do plano ajustado na posicao dela e a altura de
    referencia. Positivo = SUBIR a mesa naquele canto.

    referencia: 'auto' (uma mola fica parada e o giro total e o menor),
                'alto' (o canto mais alto fica parado, os outros so sobem),
                'baixo' (o canto mais baixo fica parado, os outros so descem).
    """
    z = plano_nas_molas(plano, molas)
    if referencia == "alto":
        ref = max(z, key=z.get)
    elif referencia == "baixo":
        ref = min(z, key=z.get)
    else:
        ref = min(z, key=lambda k: sum(abs(z[k] - z[o]) for o in z))
    out = {}
    for k, zk in z.items():
        delta = z[ref] - zk
        out[k] = {"mm": delta, "voltas": delta / mm_por_volta if mm_por_volta else 0.0,
                  "referencia": k == ref}
    return out


def calibra_rendimento(m0, m1, giros, molas):
    """mm por volta efetivo a partir de duas medicoes e das voltas dadas.

    giros: {mola: voltas}, positivo = subiu. Compara a mudanca da inclinacao
    medida com a que o modelo de mesa rigida preve para 1 mm/volta.
    """
    if not mesmas_posicoes(m0, m1):
        raise ValueError("as duas medicoes precisam ter os mesmos pontos")
    if not any(abs(v) > 1e-9 for v in giros.values()):
        raise ValueError("informe as voltas dadas em pelo menos uma mola")
    p0, p1 = ajusta_plano(pontos_validos(m0)), ajusta_plano(pontos_validos(m1))
    obs = (p1[1] - p0[1], p1[2] - p0[2])
    pred = ajusta_plano([(x, y, giros.get(k, 0.0)) for k, (x, y) in molas.items()])[1:]
    den = pred[0] ** 2 + pred[1] ** 2
    if den < 1e-12:
        raise ValueError("esse giro nao inclina a mesa (giros iguais nas quatro?)")
    p = (obs[0] * pred[0] + obs[1] * pred[1]) / den
    if p <= 0:
        raise ValueError("a mesa inclinou no sentido contrario: confira o sinal das voltas")
    return p


def analisa(m, cfg, anterior=None):
    """Tudo que a tela mostra, calculado a partir de uma medicao."""
    xs, ys, Z = m["xs"], m["ys"], m["z"]
    nx, ny = len(xs), len(ys)
    pts = pontos_validos(m)
    vals = [p[2] for p in pts]
    media = sum(vals) / len(vals)
    zmin, zmax = min(vals), max(vals)

    plano = ajusta_plano(pts) if len(pts) >= 3 and nx > 1 and ny > 1 else (media, 0.0, 0.0)
    a, b, c = plano
    resid = [[None if Z[j][i] is None else Z[j][i] - (a + b * xs[i] + c * ys[j])
              for i in range(nx)] for j in range(ny)]
    rv = [r for linha in resid for r in linha if r is not None]

    # pontos suspeitos: muito diferentes da media dos vizinhos, ou identicos
    # ao vizinho ate a terceira casa (sinal de leitura repetida). A comparacao
    # usa o residuo, para a inclinacao nao parecer defeito nos cantos; e exige
    # 3 vizinhos, porque num canto 2 vizinhos nao bastam para acusar ninguem.
    lap = {}
    for j in range(ny):
        for i in range(nx):
            if resid[j][i] is None:
                continue
            viz = [resid[jj][ii] for jj, ii in ((j - 1, i), (j + 1, i), (j, i - 1), (j, i + 1))
                   if 0 <= jj < ny and 0 <= ii < nx and resid[jj][ii] is not None]
            if len(viz) >= 3:
                lap[(j, i)] = resid[j][i] - sum(viz) / len(viz)
    # Numa medicao so, nao da para saber se um ponto destoante e leitura ruim ou
    # um calombo de verdade. Com uma medicao anterior dos mesmos pontos, da: o
    # que repetiu e real, o que mudou sozinho foi leitura ruim.
    ref_lap = max(0.2, 3 * mediana([abs(v) for v in lap.values()]))
    suspeitos = {}
    for (j, i), d in lap.items():
        if abs(d) > ref_lap:
            suspeitos[(j, i)] = f"{br(d, 2, True)} mm em relação aos vizinhos"

    # padrao do residuo: so em X (igual em todas as linhas), so em Y, cupula
    var_tot = sum(r * r for r in rv) or 1e-12
    col = [sum(resid[j][i] for j in range(ny) if resid[j][i] is not None) / ny for i in range(nx)]
    lin = [sum(resid[j][i] for i in range(nx) if resid[j][i] is not None) / nx for j in range(ny)]
    frac_x = sum(v * v for v in col) * ny / var_tot
    frac_y = sum(v * v for v in lin) * nx / var_tot
    borda, miolo = [], []
    for j in range(ny):
        for i in range(nx):
            if resid[j][i] is None:
                continue
            (borda if i in (0, nx - 1) or j in (0, ny - 1) else miolo).append(resid[j][i])
    cupula = (sum(miolo) / len(miolo) - sum(borda) / len(borda)) if miolo and borda else 0.0

    mm_volta = cfg.get("mm_volta", MM_POR_VOLTA_PADRAO)
    molas = cfg.get("molas", MOLAS_PADRAO)
    corr = correcao_molas(plano, molas, mm_volta, cfg.get("referencia", "auto"))

    comp = None
    if anterior is not None and mesmas_posicoes(m, anterior):
        dif = [[None if (Z[j][i] is None or anterior["z"][j][i] is None)
                else Z[j][i] - anterior["z"][j][i] for i in range(nx)] for j in range(ny)]
        # A diferenca pode ter uma parte plana (molas giradas, mesa mais quente):
        # tira o plano da diferenca e olha so o que mudou localmente. Duas
        # passadas, para um ponto ruim nao entortar o plano.
        dpts = [(xs[i], ys[j], dif[j][i]) for j in range(ny) for i in range(nx)
                if dif[j][i] is not None]
        pd = ajusta_plano(dpts) if len(dpts) >= 3 and nx > 1 and ny > 1 else \
            (mediana([p[2] for p in dpts]), 0.0, 0.0)
        for _ in range(2):
            rel = [[None if dif[j][i] is None else dif[j][i] - (pd[0] + pd[1] * xs[i] + pd[2] * ys[j])
                    for i in range(nx)] for j in range(ny)]
            ra = [abs(d) for linha in rel for d in linha if d is not None]
            # desvio robusto (MAD): um ponto ruim nao pode inflar o criterio que o detecta
            lim = max(0.08, 3 * 1.4826 * mediana(ra))
            bons_pts = [(x, y, d) for (x, y, d) in dpts
                        if abs(d - (pd[0] + pd[1] * x + pd[2] * y)) <= lim]
            if len(bons_pts) >= 3 and nx > 1 and ny > 1:
                pd = ajusta_plano(bons_pts)
        desloc = pd[0] + pd[1] * sum(xs) / nx + pd[2] * sum(ys) / ny
        incl_mudou = math.hypot(pd[1] * MESA_X, pd[2] * MESA_Y)
        fora = [(j, i) for j in range(ny) for i in range(nx)
                if rel[j][i] is not None and abs(rel[j][i]) > lim]
        bons = [abs(rel[j][i]) for j in range(ny) for i in range(nx)
                if rel[j][i] is not None and (j, i) not in fora]
        sig = math.sqrt(sum(d * d for d in bons) / len(bons)) if bons else 0.0
        # quem errou? Se o ponto era suspeito na anterior e nao e agora, a ruim era a anterior
        susp_ant = analisa(anterior, cfg)["suspeitos"]
        ruim_ant = [k for k in fora if k in susp_ant and k not in suspeitos]
        ruim_atual = [k for k in fora if k in suspeitos and k not in susp_ant]
        comp = {"desloc": desloc, "rel": rel, "sigma": sig, "quando": anterior["quando"],
                "fora": [k for k in fora if k not in ruim_ant], "ruim_anterior": ruim_ant,
                "ruim_atual": ruim_atual, "incl_mudou": incl_mudou}
        # destoava dos vizinhos mas repetiu: e a mesa, nao o probe
        reais = [k for k in suspeitos if k not in fora]
        comp["reais"] = reais
        for k in reais:
            del suspeitos[k]

    return {
        "media": media, "min": zmin, "max": zmax, "amplitude": zmax - zmin,
        "plano": plano,
        "incl_x": b * MESA_X, "incl_y": c * MESA_Y,       # na mesa inteira
        "resid": resid, "resid_amp": (max(rv) - min(rv)) if rv else 0.0,
        "frac_x": frac_x, "frac_y": frac_y, "cupula": cupula,
        "col_resid": col, "lin_resid": lin,
        "suspeitos": suspeitos, "molas": corr, "comp": comp,
        "nx": nx, "ny": ny,
    }


def diagnostico(m, an, cfg, estado):
    """Texto de diagnostico, regra a regra. Devolve [(nivel, titulo, texto)].

    nivel: 'ok', 'info', 'aviso', 'erro'. O primeiro item e o proximo passo.
    """
    out = []
    amp = an["amplitude"]
    tol = cfg.get("tolerancia", 0.05)
    xs, ys = m["xs"], m["ys"]

    # ---- repetibilidade ------------------------------------------------
    comp = an["comp"]
    if comp:
        if comp["ruim_anterior"]:
            nomes = ", ".join(f"X{xs[i]:.0f} Y{ys[j]:.0f}" for j, i in comp["ruim_anterior"][:4])
            out.append(("info", "Leitura ruim na medição anterior",
                        f"{nomes}: o valor mudou, mas agora concorda com os vizinhos e antes não "
                        f"concordava. A leitura errada era a de {comp['quando']}; esta vale."))
        if comp["ruim_atual"]:
            nomes = ", ".join(f"X{xs[i]:.0f} Y{ys[j]:.0f}" for j, i in comp["ruim_atual"][:4])
            out.append(("aviso", "Leitura ruim nesta medição",
                        f"{nomes}: o valor mudou em relação a {comp['quando']} e agora destoa dos "
                        "vizinhos, o que antes não acontecia. O probe errou nesta medição: "
                        "repita antes de gravar."))
        outros = [k for k in comp["fora"] if k not in comp["ruim_atual"]]
        if outros:
            nomes = ", ".join(f"X{xs[i]:.0f} Y{ys[j]:.0f}" for j, i in outros[:4])
            out.append(("aviso", "Pontos que não repetiram",
                        f"Comparado com {comp['quando']}, {len(outros)} ponto(s) mudaram além "
                        f"do normal ({nomes}). Repita a medição antes de girar as molas: a "
                        "terceira medição mostra qual das duas estava certa."))
        if comp["incl_mudou"] > 0.1:
            out.append(("info", "A inclinação mudou",
                        f"Entre as duas medições a mesa inclinou {br(comp['incl_mudou'])} mm de "
                        "um lado a outro (molas mexidas?). A comparação ponto a ponto já "
                        "desconta isso. Para calibrar o rendimento das molas, informe as voltas "
                        "na aba Molas."))
        nivel = "ok" if comp["sigma"] <= 0.03 else ("info" if comp["sigma"] <= 0.06 else "aviso")
        out.append((nivel, "Repetibilidade",
                    f"Descontado o deslocamento geral de {br(comp['desloc'], 3, True)} mm, os "
                    f"pontos variaram ±{br(comp['sigma'], 3)} mm em relação a {comp['quando']}."
                    + (" O probe está confiável." if comp["sigma"] <= 0.03 else "")))
        if abs(comp["desloc"]) > 0.05:
            out.append(("info", "A mesa inteira mudou de altura",
                        f"Todos os pontos andaram {br(comp['desloc'], 2, True)} mm juntos. Isso "
                        "costuma ser temperatura (mesa, bico ou probe) ou uma mexida nas molas. "
                        "Meça sempre na mesma condição, de preferência com a mesa quente."))

    # ---- pontos suspeitos ----------------------------------------------
    if an["suspeitos"]:
        lst = "; ".join(f"X{xs[i]:.0f} Y{ys[j]:.0f}: {t}"
                        for (j, i), t in list(an["suspeitos"].items())[:4])
        out.append(("aviso", "Pontos que destoam dos vizinhos",
                    lst + ". Pode ser leitura ruim do probe ou um calombo/vale de verdade. "
                    "Repita a medição: a tela compara as duas e diz qual é. Marcados com ⚠."))
    if comp and comp.get("reais"):
        lst = ", ".join(f"X{xs[i]:.0f} Y{ys[j]:.0f}" for j, i in comp["reais"][:4])
        out.append(("info", "Destoam, mas repetiram",
                    f"{lst}: são diferentes dos vizinhos nas duas medições, então é a forma da "
                    "mesa e não erro do probe. A compensação cuida disso."))

    # ---- inclinacao ----------------------------------------------------
    ix, iy = an["incl_x"], an["incl_y"]
    molas = an["molas"]
    giro_max = max(abs(v["mm"]) for v in molas.values())
    if giro_max > tol:
        lado_x = "a direita" if ix > 0 else "a esquerda"
        lado_y = "o fundo está" if iy > 0 else "a frente está"
        alto_y = "mais alto" if iy > 0 else "mais alta"
        out.append(("aviso" if giro_max > 0.15 else "info", "Inclinação (as molas corrigem)",
                    f"De um lado a outro da mesa, {lado_x} está {br(abs(ix))} mm mais alta "
                    f"(eixo X) e {lado_y} {br(abs(iy))} mm {alto_y} (eixo Y)."))
    else:
        out.append(("ok", "Inclinação", "A mesa está no plano: as molas não precisam de ajuste."))

    # ---- residuo -------------------------------------------------------
    ra = an["resid_amp"]
    if ra > 0.15:
        txt = (f"Descontada a inclinação, sobra {br(ra)} mm de diferença que as molas não "
               "corrigem. ")
        if an["frac_x"] > 0.6:
            txt += ("O padrão se repete em todas as linhas e muda só com o X: aponta para o "
                    "eixo X (guia, carro) variando de altura ao longo do curso, ou o vidro "
                    "ondulado nessa direção. ")
        elif an["frac_y"] > 0.6:
            txt += ("O padrão se repete em todas as colunas e muda só com o Y: aponta para a "
                    "guia do Y ou para o vidro ondulado nessa direção. ")
        elif abs(an["cupula"]) > 0.08:
            txt += ("O centro está " + ("mais alto" if an["cupula"] > 0 else "mais baixo") +
                    " que as bordas: a placa da mesa está " +
                    ("abaulada" if an["cupula"] > 0 else "côncava") +
                    ", comum quando a mesa aquece. ")
        txt += "A compensação do firmware (M420 S1) cobre isso na impressão."
        out.append(("info" if ra <= 0.3 else "aviso", "Ondulação", txt))

    # ---- alcance -------------------------------------------------------
    if amp > ALCANCE_BUSCA * 0.6:
        out.append(("erro", "Perto do limite do probe",
                    f"A amplitude ({br(amp)} mm) está perto do alcance de busca do probe "
                    f"({br(ALCANCE_BUSCA, 1)} mm). Ajuste as molas antes que algum ponto falhe."))

    # ---- area nao medida -----------------------------------------------
    if xs[-1] < MESA_X - 20 and m["tipo"] == "G29":
        borda = [an["resid"][j][-1] for j in range(an["ny"]) if an["resid"][j][-1] is not None]
        out.append(("info", "Faixa sem medição",
                    f"O probe só alcança até X {xs[-1]:.0f} e Y {ys[-1]:.0f}. Dali até a borda "
                    f"(direita e fundo) o firmware repete o valor da última linha de pontos. "
                    "Peças nessas faixas dependem da mesa estar plana ali."))

    # ---- temperatura e estado ------------------------------------------
    tm = m.get("temp_mesa")
    if tm is not None and tm < 40:
        out.append(("info", "Medição com a mesa fria",
                    f"A mesa estava a {tm:.0f} °C. Ela muda de forma ao aquecer: para a malha "
                    "que vai imprimir, meça com a mesa na temperatura de impressão."))
    if estado.get("compensacao") is False:
        out.append(("aviso", "Compensação desligada",
                    "O firmware disse 'Bed Leveling OFF'. O G-code do fatiador liga com "
                    "M420 S1; para mover à mão com a malha, use o botão Ligar."))
    if m["tipo"] == "G30":
        out.append(("info", "Medição rápida",
                    "Esta medição (G30) serve para ajustar as molas. A malha que vale para a "
                    "impressão é a do G29: depois do ajuste, meça com G29 e grave (M500)."))

    # ---- veredito e proximo passo --------------------------------------
    if comp and comp["fora"]:
        passo = ("aviso", "Próximo passo", "Repita a medição: há pontos que não repetiram.")
    elif an["suspeitos"] and not comp:
        passo = ("aviso", "Próximo passo",
                 "Repita a medição para confirmar os pontos suspeitos antes de girar as molas.")
    elif giro_max > max(tol, 0.10):
        passo = ("aviso", "Próximo passo",
                 "Gire as molas como indicam os cards dos cantos (comece pela metade se "
                 "a correção passar de 1 volta), faça Home e meça de novo.")
    elif m["tipo"] == "G30":
        passo = ("ok", "Próximo passo", "Molas no ponto. Agora meça a malha (G29) e grave com M500.")
    else:
        passo = ("ok", "Próximo passo",
                 "Mesa boa para imprimir. Se esta malha ainda não foi gravada, clique em Gravar (M500).")
    nota = ("excelente" if amp <= 0.15 else "boa" if amp <= 0.3
            else "aceitável com a compensação" if amp <= 0.5 else "desnivelada")
    out.insert(0, passo)
    out.insert(1, ("ok" if amp <= 0.3 else "info" if amp <= 0.5 else "aviso",
                   f"Mesa {nota}",
                   f"Amplitude de {br(amp)} mm (de {br(an['min'], 3, True)} a "
                   f"{br(an['max'], 3, True)})."))
    return out


def texto_para_ia(m, an, diag):
    """Resumo da medicao para mandar a um modelo local."""
    linhas = []
    for j in range(len(m["ys"]) - 1, -1, -1):
        linhas.append(f"  Y{m['ys'][j]:6.1f}: " + "  ".join(
            "  ---  " if v is None else f"{v:+.3f}" for v in m["z"][j]))
    linhas.append("          " + "  ".join(f"X{x:5.1f}" for x in m["xs"]))
    molas = "; ".join(f"{MOLAS_NOMES[k]}: {v['mm']:+.2f} mm ({v['voltas']:+.2f} voltas)"
                      for k, v in an["molas"].items())
    regras = "\n".join(f"- {t}: {x}" for _, t, x in diag)
    return (
        "Você é um técnico de impressoras 3D. Responda em português do Brasil, em no máximo "
        "8 linhas, de forma direta e prática.\n\n"
        "Impressora CoreXY, Marlin, nivelamento bilinear, mesa de vidro sobre 4 molas com "
        "parafusos M3. Valores em mm; positivo = mesa mais alta. A linha de cima é o fundo, "
        "a de baixo é a frente.\n\n"
        f"Malha medida ({m['tipo']}, {m['quando']}):\n" + "\n".join(linhas) + "\n\n"
        f"Correção calculada nas molas (positivo = subir): {molas}\n\n"
        f"Diagnóstico automático:\n{regras}\n\n"
        "Confirme ou corrija o diagnóstico e diga o que fazer agora, em passos."
    )


class LeitorSerial:
    """Reconhece malhas (M420 V / G29) e sondagens isoladas (G30) no texto da placa."""

    def __init__(self):
        self.cols = None
        self.linhas = {}
        self.ativo = False

    def alimenta(self, linha):
        """Devolve ('grade', z) ao fim de uma malha, ('g30', (x, y, z)), ou None."""
        m = RE_G30.search(linha)
        if m:
            return "g30", tuple(float(v) for v in m.groups())
        if RE_GRADE.search(linha):
            self.ativo, self.cols, self.linhas = True, None, {}
            return None
        if not self.ativo:
            return None
        if self.cols is None:
            if RE_CABECALHO.match(linha):
                self.cols = len(linha.split())
            return None
        m = RE_LINHA.match(linha)
        if m:
            toks = m.group(2).split()
            if len(toks) == self.cols:
                self.linhas[int(m.group(1))] = [None if t.startswith("=") else float(t)
                                                for t in toks]
                return None
        # fim da grade
        self.ativo = False
        if self.linhas and sorted(self.linhas) == list(range(len(self.linhas))):
            return "grade", [self.linhas[j] for j in range(len(self.linhas))]
        return None


# ---------------------------------------------------------------------------
# Persistencia
# ---------------------------------------------------------------------------

def cfg_padrao():
    return {
        "area": {"L": SONDA_X_MIN, "R": SONDA_X_MAX, "F": SONDA_Y_MIN, "B": SONDA_Y_MAX},
        "area_malha": {"L": SONDA_X_MIN, "R": SONDA_X_MAX, "F": SONDA_Y_MIN, "B": SONDA_Y_MAX},
        "modo": "G29", "grade_rapida": 3,
        "mm_volta": MM_POR_VOLTA_PADRAO, "sentido_sobe": "?",
        "referencia": "auto", "tolerancia": 0.05,
        "molas": {k: list(v) for k, v in MOLAS_PADRAO.items()},
        "cor_modo": "media", "temp_mesa": 55,
        "ia_url": "http://localhost:11434", "ia_modelo": "llama3.2",
    }


def carrega_dados():
    dados = {"cfg": cfg_padrao(), "historico": []}
    try:
        lido = json.loads(ARQ_DADOS.read_text(encoding="utf-8"))
        dados["cfg"].update(lido.get("cfg", {}))
        dados["historico"] = lido.get("historico", [])[-MAX_HISTORICO:]
    except (OSError, ValueError):
        pass
    return dados


def grava_dados(dados):
    try:
        ARQ_DADOS.write_text(json.dumps(dados, ensure_ascii=False, indent=1), encoding="utf-8")
    except OSError:
        pass


# ---------------------------------------------------------------------------
# Janela
# ---------------------------------------------------------------------------

class TelaNivelamento(tk.Toplevel):
    """Tela de nivelamento. 'app' e o console: usa send, confirm_send, link e temps."""

    def __init__(self, app):
        super().__init__(app)
        self.app = app
        self.title("Nivelamento da mesa — CoreXY TinyBee")
        self.configure(bg=C_FUNDO)
        sw, sh = self.winfo_screenwidth(), self.winfo_screenheight()
        self.geometry(f"{min(1500, sw - 40)}x{min(940, sh - 80)}+10+10")
        self.minsize(min(1100, sw - 40), min(700, sh - 80))
        if sw < 1600 or sh < 1000:
            try:
                self.state("zoomed")     # tela pequena: aproveita tudo
            except tk.TclError:
                pass

        self.dados = carrega_dados()
        self.cfg = self.dados["cfg"]
        self.hist = self.dados["historico"]
        self.atual = self.hist[-1] if self.hist else None
        self.analise = None
        self.estado = {"compensacao": None}
        self.leitor = LeitorSerial()
        self.pendente = None          # medicao em andamento
        self.ultima_grade = (None, 0.0)
        self.ia_fila = queue.Queue()
        self._tip = None

        self._monta()
        self._recalcula()

        app.line_hooks.append(self._linha)
        app.tx_hooks.append(self._enviado)
        self.protocol("WM_DELETE_WINDOW", self._fecha)
        self.after(500, self._tick)

    # ------------------------------------------------------------- estilo
    def _botao(self, pai, texto, cmd, cor=C_PAINEL2, fg=C_TEXTO, largura=None, negrito=False,
               tam=10, quebra=170):
        b = tk.Button(pai, text=texto, command=cmd, bg=cor, fg=fg,
                      activebackground=mistura(cor, "#ffffff", 0.15),
                      activeforeground=fg, relief="flat", bd=0, cursor="hand2",
                      font=(FONTE, tam, "bold" if negrito else "normal"),
                      padx=8, pady=7, highlightthickness=0, wraplength=quebra, justify="center")
        if largura:
            b.config(width=largura)
        b.bind("<Enter>", lambda e: b.config(bg=mistura(cor, "#ffffff", 0.12)))
        b.bind("<Leave>", lambda e: b.config(bg=cor))
        return b

    def _rotulo(self, pai, texto, tam=10, cor=C_TEXTO, negrito=False, bg=C_PAINEL, **kw):
        return tk.Label(pai, text=texto, bg=bg, fg=cor,
                        font=(FONTE, tam, "bold" if negrito else "normal"), **kw)

    def _paragrafo(self, pai, texto):
        l = self._rotulo(pai, texto, 9, C_SUAVE, justify="left", wraplength=360, anchor="w")
        l.pack(fill="x", padx=12, pady=(2, 4))
        return l

    def _spin(self, pai, var, de, ate, passo, larg=7, fmt="%.1f"):
        return tk.Spinbox(pai, from_=de, to=ate, increment=passo, textvariable=var, width=larg,
                          format=fmt, bg=C_PAINEL2, fg=C_TEXTO, buttonbackground=C_PAINEL2,
                          insertbackground=C_TEXTO, relief="flat", font=(FONTE, 10),
                          highlightthickness=1, highlightbackground=C_BORDA,
                          highlightcolor=C_ACENTO)

    # ------------------------------------------------------------- layout
    def _monta(self):
        topo = tk.Frame(self, bg=C_FUNDO)
        topo.pack(fill="x", padx=16, pady=(12, 6))
        self._rotulo(topo, "Nivelamento da mesa", 18, negrito=True, bg=C_FUNDO).pack(side="left")
        self.chips = {}
        for chave in ("conexao", "mesa", "comp", "medicao"):
            l = tk.Label(topo, text="", bg=C_PAINEL2, fg=C_TEXTO, font=(FONTE, 9, "bold"),
                         padx=10, pady=4)
            l.pack(side="left", padx=(10, 0))
            self.chips[chave] = l
        self._botao(topo, "PARADA (M112)", self.app.emergency_stop, cor="#9c2b2b",
                    negrito=True).pack(side="right")

        corpo = tk.Frame(self, bg=C_FUNDO)
        corpo.pack(fill="both", expand=True, padx=16, pady=(0, 14))
        corpo.columnconfigure(0, weight=1)
        corpo.columnconfigure(1, weight=0, minsize=420)
        corpo.rowconfigure(0, weight=1)

        esq = tk.Frame(corpo, bg=C_FUNDO)
        esq.grid(row=0, column=0, sticky="nsew", padx=(0, 12))
        dir_ = tk.Frame(corpo, bg=C_FUNDO, width=420)
        dir_.grid(row=0, column=1, sticky="nsew")

        # --- mapa
        quadro = tk.Frame(esq, bg=C_PAINEL, highlightthickness=1, highlightbackground=C_BORDA)
        quadro.pack(fill="both", expand=True)
        self.lbl_resumo = self._rotulo(quadro, "", 10, C_SUAVE, anchor="w")
        self.lbl_resumo.pack(fill="x", padx=12, pady=(8, 0))
        barra = tk.Frame(quadro, bg=C_PAINEL)
        barra.pack(fill="x", padx=10, pady=(4, 0))
        self._rotulo(barra, "Colorir por:", 9, C_SUAVE).pack(side="left", padx=(2, 6))
        self.var_cor = tk.StringVar(value=self.cfg.get("cor_modo", "media"))
        for val, txt in (("media", "Desvio da média"), ("plano", "O que sobra depois das molas"),
                         ("anterior", "Mudança desde a medição anterior")):
            tk.Radiobutton(barra, text=txt, value=val, variable=self.var_cor,
                           command=self._muda_cor, bg=C_PAINEL2, fg=C_TEXTO,
                           selectcolor="#2d5b8a", activebackground=C_PAINEL2,
                           activeforeground=C_TEXTO, font=(FONTE, 9),
                           indicatoron=False, padx=10, pady=3, relief="flat",
                           bd=0, highlightthickness=0).pack(side="left", padx=2)
        self.lbl_cor = self._rotulo(barra, "", 9, C_SUAVE)
        self.lbl_cor.pack(side="right", padx=4)

        self.cv = tk.Canvas(quadro, bg=C_PAINEL, highlightthickness=0)
        self.cv.pack(fill="both", expand=True, padx=4, pady=4)
        self.cv.bind("<Configure>", lambda e: self._desenha())

        # --- diagnostico
        dq = tk.Frame(esq, bg=C_PAINEL, highlightthickness=1, highlightbackground=C_BORDA)
        dq.pack(fill="x", pady=(10, 0))
        cab = tk.Frame(dq, bg=C_PAINEL)
        cab.pack(fill="x", padx=10, pady=(8, 2))
        self._rotulo(cab, "Diagnóstico (análise local, sem internet)", 11, negrito=True).pack(side="left")
        corpo_diag = tk.Frame(dq, bg=C_PAINEL)
        corpo_diag.pack(fill="x", padx=2, pady=(0, 8))
        self.txt_diag = tk.Text(corpo_diag, height=9, bg=C_PAINEL, fg=C_TEXTO, relief="flat",
                                wrap="word", font=(FONTE, 10), padx=10, pady=4,
                                highlightthickness=0, cursor="arrow", spacing1=2)
        sbd = tk.Scrollbar(corpo_diag, command=self.txt_diag.yview, width=12)
        self.txt_diag.config(yscrollcommand=sbd.set)
        sbd.pack(side="right", fill="y")
        self.txt_diag.pack(side="left", fill="x", expand=True)
        for nivel, cor in (("ok", C_OK), ("info", C_ACENTO), ("aviso", C_AVISO), ("erro", C_ERRO)):
            self.txt_diag.tag_config(nivel, foreground=cor, font=(FONTE, 10, "bold"))
        self.txt_diag.tag_config("corpo", foreground=C_TEXTO)

        # --- acoes
        acoes = tk.Frame(dir_, bg=C_PAINEL, highlightthickness=1, highlightbackground=C_BORDA)
        acoes.pack(fill="x")
        self._rotulo(acoes, "Ações", 11, negrito=True).pack(anchor="w", padx=10, pady=(8, 4))
        g = tk.Frame(acoes, bg=C_PAINEL)
        g.pack(fill="x", padx=8, pady=(0, 8))
        botoes = [
            ("⌂  Home (G28)", lambda: self.app.send("G28"), "#2d5b8a"),
            ("◎  Medir", self.medir, "#1f7a4d"),
            ("⟳  Ler malha", lambda: self.app.send("M420 V"), C_PAINEL2),
            ("✔  Ligar compensação", lambda: self.app.send("M420 S1"), C_PAINEL2),
            ("✕  Desligar compensação", lambda: self.app.send("M420 S0"), C_PAINEL2),
            ("💾  Gravar (M500)", self.gravar, "#6a4c93"),
            ("↺  Carregar da EEPROM", self.carregar_eeprom, C_PAINEL2),
            ("⇡  Subir Z 20 mm", self.subir_z, C_PAINEL2),
            ("⚙  Liberar motores", self.liberar_motores, C_PAINEL2),
            ("🔒  Motores sempre ligados", self.motores_fixos, C_PAINEL2),
            ("🔥  Aquecer mesa", self.aquecer_mesa, "#8a4b2d"),
            ("❄  Desligar mesa", lambda: self.app.send("M140 S0"), C_PAINEL2),
        ]
        for n, (t, c, cor) in enumerate(botoes):
            b = self._botao(g, t, c, cor=cor, negrito=t.startswith(("⌂", "◎", "💾")),
                            tam=9, quebra=112)
            b.grid(row=n // 3, column=n % 3, sticky="nsew", padx=3, pady=3)
        for i in range(3):
            g.columnconfigure(i, weight=1, uniform="ac")

        # --- abas (feitas a mao: o ttk.Notebook do Windows nao aceita tema escuro)
        barra_abas = tk.Frame(dir_, bg=C_FUNDO)
        barra_abas.pack(fill="x", pady=(10, 0))
        self.corpo_abas = tk.Frame(dir_, bg=C_PAINEL, highlightthickness=1,
                                   highlightbackground=C_BORDA)
        self.corpo_abas.pack(fill="both", expand=True)
        self.abas = {}
        for nome, fab in (("Medição", self._aba_medicao), ("Molas", self._aba_molas),
                          ("Histórico", self._aba_historico), ("IA local", self._aba_ia)):
            quadro = fab(self.corpo_abas)._ext
            b = tk.Button(barra_abas, text=nome, relief="flat", bd=0, cursor="hand2",
                          font=(FONTE, 10, "bold"), padx=16, pady=6, highlightthickness=0,
                          command=lambda n=nome: self._mostra_aba(n))
            b.pack(side="left", padx=(0, 2))
            self.abas[nome] = (b, quadro)
        self._mostra_aba("Medição")

    def _mostra_aba(self, nome):
        for n, (b, q) in self.abas.items():
            if n == nome:
                b.config(bg=C_PAINEL, fg=C_TEXTO, activebackground=C_PAINEL)
                q.pack(fill="both", expand=True)
            else:
                b.config(bg=C_FUNDO, fg=C_SUAVE, activebackground=C_PAINEL2)
                q.pack_forget()

    def _aba(self, pai):
        """Quadro com rolagem vertical. Devolve o interior; o quadro externo fica em _ext."""
        ext = tk.Frame(pai, bg=C_PAINEL)
        cv = tk.Canvas(ext, bg=C_PAINEL, highlightthickness=0, bd=0)
        sb = tk.Scrollbar(ext, orient="vertical", command=cv.yview, width=12)
        f = tk.Frame(cv, bg=C_PAINEL)
        f.bind("<Configure>", lambda e: cv.configure(scrollregion=cv.bbox("all")))
        win = cv.create_window((0, 0), window=f, anchor="nw")
        cv.bind("<Configure>", lambda e: cv.itemconfigure(win, width=e.width))
        cv.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        cv.pack(side="left", fill="both", expand=True)

        def roda(e):
            if cv.bbox("all") and cv.bbox("all")[3] > cv.winfo_height():
                cv.yview_scroll(int(-e.delta / 120), "units")
        f.bind("<Enter>", lambda e: cv.bind_all("<MouseWheel>", roda))
        f.bind("<Leave>", lambda e: cv.unbind_all("<MouseWheel>"))
        f._ext = ext
        return f

    def _aba_medicao(self, nb):
        f = self._aba(nb)
        p = dict(anchor="w", padx=12)
        self._rotulo(f, "Tipo de medição", 11, negrito=True).pack(pady=(10, 4), **p)
        self.var_modo = tk.StringVar(value=self.cfg.get("modo", "G29"))
        for val, txt in (("G29", "Malha G29 — 5×5, a que o firmware usa ao imprimir"),
                         ("G30", "Medição rápida G30 — só para ajustar as molas")):
            tk.Radiobutton(f, text=txt, value=val, variable=self.var_modo, command=self._salva_cfg,
                           bg=C_PAINEL, fg=C_TEXTO, selectcolor=C_PAINEL2, activebackground=C_PAINEL,
                           activeforeground=C_TEXTO, font=(FONTE, 10), anchor="w",
                           highlightthickness=0, wraplength=380, justify="left").pack(fill="x", **p)
        lin = tk.Frame(f, bg=C_PAINEL)
        lin.pack(fill="x", padx=34, pady=(2, 6))
        self._rotulo(lin, "Pontos da medição rápida:", 10, C_SUAVE).pack(side="left")
        self.var_grade = tk.IntVar(value=self.cfg.get("grade_rapida", 3))
        for n in (3, 4, 5):
            tk.Radiobutton(lin, text=f"{n}×{n}", value=n, variable=self.var_grade,
                           command=self._salva_cfg, indicatoron=False, bg=C_PAINEL2, fg=C_TEXTO,
                           selectcolor="#2d5b8a", activebackground=C_PAINEL2,
                           activeforeground=C_TEXTO, font=(FONTE, 10), padx=10, pady=2,
                           relief="flat", bd=0, highlightthickness=0).pack(side="left", padx=2)

        self._rotulo(f, "Área de sondagem (posição do probe, mm)", 11, negrito=True).pack(pady=(10, 4), **p)
        self.var_area = {}
        limites = {"L": (SONDA_X_MIN, SONDA_X_MAX, "Esquerda (L)"),
                   "R": (SONDA_X_MIN, SONDA_X_MAX, "Direita (R)"),
                   "F": (SONDA_Y_MIN, SONDA_Y_MAX, "Frente (F)"),
                   "B": (SONDA_Y_MIN, SONDA_Y_MAX, "Fundo (B)")}
        ga = tk.Frame(f, bg=C_PAINEL)
        ga.pack(fill="x", padx=12)
        for n, (k, (lo, hi, nome)) in enumerate(limites.items()):
            v = tk.DoubleVar(value=self.cfg["area"].get(k, lo if k in "LF" else hi))
            self.var_area[k] = v
            self._rotulo(ga, nome, 10, C_SUAVE).grid(row=n // 2, column=(n % 2) * 2, sticky="w",
                                                     pady=3, padx=(0, 6))
            s = self._spin(ga, v, lo, hi, 1.0)
            s.grid(row=n // 2, column=(n % 2) * 2 + 1, sticky="w", padx=(0, 18))
            s.bind("<FocusOut>", lambda e: self._salva_cfg())
        self._botao(f, "Restaurar área máxima", self._area_maxima).pack(anchor="w", padx=12, pady=6)
        self._paragrafo(f, (
            f"Limites do probe: X {SONDA_X_MIN:.0f}–{SONDA_X_MAX:.0f} e Y {SONDA_Y_MIN:.0f}–"
            f"{SONDA_Y_MAX:.0f}. O probe fica 54 mm à esquerda e 10 mm à frente do bico, por "
            "isso a faixa da direita e o fundo não são alcançáveis."))
        self._paragrafo(f, "• Área: muda sem regravar o firmware (G29 L R F B). O M500 grava a "
                           "área junto com a malha.")
        self._paragrafo(f, "• Número de pontos do G29: fixo em 5×5 na compilação "
                           "(GRID_MAX_POINTS_X). Mudar exige regravar o firmware.")
        self._paragrafo(f, "• A medição rápida G30 mede 3×3, 4×4 ou 5×5 sem regravar, mas não "
                           "vira malha de impressão: serve só para acertar as molas.")
        self.lbl_tempo = self._rotulo(f, "", 10, C_ACENTO)
        self.lbl_tempo.pack(anchor="w", padx=12)
        return f

    def _aba_molas(self, nb):
        f = self._aba(nb)
        p = dict(anchor="w", padx=12)
        self._rotulo(f, "Como converter altura em voltas", 11, negrito=True).pack(pady=(10, 4), **p)
        l1 = tk.Frame(f, bg=C_PAINEL)
        l1.pack(fill="x", padx=12)
        self._rotulo(l1, "Rendimento (mm por volta):", 10, C_SUAVE).pack(side="left")
        self.var_mmv = tk.DoubleVar(value=self.cfg.get("mm_volta", MM_POR_VOLTA_PADRAO))
        s = self._spin(l1, self.var_mmv, 0.05, 3.0, 0.05, fmt="%.2f")
        s.pack(side="left", padx=6)
        s.bind("<FocusOut>", lambda e: self._salva_cfg())
        self._botao(l1, "Padrão", lambda: (self.var_mmv.set(MM_POR_VOLTA_PADRAO),
                                           self._salva_cfg())).pack(side="left")
        self._paragrafo(f, (
            "Parafuso M3: a rosca anda 0,5 mm por volta. O que importa é quanto a mesa "
            "inclina, e isso foi medido em 25/10 com as malhas 3×3: equivale a "
            f"{br(MM_POR_VOLTA_PADRAO)} mm/volta com os parafusos nas posições abaixo."))

        self._rotulo(f, "Sentido que SOBE a mesa (olhando o botão que você gira)",
                     10, negrito=True).pack(**p)
        self.var_sentido = tk.StringVar(value=self.cfg.get("sentido_sobe", "?"))
        ls = tk.Frame(f, bg=C_PAINEL)
        ls.pack(fill="x", padx=12, pady=(2, 8))
        for val, txt in (("horario", "↻ horário"), ("anti", "↺ anti-horário"), ("?", "não sei")):
            tk.Radiobutton(ls, text=txt, value=val, variable=self.var_sentido,
                           command=self._salva_cfg, indicatoron=False, bg=C_PAINEL2, fg=C_TEXTO,
                           selectcolor="#2d5b8a", activebackground=C_PAINEL2,
                           activeforeground=C_TEXTO, font=(FONTE, 10), padx=10, pady=3,
                           relief="flat", bd=0, highlightthickness=0).pack(side="left", padx=2)

        self._rotulo(f, "Qual mola fica parada", 10, negrito=True).pack(**p)
        self.var_ref = tk.StringVar(value=self.cfg.get("referencia", "auto"))
        for val, txt in (("auto", "Automático — o menor giro total"),
                         ("baixo", "A do canto mais baixo — as outras só descem (molas mais firmes)"),
                         ("alto", "A do canto mais alto — as outras só sobem")):
            tk.Radiobutton(f, text=txt, value=val, variable=self.var_ref, command=self._salva_cfg,
                           bg=C_PAINEL, fg=C_TEXTO, selectcolor=C_PAINEL2, activebackground=C_PAINEL,
                           activeforeground=C_TEXTO, font=(FONTE, 9), anchor="w",
                           highlightthickness=0, wraplength=380, justify="left").pack(fill="x", padx=12)

        lt = tk.Frame(f, bg=C_PAINEL)
        lt.pack(fill="x", padx=12, pady=(8, 4))
        self._rotulo(lt, "Tolerância (mm), abaixo disso 'OK':", 10, C_SUAVE).pack(side="left")
        self.var_tol = tk.DoubleVar(value=self.cfg.get("tolerancia", 0.05))
        s = self._spin(lt, self.var_tol, 0.01, 0.5, 0.01, fmt="%.2f")
        s.pack(side="left", padx=6)
        s.bind("<FocusOut>", lambda e: self._salva_cfg())

        self._rotulo(f, "Posição dos parafusos (mm, coordenadas da mesa)", 10, negrito=True).pack(
            pady=(8, 2), **p)
        gp = tk.Frame(f, bg=C_PAINEL)
        gp.pack(fill="x", padx=12)
        self.var_molas = {}
        for n, k in enumerate(("TE", "TD", "FE", "FD")):
            x, y = self.cfg["molas"].get(k, MOLAS_PADRAO[k])
            vx, vy = tk.DoubleVar(value=x), tk.DoubleVar(value=y)
            self.var_molas[k] = (vx, vy)
            cel = tk.Frame(gp, bg=C_PAINEL)
            cel.grid(row=n // 2, column=n % 2, sticky="w", pady=2, padx=(0, 12))
            self._rotulo(cel, MOLAS_NOMES[k], 9, C_SUAVE, width=14, anchor="w").pack(side="left")
            for vv in (vx, vy):
                s = self._spin(cel, vv, -60, 260, 1.0, larg=5, fmt="%.0f")
                s.pack(side="left", padx=1)
                s.bind("<FocusOut>", lambda e: self._salva_cfg())

        self._rotulo(f, "Calibrar o rendimento", 11, negrito=True).pack(pady=(12, 2), **p)
        self._paragrafo(f, "Meça, gire as molas, meça de novo. Informe quantas voltas deu em cada "
                           "uma entre a medição mostrada e a anterior com os mesmos pontos "
                           "(positivo = subiu). Gire em PAR (as duas de um mesmo lado, a mesma "
                           "quantidade): um canto sozinho torce a mesa e dá número menos "
                           "confiável. Em 25/10 o par da direita deu 0,96 e o canto sozinho, 0,49.")
        gc = tk.Frame(f, bg=C_PAINEL)
        gc.pack(fill="x", padx=12, pady=4)
        self.var_giros = {}
        for n, k in enumerate(("TE", "TD", "FE", "FD")):
            v = tk.DoubleVar(value=0.0)
            self.var_giros[k] = v
            cel = tk.Frame(gc, bg=C_PAINEL)
            cel.grid(row=n // 2, column=n % 2, sticky="w", pady=2, padx=(0, 12))
            self._rotulo(cel, MOLAS_NOMES[k], 9, C_SUAVE, width=14, anchor="w").pack(side="left")
            self._spin(cel, v, -10, 10, 0.25, larg=6, fmt="%.2f").pack(side="left")
        self._botao(f, "Calcular rendimento", self._calibrar, cor="#2d5b8a").pack(
            anchor="w", padx=12, pady=6)
        return f

    def _aba_historico(self, nb):
        f = self._aba(nb)
        self._rotulo(f, "Medições guardadas (a última fica em cima)", 11, negrito=True).pack(
            anchor="w", padx=12, pady=(10, 4))
        fr = tk.Frame(f, bg=C_PAINEL)
        fr.pack(fill="both", expand=True, padx=12)
        self.lst = tk.Listbox(fr, bg=C_PAINEL2, fg=C_TEXTO, selectbackground="#2d5b8a",
                              relief="flat", highlightthickness=0, font=("Consolas", 10),
                              activestyle="none")
        sb = tk.Scrollbar(fr, command=self.lst.yview)
        self.lst.config(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        self.lst.pack(side="left", fill="both", expand=True)
        self.lst.bind("<<ListboxSelect>>", self._seleciona_hist)
        bl = tk.Frame(f, bg=C_PAINEL)
        bl.pack(fill="x", padx=10, pady=8)
        self._botao(bl, "Colar malha do log", self._colar).pack(side="left", padx=2)
        self._botao(bl, "Anotar", self._anotar).pack(side="left", padx=2)
        self._botao(bl, "Apagar", self._apagar_hist, cor="#6b2a2a").pack(side="left", padx=2)
        self._paragrafo(f, "'Colar malha do log' lê o texto copiado de um M420 V (do console ou de "
                           "um arquivo de log) e mostra no mapa. Clique numa medição para vê-la.")
        self._lista_hist()
        return f

    def _aba_ia(self, nb):
        f = self._aba(nb)
        self._rotulo(f, "IA local (opcional, grátis, funciona sem internet)", 11,
                     negrito=True).pack(anchor="w", padx=12, pady=(10, 4))
        self._paragrafo(f, (
            "O diagnóstico abaixo do mapa é calculado aqui mesmo e não precisa de nada. "
            "Se quiser uma segunda opinião em texto livre, instale o Ollama (ollama.com) uma "
            "vez, com internet, e baixe um modelo com o comando  ollama pull llama3.2 . "
            "Depois ele roda no próprio computador, sem internet e sem custo."))
        l1 = tk.Frame(f, bg=C_PAINEL)
        l1.pack(fill="x", padx=12, pady=8)
        self._rotulo(l1, "Modelo:", 10, C_SUAVE).pack(side="left")
        self.var_modelo = tk.StringVar(value=self.cfg.get("ia_modelo", "llama3.2"))
        e = tk.Entry(l1, textvariable=self.var_modelo, width=16, bg=C_PAINEL2, fg=C_TEXTO,
                     insertbackground=C_TEXTO, relief="flat", font=(FONTE, 10))
        e.pack(side="left", padx=6)
        e.bind("<FocusOut>", lambda ev: self._salva_cfg())
        self._botao(l1, "Perguntar à IA", self._pergunta_ia, cor="#2d5b8a").pack(side="left")
        self.lbl_ia = self._rotulo(f, "", 9, C_SUAVE)
        self.lbl_ia.pack(anchor="w", padx=12)
        self.txt_ia = tk.Text(f, bg=C_PAINEL2, fg=C_TEXTO, relief="flat", wrap="word",
                              font=(FONTE, 10), padx=8, pady=6, highlightthickness=0, height=12)
        self.txt_ia.pack(fill="both", expand=True, padx=12, pady=(4, 12))
        return f

    # ------------------------------------------------------------- dados
    def _salva_cfg(self):
        try:
            self.cfg["modo"] = self.var_modo.get()
            self.cfg["grade_rapida"] = int(self.var_grade.get())
            area = {k: float(v.get()) for k, v in self.var_area.items()}
            area["L"] = max(SONDA_X_MIN, min(area["L"], SONDA_X_MAX))
            area["R"] = max(SONDA_X_MIN, min(area["R"], SONDA_X_MAX))
            area["F"] = max(SONDA_Y_MIN, min(area["F"], SONDA_Y_MAX))
            area["B"] = max(SONDA_Y_MIN, min(area["B"], SONDA_Y_MAX))
            self.cfg["area"] = area
            self.cfg["mm_volta"] = max(0.01, float(self.var_mmv.get()))
            self.cfg["sentido_sobe"] = self.var_sentido.get()
            self.cfg["referencia"] = self.var_ref.get()
            self.cfg["tolerancia"] = float(self.var_tol.get())
            self.cfg["molas"] = {k: [float(vx.get()), float(vy.get())]
                                 for k, (vx, vy) in self.var_molas.items()}
            self.cfg["cor_modo"] = self.var_cor.get()
            self.cfg["ia_modelo"] = self.var_modelo.get().strip() or "llama3.2"
        except (tk.TclError, ValueError):
            return            # campo sendo digitado: espera o proximo evento
        grava_dados(self.dados)
        self._recalcula()

    def _area_maxima(self):
        for k, v in (("L", SONDA_X_MIN), ("R", SONDA_X_MAX), ("F", SONDA_Y_MIN), ("B", SONDA_Y_MAX)):
            self.var_area[k].set(v)
        self._salva_cfg()

    def _anterior_de(self, m):
        """Medicao anterior com os mesmos pontos, para comparar."""
        if m not in self.hist:
            cand = self.hist
        else:
            cand = self.hist[:self.hist.index(m)]
        for h in reversed(cand):
            if h is not m and mesmas_posicoes(h, m):
                return h
        return None

    def _registra(self, m):
        self.hist.append(m)
        del self.hist[:-MAX_HISTORICO]
        self.atual = m
        grava_dados(self.dados)
        self._lista_hist()
        self._recalcula()

    def _lista_hist(self):
        self.lst.delete(0, "end")
        for m in reversed(self.hist):
            vals = [v for linha in m["z"] for v in linha if v is not None]
            amp = max(vals) - min(vals) if vals else 0
            n = f"{len(m['xs'])}×{len(m['ys'])}"
            nota = f"  {m['nota']}" if m.get("nota") else ""
            self.lst.insert("end", f"{m['quando'][5:16]}  {m['tipo']} {n}  {br(amp)} mm{nota}")

    def _seleciona_hist(self, _):
        sel = self.lst.curselection()
        if sel:
            self.atual = self.hist[len(self.hist) - 1 - sel[0]]
            self._recalcula()

    def _apagar_hist(self):
        sel = self.lst.curselection()
        if not sel:
            return
        m = self.hist[len(self.hist) - 1 - sel[0]]
        if not messagebox.askyesno("Apagar", f"Apagar a medição de {m['quando']}?", parent=self):
            return
        self.hist.remove(m)
        if self.atual is m:
            self.atual = self.hist[-1] if self.hist else None
        grava_dados(self.dados)
        self._lista_hist()
        self._recalcula()

    def _anotar(self):
        sel = self.lst.curselection()
        if not sel:
            return
        m = self.hist[len(self.hist) - 1 - sel[0]]
        w = tk.Toplevel(self, bg=C_PAINEL)
        w.title("Anotar medição")
        w.transient(self)
        self._rotulo(w, f"Nota para {m['quando']}  (ex.: depois de 1 volta no fundo-esq.)",
                     10).pack(padx=12, pady=(12, 4))
        v = tk.StringVar(value=m.get("nota", ""))
        e = tk.Entry(w, textvariable=v, width=50, bg=C_PAINEL2, fg=C_TEXTO,
                     insertbackground=C_TEXTO, relief="flat", font=(FONTE, 10))
        e.pack(padx=12, pady=4)
        e.focus_set()

        def ok(_=None):
            m["nota"] = v.get().strip()[:60]
            grava_dados(self.dados)
            self._lista_hist()
            w.destroy()
        e.bind("<Return>", ok)
        self._botao(w, "Salvar", ok, cor="#2d5b8a").pack(pady=(4, 12))

    def _colar(self):
        try:
            texto = self.clipboard_get()
        except tk.TclError:
            texto = ""
        leitor = LeitorSerial()
        grade = None
        for linha in texto.splitlines() + [""]:
            # aceita linhas com o horario do console na frente
            linha = re.sub(r"^\s*\d\d:\d\d:\d\d\s+(\[\w+\]\s+)?", "", linha)
            r = leitor.alimenta(linha)
            if r and r[0] == "grade":
                grade = r[1]
        if not grade:
            messagebox.showinfo("Colar malha", "Não encontrei uma malha no texto copiado.\n"
                                "Copie desde a linha 'Bilinear Leveling Grid:' até a última "
                                "linha de números.", parent=self)
            return
        self._grade_recebida(grade, tipo="G29", nota="colada")

    # ------------------------------------------------------------- serial
    def _linha(self, linha):
        """Chamado pelo console para cada linha recebida da placa."""
        low = linha.lower()
        if "bed leveling on" in low:
            self.estado["compensacao"] = True
        elif "bed leveling off" in low:
            self.estado["compensacao"] = False
        if self.pendente and RE_FALHA.search(linha):
            if self.pendente["tipo"] == "G30" and "home" not in low:
                # um ponto falhou: fica sem leitura e a medicao segue com os outros
                self._g30_ponto(None)
            else:
                self.pendente = None
                self._status_medicao(f"Falhou: {linha.strip()[:60]}", C_ERRO)
            return
        r = self.leitor.alimenta(linha)
        if not r:
            return
        if r[0] == "grade":
            self._grade_recebida(r[1])
        elif r[0] == "g30" and self.pendente and self.pendente["tipo"] == "G30":
            self._g30_ponto(r[1][2])

    def _enviado(self, cmd):
        """Chamado pelo console para cada comando enviado: acompanha a area do G29."""
        c = cmd.strip().upper()
        if not re.match(r"^G29\b", c):
            return
        par = dict(re.findall(r"([LRFBH])\s*(-?[\d.]+)", c))
        if "H" in par:
            t = float(par["H"])
            x0 = max(MESA_X / 2 - t / 2, SONDA_X_MIN)
            y0 = max(MESA_Y / 2 - t / 2, SONDA_Y_MIN)
            area = {"L": x0, "F": y0, "R": min(x0 + t, SONDA_X_MAX), "B": min(y0 + t, SONDA_Y_MAX)}
        else:
            area = {"L": float(par.get("L", SONDA_X_MIN)), "R": float(par.get("R", SONDA_X_MAX)),
                    "F": float(par.get("F", SONDA_Y_MIN)), "B": float(par.get("B", SONDA_Y_MAX))}
        self.cfg["area_malha"] = area
        grava_dados(self.dados)
        if not self.pendente:
            self.pendente = {"tipo": "G29", "inicio": time.time()}

    def _grade_recebida(self, grade, tipo=None, nota=""):
        # o G29 imprime a grade e o M420 V em seguida imprime de novo: ignora repetida
        chave = json.dumps(grade)
        if not nota and self.ultima_grade[0] == chave and time.time() - self.ultima_grade[1] < 120:
            return
        self.ultima_grade = (chave, time.time())
        ny, nx = len(grade), len(grade[0])
        a = self.cfg.get("area_malha", self.cfg["area"])
        m = nova_medicao(tipo or "G29", linspace(a["L"], a["R"], nx), linspace(a["F"], a["B"], ny),
                         grade, self._temp_mesa(), nota)
        if self.atual and self.atual.get("z") == grade and not nota and \
                mesmas_posicoes(self.atual, m):
            # mesma malha que ja esta na tela (ex.: M420 V depois de reiniciar)
            self.pendente = None
            self._status_medicao("Malha lida da placa (igual à última guardada)", C_OK)
            return
        self.pendente = None
        self._registra(m)
        self._status_medicao(f"Malha recebida às {m['quando'][11:16]}", C_OK)

    def _g30_ponto(self, z):
        """Resultado (ou falha, z=None) do proximo ponto na ordem de envio."""
        p = self.pendente
        j, i = p["ordem"][p["idx"]]
        p["grade"][j][i] = z
        p["idx"] += 1
        self._status_medicao(f"Medindo (G30) {p['idx']}/{p['total']}...", C_ACENTO)
        if p["idx"] < p["total"]:
            return
        if all(v is None for linha in p["grade"] for v in linha):
            self.pendente = None
            self._status_medicao("Medição rápida falhou em todos os pontos", C_ERRO)
            return
        m = nova_medicao("G30", p["xs"], p["ys"], p["grade"], self._temp_mesa())
        self.pendente = None
        self._registra(m)
        self._status_medicao(f"Medição rápida concluída às {m['quando'][11:16]}", C_OK)
        self.app.send("M420 S1")   # o G30 desliga a compensacao (G30.cpp:69)

    def _temp_mesa(self):
        t = getattr(self.app, "temps", {})
        return round(t.get("b", 0.0), 1) if t.get("b") else None

    # ------------------------------------------------------------- acoes
    def _conectado(self):
        if not self.app.link.connected:
            messagebox.showwarning("Sem conexão", "Conecte a placa no console antes.", parent=self)
            return False
        return True

    def medir(self):
        if not self._conectado():
            return
        self._salva_cfg()
        a = self.cfg["area"]
        if a["R"] - a["L"] < 20 or a["B"] - a["F"] < 20:
            messagebox.showwarning("Área pequena", "A área precisa ter pelo menos 20 mm em X e em Y.",
                                   parent=self)
            return
        if self.cfg["modo"] == "G29":
            n, seg = GRADE_FIRMWARE ** 2, 10
            cmd = f"G29 L{a['L']:g} R{a['R']:g} F{a['F']:g} B{a['B']:g}"
            if not messagebox.askokcancel("Medir malha (G29)", (
                    f"Sondar {n} pontos (~{n * seg // 60} min) na área X {a['L']:g}–{a['R']:g}, "
                    f"Y {a['F']:g}–{a['B']:g}?\n\n"
                    "Faz Home antes (G28). Fique perto: no primeiro ponto, confira que o\n"
                    "probe não toca nas presilhas do vidro.\n\n"
                    "Depois, use 'Gravar (M500)' para guardar a malha."), parent=self):
                return
            self.app.send("G28")
            self.app.send(cmd)          # _enviado() registra a area e marca pendente
            self.app.send("M420 V")
            self.pendente = {"tipo": "G29", "inicio": time.time(), "total": n, "seg": seg}
        else:
            n = int(self.cfg["grade_rapida"])
            xs, ys = linspace(a["L"], a["R"], n), linspace(a["F"], a["B"], n)
            if not messagebox.askokcancel("Medição rápida (G30)", (
                    f"Sondar {n}×{n} pontos com G30 (~{n * n * 12 // 60 + 1} min)?\n\n"
                    "Faz Home antes (G28). Não altera a malha guardada: serve para\n"
                    "ajustar as molas. No fim a compensação é religada (M420 S1)."), parent=self):
                return
            # vai e volta em zigue-zague: menos deslocamento entre pontos
            ordem = [(j, i) for j in range(n)
                     for i in (range(n) if j % 2 == 0 else range(n - 1, -1, -1))]
            self.pendente = {"tipo": "G30", "inicio": time.time(), "total": n * n, "seg": 12,
                             "xs": xs, "ys": ys, "ordem": ordem, "idx": 0,
                             "grade": [[None] * n for _ in range(n)]}
            self.app.send("G28")
            for j, i in ordem:
                self.app.send(f"G30 X{xs[i]:.1f} Y{ys[j]:.1f}")
        self._status_medicao("Medindo...", C_ACENTO)

    def gravar(self):
        if not self._conectado():
            return
        if messagebox.askokcancel("Gravar (M500)", (
                "Gravar na EEPROM a malha que está na placa e os demais ajustes em vigor?\n\n"
                "Sem isso, a malha some ao reiniciar."), parent=self):
            self.app.send("M500")

    def carregar_eeprom(self):
        if self._conectado():
            self.app.send("M501")
            self.app.send("M420 V")

    def subir_z(self):
        if self._conectado():
            for c in ("G91", "G1 Z20 F600", "G90"):
                self.app.send(c)

    def liberar_motores(self):
        if self._conectado() and messagebox.askokcancel("Liberar motores", (
                "Desligar os motores (M84)?\n\nO carro fica solto e o firmware exige novo Home "
                "antes de medir (HOME_AFTER_DEACTIVATE)."), parent=self):
            self.app.send("M84")

    def motores_fixos(self):
        if self._conectado():
            self.app.send("M84 S0")
            self.app.log("Motores ficam ligados até reiniciar a placa (M84 S0): o Home não "
                         "se perde enquanto você gira as molas.", "sys")

    def aquecer_mesa(self):
        if not self._conectado():
            return
        w = tk.Toplevel(self, bg=C_PAINEL)
        w.title("Aquecer mesa")
        w.transient(self)
        self._rotulo(w, "Temperatura da mesa (°C)", 10).pack(padx=16, pady=(14, 4))
        v = tk.IntVar(value=int(self.cfg.get("temp_mesa", 55)))
        self._spin(w, v, 30, 100, 5, fmt="%.0f").pack(padx=16)

        def ok():
            self.cfg["temp_mesa"] = int(v.get())
            grava_dados(self.dados)
            self.app.send(f"M140 S{int(v.get())}")
            w.destroy()
        self._botao(w, "Aquecer", ok, cor="#8a4b2d").pack(pady=12)

    def _calibrar(self):
        if not self.atual:
            return
        ant = self._anterior_de(self.atual)
        if not ant:
            messagebox.showinfo("Calibrar", "Preciso de duas medições com os mesmos pontos.",
                                parent=self)
            return
        giros = {k: float(v.get()) for k, v in self.var_giros.items()}
        try:
            p = calibra_rendimento(ant, self.atual, giros, self.cfg["molas"])
        except ValueError as exc:
            messagebox.showwarning("Calibrar", str(exc), parent=self)
            return
        if messagebox.askyesno("Calibrar", (
                f"Entre {ant['quando']} e {self.atual['quando']}, o rendimento medido foi "
                f"{br(p)} mm por volta (atual: {br(self.cfg['mm_volta'])}).\n\nUsar este valor?"),
                parent=self):
            self.var_mmv.set(round(p, 2))
            self._salva_cfg()

    def _pergunta_ia(self):
        if not self.atual or not self.analise:
            return
        self._salva_cfg()
        diag = diagnostico(self.atual, self.analise, self.cfg, self.estado)
        prompt = texto_para_ia(self.atual, self.analise, diag)
        url = self.cfg.get("ia_url", "http://localhost:11434").rstrip("/") + "/api/generate"
        modelo = self.cfg["ia_modelo"]
        self.lbl_ia.config(text=f"Perguntando ao {modelo}... (pode levar até 1 minuto)", fg=C_ACENTO)
        self.txt_ia.delete("1.0", "end")

        def trabalho():
            try:
                req = urllib.request.Request(
                    url, data=json.dumps({"model": modelo, "prompt": prompt, "stream": False}).encode(),
                    headers={"Content-Type": "application/json"})
                with urllib.request.urlopen(req, timeout=180) as r:
                    resp = json.loads(r.read().decode("utf-8")).get("response", "").strip()
                self.ia_fila.put(("ok", resp or "(resposta vazia)"))
            except urllib.error.HTTPError as exc:
                self.ia_fila.put(("erro", f"O Ollama respondeu erro {exc.code}. O modelo "
                                          f"'{modelo}' foi baixado? (ollama pull {modelo})"))
            except (urllib.error.URLError, OSError, ValueError):
                self.ia_fila.put(("erro", "Ollama não encontrado em localhost:11434. "
                                          "Instale e abra o Ollama, ou use só o diagnóstico local."))
        threading.Thread(target=trabalho, daemon=True).start()

    # ------------------------------------------------------------- ciclo
    def _status_medicao(self, texto, cor):
        self.chips["medicao"].config(text=texto, fg=cor)

    def _tick(self):
        if not self.winfo_exists():
            return
        con = self.app.link.connected
        self.chips["conexao"].config(text="● conectado" if con else "● desconectado",
                                     fg=C_OK if con else C_ERRO)
        t = getattr(self.app, "temps", {})
        b, bt = t.get("b", 0.0), t.get("bt", 0.0)
        self.chips["mesa"].config(text=f"Mesa {b:.0f} / {bt:.0f} °C",
                                  fg=C_AVISO if bt > 0 else C_TEXTO)
        c = self.estado["compensacao"]
        self.chips["comp"].config(text={True: "Compensação ON", False: "Compensação OFF",
                                        None: "Compensação ?"}[c],
                                  fg={True: C_OK, False: C_AVISO, None: C_SUAVE}[c])
        p = self.pendente
        if p and p.get("total"):
            dec = time.time() - p["inicio"]
            falta = max(0, p["total"] * p.get("seg", 10) - dec)
            if dec > p["total"] * p.get("seg", 10) * 2.5 + 60:
                self.pendente = None
                self._status_medicao("Sem resultado da placa: medição abandonada", C_ERRO)
            elif p["tipo"] == "G29":
                self._status_medicao(f"Medindo (G29)... {int(dec)} s  ~{int(falta)} s restantes",
                                     C_ACENTO)
        elif not self.chips["medicao"].cget("text"):
            self._status_medicao("Pronto", C_SUAVE)
        try:
            while True:
                tipo, txt = self.ia_fila.get_nowait()
                self.lbl_ia.config(text="Resposta da IA local:" if tipo == "ok" else "Não deu certo:",
                                   fg=C_OK if tipo == "ok" else C_ERRO)
                self.txt_ia.delete("1.0", "end")
                self.txt_ia.insert("1.0", txt)
        except queue.Empty:
            pass
        self.after(500, self._tick)

    def _fecha(self):
        self._salva_cfg()
        for lst, f in ((self.app.line_hooks, self._linha), (self.app.tx_hooks, self._enviado)):
            if f in lst:
                lst.remove(f)
        self.app.tela_nivel = None
        self.destroy()

    # ------------------------------------------------------------- analise
    def _muda_cor(self):
        self.cfg["cor_modo"] = self.var_cor.get()
        grava_dados(self.dados)
        self._desenha()

    def _recalcula(self):
        if self.atual:
            self.analise = analisa(self.atual, self.cfg, self._anterior_de(self.atual))
        else:
            self.analise = None
        self._escreve_diag()
        self._desenha()
        a = self.cfg["area"]
        if self.cfg.get("modo") == "G29":
            t = f"G29 L{a['L']:g} R{a['R']:g} F{a['F']:g} B{a['B']:g}  —  25 pontos, ~4 min"
        else:
            n = self.cfg.get("grade_rapida", 3)
            t = f"{n * n} × G30 na área X {a['L']:g}–{a['R']:g}, Y {a['F']:g}–{a['B']:g}  —  ~{n * n * 12 // 60 + 1} min"
        if hasattr(self, "lbl_tempo"):
            self.lbl_tempo.config(text="Ao clicar em Medir: " + t)

    def _escreve_diag(self):
        t = self.txt_diag
        t.config(state="normal")
        t.delete("1.0", "end")
        if not self.atual:
            t.insert("end", "Sem medição. Faça Home e clique em Medir, ou em 'Ler malha (M420 V)' "
                            "para ver a malha gravada na placa.", "corpo")
        else:
            icones = {"ok": "✔", "info": "●", "aviso": "▲", "erro": "✖"}
            for nivel, titulo, texto in diagnostico(self.atual, self.analise, self.cfg, self.estado):
                t.insert("end", f"{icones[nivel]} {titulo}: ", nivel)
                t.insert("end", texto + "\n", "corpo")
        t.config(state="disabled")
        an = self.analise
        if an:
            m = self.atual
            self.lbl_resumo.config(text=(
                f"{m['tipo']} {an['nx']}×{an['ny']}  ·  {m['quando'][:16]}  ·  amplitude "
                f"{br(an['amplitude'])} mm  ·  inclinação X {br(abs(an['incl_x']))} / "
                f"Y {br(abs(an['incl_y']))} mm  ·  ondulação {br(an['resid_amp'])} mm"))
        else:
            self.lbl_resumo.config(text="")

    # ------------------------------------------------------------- desenho
    def _valores_cor(self):
        """Grade de desvios usada para colorir, conforme o modo escolhido."""
        m, an = self.atual, self.analise
        modo = self.var_cor.get()
        if modo == "anterior":
            if an["comp"]:
                return an["comp"]["rel"], "mudança em relação a " + an["comp"]["quando"][5:16]
            modo = "media"
        if modo == "plano":
            return an["resid"], "o que sobra depois de acertar as molas"
        return [[None if v is None else v - an["media"] for v in linha] for linha in m["z"]], \
            "desvio em relação à média da mesa"

    def _desenha(self):
        cv = self.cv
        cv.delete("all")
        W, H = cv.winfo_width(), cv.winfo_height()
        if W < 200 or H < 200:
            return
        pad_x, pad_t, pad_b = 170, 40, 92
        s = min((W - 2 * pad_x) / MESA_X, (H - pad_t - pad_b) / (MESA_Y - Y_HOME))
        ox = (W - MESA_X * s) / 2
        oy = pad_t + (MESA_Y - Y_HOME) * s           # y da tela para Y = Y_HOME

        def T(x, y):
            return ox + x * s, oy - (y - Y_HOME) * s

        # area total do curso e presilhas
        x0, y0 = T(0, Y_HOME)
        x1, y1 = T(MESA_X, 0)
        cv.create_rectangle(x0, y0, x1, y1, fill="#23282e", outline="")
        for k in range(int(x0) - 40, int(x1), 14):
            cv.create_line(max(k, x0), y0 - max(0, x0 - k), min(k + (y0 - y1), x1),
                           y0 - min(y0 - y1, x1 - k), fill="#30363d")
        cv.create_text((x0 + x1) / 2, (y0 + y1) / 2, text="presilhas do vidro (Y −15 a 0)",
                       fill=C_SUAVE, font=(FONTE, 9))

        # vidro
        gx0, gy0 = T(0, MESA_Y)
        gx1, gy1 = T(MESA_X, 0)
        self._ret_arred(gx0, gy0, gx1, gy1, 10, fill=C_VIDRO, outline="#4b5662", width=2)

        # alcance do probe
        px0, py0 = T(SONDA_X_MIN, SONDA_Y_MAX)
        px1, py1 = T(SONDA_X_MAX, SONDA_Y_MIN)
        cv.create_rectangle(px0, py0, px1, py1, outline="#56616d", dash=(4, 4))
        rx, _ = T(SONDA_X_MAX, 0)
        if self.atual and len(self.atual["xs"]) > 1:
            meio_card = min((self.atual["xs"][1] - self.atual["xs"][0]) * s * 0.72, 92) / 2
        else:
            meio_card = 0
        cv.create_text((rx + meio_card + gx1) / 2, (gy0 + gy1) / 2, angle=90,
                       text="fora do alcance do probe  ·  o firmware repete a borda",
                       fill="#6c7782", font=(FONTE, 9))

        # rotulos de orientacao
        cv.create_text((gx0 + gx1) / 2, gy0 - 18, text="FUNDO", fill=C_SUAVE, font=(FONTE, 10, "bold"))
        cv.create_text((x0 + x1) / 2, y0 + 16, text="FRENTE  (você está aqui)", fill=C_SUAVE,
                       font=(FONTE, 10, "bold"))
        cv.create_text(gx0 - 14, (gy0 + gy1) / 2, text="ESQUERDA", angle=90, fill=C_SUAVE,
                       font=(FONTE, 10, "bold"))
        cv.create_text(gx1 + 14, (gy0 + gy1) / 2, text="DIREITA", angle=270, fill=C_SUAVE,
                       font=(FONTE, 10, "bold"))

        self._legenda(W, H)
        if not self.atual:
            cv.create_text((gx0 + gx1) / 2, (gy0 + gy1) / 2, fill=C_TEXTO, font=(FONTE, 13),
                           text="Sem medição\n\nFaça Home e clique em Medir,\nou em 'Ler malha (M420 V)'",
                           justify="center")
            self._molas(T, s, None)
            return

        m, an = self.atual, self.analise
        grade, legenda_modo = self._valores_cor()
        xs, ys = m["xs"], m["ys"]
        nx, ny = len(xs), len(ys)

        # mapa de fundo interpolado
        if nx > 1 and ny > 1:
            passos = 40
            for a_ in range(passos):
                for b_ in range(passos):
                    u0, u1 = a_ / passos, (a_ + 1) / passos
                    v0, v1 = b_ / passos, (b_ + 1) / passos
                    val = self._interp(grade, (u0 + u1) / 2 * (nx - 1), (v0 + v1) / 2 * (ny - 1))
                    if val is None:
                        continue
                    xa, ya = T(xs[0] + (xs[-1] - xs[0]) * u0, ys[0] + (ys[-1] - ys[0]) * v1)
                    xb, yb = T(xs[0] + (xs[-1] - xs[0]) * u1, ys[0] + (ys[-1] - ys[0]) * v0)
                    cv.create_rectangle(xa, ya, xb + 1, yb + 1, outline="",
                                        fill=mistura(cor_continua(val), C_VIDRO, 0.55))
        self.lbl_cor.config(text="cor = " + legenda_modo)

        # cards dos pontos
        dx = (xs[1] - xs[0]) * s if nx > 1 else 80
        dy = (ys[1] - ys[0]) * s if ny > 1 else 60
        cw = max(46, min(dx * 0.72, 92))
        ch = max(32, min(dy * 0.58, 58))
        fz = 11 if cw >= 64 else 9
        for j, y in enumerate(ys):
            for i, x in enumerate(xs):
                cx, cy = T(x, y)
                v = m["z"][j][i]
                d = grade[j][i]
                tag = f"pt_{j}_{i}"
                if v is None:
                    self._ret_arred(cx - cw / 2, cy - ch / 2, cx + cw / 2, cy + ch / 2, 8,
                                    fill="#444", outline="#666", tags=(tag, "card"))
                    cv.create_text(cx, cy, text="sem\nleitura", fill=C_TEXTO, font=(FONTE, 8),
                                   tags=(tag, "card"), justify="center")
                    continue
                _, cor, _ = faixa(d if d is not None else 0)
                escuro = cor in (FAIXAS[2][1], FAIXAS[1][1])
                fg = "#15181b" if escuro else "#ffffff"
                suspeito = (j, i) in an["suspeitos"] or \
                    (an["comp"] and (j, i) in an["comp"]["fora"])
                self._ret_arred(cx - cw / 2 + 2, cy - ch / 2 + 3, cx + cw / 2 + 2, cy + ch / 2 + 3, 9,
                                fill="#0e1013", outline="", tags=(tag, "card"))
                self._ret_arred(cx - cw / 2, cy - ch / 2, cx + cw / 2, cy + ch / 2, 9, fill=cor,
                                outline="#ffffff" if suspeito else mistura(cor, "#000000", 0.3),
                                width=3 if suspeito else 1, tags=(tag, "card"))
                cv.create_text(cx, cy - ch * 0.13, text=br(v, 3, True), fill=fg,
                               font=(FONTE, fz, "bold"), tags=(tag, "card"))
                seta = "▲ alto" if (d or 0) > 0.025 else ("▼ baixo" if (d or 0) < -0.025 else "● nível")
                if suspeito:
                    seta = "⚠ suspeito"
                if ch >= 38:
                    cv.create_text(cx, cy + ch * 0.25, text=seta, fill=fg, font=(FONTE, 8), tags=(tag, "card"))
                cv.tag_bind(tag, "<Enter>", lambda e, jj=j, ii=i: self._dica(e, jj, ii))
                cv.tag_bind(tag, "<Leave>", lambda e: self._sem_dica())

        self._molas(T, s, an)
        cv.tag_raise("card")

    def _interp(self, g, fx, fy):
        nx, ny = len(g[0]), len(g)
        i0, j0 = min(int(fx), nx - 2), min(int(fy), ny - 2)
        tx, ty = fx - i0, fy - j0
        cantos = [g[j0][i0], g[j0][i0 + 1], g[j0 + 1][i0], g[j0 + 1][i0 + 1]]
        if any(c is None for c in cantos):
            return None
        a, b, c, d = cantos
        return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty

    def _molas(self, T, s, an):
        cv = self.cv
        W = cv.winfo_width()
        tol = self.cfg.get("tolerancia", 0.05)
        sentido = self.cfg.get("sentido_sobe", "?")
        gx0, gy0 = T(0, MESA_Y)
        gx1, gy1 = T(MESA_X, Y_HOME)
        cw, ch = 150, 112
        for k, (mx, my) in self.cfg["molas"].items():
            px, py = T(mx, my)
            esquerda = k.endswith("E")
            frente = k.startswith("F")
            cx = (gx0 - 20 - cw / 2 - 6) if esquerda else (gx1 + 20 + cw / 2 + 6)
            cx = max(cw / 2 + 4, min(W - cw / 2 - 4, cx))
            cy = (gy1 - ch / 2) if frente else (gy0 + ch / 2)
            # parafuso
            cv.create_line(px, py, cx + (cw / 2 if esquerda else -cw / 2), cy, fill="#56616d",
                           dash=(3, 3))
            cv.create_oval(px - 9, py - 9, px + 9, py + 9, fill="#596673", outline="#c0c8d0", width=2)
            cv.create_line(px - 5, py, px + 5, py, fill="#1a1d21", width=2)

            if an is None:
                cor, titulo, linha2, linha3 = C_PAINEL2, "—", "sem medição", ""
            else:
                c = an["molas"][k]
                mm, v = c["mm"], c["voltas"]
                if c["referencia"]:
                    cor, titulo, linha2, linha3 = "#2d5b8a", "REFERÊNCIA", "não mexer", "as outras se acertam por esta"
                elif abs(mm) <= tol:
                    cor, titulo, linha2, linha3 = C_OK, "OK", "não mexer", f"{br(mm, 2, True)} mm"
                else:
                    sobe = mm > 0
                    cor = "#c26a16" if abs(mm) > 0.3 else "#a8861c"
                    titulo = ("SUBIR " if sobe else "DESCER ") + texto_voltas(v)
                    if sentido == "?":
                        linha2 = "sentido: configure na aba Molas"
                    else:
                        horario = (sentido == "horario") == sobe
                        linha2 = "↻ sentido horário" if horario else "↺ sentido anti-horário"
                    linha3 = f"{br(mm, 2, True)} mm no canto"
            self._ret_arred(cx - cw / 2 + 2, cy - ch / 2 + 3, cx + cw / 2 + 2, cy + ch / 2 + 3, 12,
                            fill="#0e1013", outline="")
            self._ret_arred(cx - cw / 2, cy - ch / 2, cx + cw / 2, cy + ch / 2, 12, fill=C_PAINEL2,
                            outline=cor, width=2)
            self._ret_arred(cx - cw / 2, cy - ch / 2, cx + cw / 2, cy - ch / 2 + 26, 12, fill=cor,
                            outline=cor)
            cv.create_rectangle(cx - cw / 2 + 1, cy - ch / 2 + 14, cx + cw / 2 - 1,
                                cy - ch / 2 + 26, fill=cor, outline="")
            cv.create_text(cx, cy - ch / 2 + 13, text="Mola · " + MOLAS_NOMES[k].lower(),
                           fill="#ffffff", font=(FONTE, 9, "bold"))
            cv.create_text(cx, cy - 6, text=titulo, fill=C_TEXTO, font=(FONTE, 12, "bold"),
                           width=cw - 12, justify="center")
            cv.create_text(cx, cy + 20, text=linha2, fill=C_SUAVE, font=(FONTE, 9),
                           width=cw - 12, justify="center")
            cv.create_text(cx, cy + 38, text=linha3, fill=C_SUAVE, font=(FONTE, 8),
                           width=cw - 12, justify="center")

    def _legenda(self, W, H):
        cv = self.cv
        titulo = {"anterior": "Mudança desde a medição anterior (mm)",
                  "plano": "O que sobra depois das molas (mm)"}.get(self.var_cor.get(),
                                                                     "Desvio da média (mm)")
        n = len(FAIXAS)
        lw = min(130, (W - 80) / n)
        x = (W - lw * n) / 2
        y = H - 34
        cv.create_text(W / 2, y - 16, text=titulo, fill=C_SUAVE, font=(FONTE, 9))
        for i, (_, cor, nome, faixa_txt) in enumerate(FAIXAS):
            self._ret_arred(x + i * lw + 3, y - 4, x + (i + 1) * lw - 3, y + 26, 7, fill=cor, outline="")
            fg = "#15181b" if i in (1, 2) else "#ffffff"
            cv.create_text(x + (i + 0.5) * lw, y + 5, text=nome, fill=fg, font=(FONTE, 9, "bold"))
            cv.create_text(x + (i + 0.5) * lw, y + 18, text=faixa_txt, fill=fg, font=(FONTE, 8))

    def _ret_arred(self, x0, y0, x1, y1, r, **kw):
        r = min(r, (x1 - x0) / 2, (y1 - y0) / 2)
        pts = [x0 + r, y0, x1 - r, y0, x1, y0, x1, y0 + r, x1, y1 - r, x1, y1,
               x1 - r, y1, x0 + r, y1, x0, y1, x0, y1 - r, x0, y0 + r, x0, y0]
        return self.cv.create_polygon(pts, smooth=True, **kw)

    def _dica(self, e, j, i):
        self._sem_dica()
        m, an = self.atual, self.analise
        v = m["z"][j][i]
        if v is None:
            return
        linhas = [f"Ponto X {m['xs'][i]:.1f}  Y {m['ys'][j]:.1f}  (linha {j}, coluna {i})",
                  f"Medido: {br(v, 3, True)} mm",
                  f"Desvio da média: {br(v - an['media'], 3, True)} mm",
                  f"Depois das molas: {br(an['resid'][j][i], 3, True)} mm"]
        if an["comp"] and an["comp"]["rel"][j][i] is not None:
            linhas.append(f"Mudou desde a anterior: {br(an['comp']['rel'][j][i], 3, True)} mm "
                          "(fora o deslocamento geral)")
        if (j, i) in an["suspeitos"]:
            linhas.append("⚠ " + an["suspeitos"][(j, i)])
        x, y = e.x + 16, e.y + 12
        t = self.cv.create_text(x + 8, y + 6, text="\n".join(linhas), anchor="nw", fill=C_TEXTO,
                                font=(FONTE, 9), tags="dica")
        bx0, by0, bx1, by1 = self.cv.bbox(t)
        W, H = self.cv.winfo_width(), self.cv.winfo_height()
        ddx = min(0, W - 6 - bx1 - 8) if bx1 + 8 > W else 0
        ddy = min(0, H - 6 - by1 - 8) if by1 + 8 > H else 0
        if ddx or ddy:
            self.cv.move(t, ddx if ddx else 0, ddy if ddy else 0)
            bx0, by0, bx1, by1 = self.cv.bbox(t)
        r = self.cv.create_rectangle(bx0 - 8, by0 - 6, bx1 + 8, by1 + 6, fill="#0e1013",
                                     outline=C_BORDA, tags="dica")
        self.cv.tag_raise(t, r)

    def _sem_dica(self):
        self.cv.delete("dica")


# ---------------------------------------------------------------------------
# Execucao avulsa: abre a tela com uma placa simulada (sem porta serial)
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    class _LinkFalso:
        connected = False

    class _AppFalso(tk.Tk):
        def __init__(self):
            super().__init__()
            self.withdraw()
            self.link = _LinkFalso()
            self.temps = {"b": 0.0, "bt": 0.0}
            self.line_hooks, self.tx_hooks = [], []

        def send(self, cmd):
            print(">>", cmd)

        def log(self, txt, tag=""):
            print(txt)

        def emergency_stop(self):
            print(">> M112")

    app = _AppFalso()
    tela = TelaNivelamento(app)
    tela.protocol("WM_DELETE_WINDOW", app.destroy)
    app.mainloop()
