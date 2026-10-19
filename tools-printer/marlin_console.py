#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Console de testes Marlin — MKS TinyBee / CoreXY
================================================
Ferramenta de bancada para a fase inicial de montagem: localizar a placa,
monitorar sensores e temperaturas, testar direção dos motores, calibrar
passos/mm, acionar o probe por servo e enviar G-code direto.

Feito sob medida para esta máquina:
  - Cinemática CoreXY
  - Z duplo (Z = motor direito, Z2 = motor esquerdo no driver E1)
  - Fins de curso NF, MIN e MAX em série, homing no mínimo
  - Probe por servo: 45° = ativo, 0° = recolhido
  - Sem cartão SD; EEPROM habilitada

Requisitos: Python 3 com tkinter (padrão) e pyserial.
    pip install pyserial

Uso:  python marlin_console.py
"""

import io
import os
import queue
import re
import subprocess
import sys
import threading
import time
from collections import deque
from pathlib import Path
import tkinter as tk
from tkinter import messagebox, ttk

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    raise SystemExit("pyserial nao encontrado. Instale com:  pip install pyserial")

# ---------------------------------------------------------------------------
# Configuração
# ---------------------------------------------------------------------------

BAUDRATES = [250000, 115200, 500000, 57600]
DETECT_TIMEOUT = 3.0          # (s) espera por resposta do M115 ao detectar
TEMP_REPORT_INTERVAL = 2      # (s) M155 S<n> — auto-report de temperatura
ENDSTOP_POLL_INTERVAL = 3.0   # (s) M119 periódico quando o monitor está ligado
OK_TIMEOUT = 45.0             # (s) sem resposta da placa, destrava a fila de envio

LOG_DIR = Path(__file__).resolve().parent / "logs"

# Gravacao de firmware pelo PlatformIO
PROJECT_DIR = Path(__file__).resolve().parent.parent
PIO_ENV = "mks_tinybee"
PIO_EXE = Path.home() / ".platformio" / "penv" / "Scripts" / (
    "pio.exe" if sys.platform == "win32" else "pio")

SERVO_DEPLOY_ANGLE = 35       # Z_SERVO_ANGLES { 35, 0 }
SERVO_STOW_ANGLE = 0

# Cores
BG_OK = "#1f7a3f"
BG_WARN = "#b07d00"
BG_ERR = "#9c2b2b"
BG_IDLE = "#555555"

# Regex de parsing
RE_TEMP = re.compile(
    r"T:\s*(-?[\d.]+)\s*/\s*(-?[\d.]+)"      # hotend atual/alvo
    r"(?:.*?B:\s*(-?[\d.]+)\s*/\s*(-?[\d.]+))?"  # mesa atual/alvo (opcional)
)
RE_POS = re.compile(r"X:\s*(-?[\d.]+)\s*Y:\s*(-?[\d.]+)\s*Z:\s*(-?[\d.]+)\s*E:\s*(-?[\d.]+)")
RE_ENDSTOP = re.compile(r"^\s*([a-z_0-9]+):\s*(open|TRIGGERED)\s*$", re.IGNORECASE)
RE_STEPS = re.compile(r"M92\s+X([\d.]+)\s+Y([\d.]+)\s+Z([\d.]+)\s+E([\d.]+)")


# ---------------------------------------------------------------------------
# Camada serial
# ---------------------------------------------------------------------------

class SerialLink:
    """Conexão serial com leitura em thread separada."""

    def __init__(self, on_line):
        self.ser = None
        self.on_line = on_line
        self._stop = threading.Event()
        self._thread = None
        self.port = None
        self.baud = None

    # -- detecção ----------------------------------------------------------
    @staticmethod
    def candidate_ports():
        """Portas seriais, com as de conversor USB conhecido primeiro."""
        ports = list(list_ports.comports())
        known = ("CH340", "CH910", "CP210", "FTDI", "FT232", "USB-SERIAL",
                 "Silicon Labs", "USB Serial")

        def score(p):
            desc = f"{p.description} {p.manufacturer or ''}".upper()
            return 0 if any(k.upper() in desc for k in known) else 1

        return sorted(ports, key=score)

    @classmethod
    def autodetect(cls, log=None):
        """Procura uma placa rodando Marlin. Devolve (porta, baud, banner) ou None."""
        for p in cls.candidate_ports():
            for baud in BAUDRATES:
                if log:
                    log(f"  testando {p.device} @ {baud}...")
                try:
                    with serial.Serial(p.device, baud, timeout=0.5) as s:
                        time.sleep(2.0)          # ESP32 reinicia ao abrir a porta
                        s.reset_input_buffer()
                        s.write(b"\nM115\n")
                        deadline = time.time() + DETECT_TIMEOUT
                        buf = ""
                        while time.time() < deadline:
                            buf += s.read(256).decode("ascii", "ignore")
                            if "FIRMWARE_NAME" in buf:
                                banner = next(
                                    (ln for ln in buf.splitlines()
                                     if "FIRMWARE_NAME" in ln), buf[:200])
                                return p.device, baud, banner.strip()
                except (OSError, serial.SerialException):
                    break   # porta ocupada ou inexistente: não insiste em outros bauds
        return None

    # -- conexão -----------------------------------------------------------
    def connect(self, port, baud):
        self.disconnect()
        self.ser = serial.Serial(port, baud, timeout=0.2)
        self.port, self.baud = port, baud
        time.sleep(2.0)
        self.ser.reset_input_buffer()
        self._stop.clear()
        self._thread = threading.Thread(target=self._reader, daemon=True)
        self._thread.start()

    def disconnect(self):
        self._stop.set()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=1.0)
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except OSError:
                pass
        self.ser = None

    @property
    def connected(self):
        return self.ser is not None and self.ser.is_open

    def send(self, cmd):
        if not self.connected:
            return False
        try:
            self.ser.write((cmd.strip() + "\n").encode("ascii", "ignore"))
            return True
        except (OSError, serial.SerialException):
            return False

    def _reader(self):
        buf = b""
        while not self._stop.is_set():
            try:
                chunk = self.ser.read(512)
            except (OSError, serial.SerialException, AttributeError):
                break
            if chunk:
                buf += chunk
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    self.on_line(raw.decode("ascii", "ignore").strip("\r"))
            else:
                time.sleep(0.01)


# ---------------------------------------------------------------------------
# Interface
# ---------------------------------------------------------------------------

class App(tk.Tk):

    def __init__(self):
        super().__init__()
        self.title("Console Marlin — MKS TinyBee / CoreXY")
        self.geometry("1180x800")
        self.minsize(960, 680)

        self.rx = queue.Queue()
        self.link = SerialLink(self.rx.put)

        # estado observado
        self.temps = {"h": 0.0, "ht": 0.0, "b": 0.0, "bt": 0.0}
        self.pos = {"X": 0.0, "Y": 0.0, "Z": 0.0, "E": 0.0}
        self.endstops = {}
        self.last_endstop_poll = 0.0
        # fila de envio com controle de fluxo por 'ok'
        self.txq = deque()
        self.awaiting_ok = False
        self.last_rx = 0.0

        # passos/mm correntes (espelham o firmware; M503 atualiza)
        self.var_steps = {ax: tk.DoubleVar(value=v) for ax, v in
                          (("X", 80.0), ("Y", 80.0), ("Z", 400.0), ("E", 415.0))}

        # calibracao de passos/mm
        self.cal_axis = tk.StringVar(value="X")
        self.cal_dir = tk.IntVar(value=1)
        self.cal_dist = tk.DoubleVar(value=100.0)
        self.cal_measured = tk.DoubleVar(value=0.0)
        self.cal_commanded = None
        self.cal_new = None
        self.cal_history = []

        self.logfile = None
        self.logpath = None
        self._open_logfile()
        self._found = None
        self.monitoring = tk.BooleanVar(value=False)
        self.jog_step = tk.DoubleVar(value=10.0)
        self.jog_feed = tk.IntVar(value=600)

        self._build_ui()
        self.after(60, self._drain)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # -- construção da interface ------------------------------------------
    def _build_ui(self):
        root = ttk.Frame(self, padding=8)
        root.pack(fill="both", expand=True)

        self._build_connbar(root)

        body = ttk.Frame(root)
        body.pack(fill="both", expand=True, pady=(8, 0))
        body.columnconfigure(0, weight=0, minsize=430)
        body.columnconfigure(1, weight=1)
        body.rowconfigure(0, weight=1)

        left = ttk.Frame(body)
        left.grid(row=0, column=0, sticky="nsew", padx=(0, 8))
        right = ttk.Frame(body)
        right.grid(row=0, column=1, sticky="nsew")

        self._build_status(left)
        self._build_actions(left)
        self._build_console(right)

    def _build_connbar(self, parent):
        bar = ttk.LabelFrame(parent, text="Conexão", padding=6)
        bar.pack(fill="x")

        self.btn_detect = ttk.Button(bar, text="Localizar placa",
                                     command=self.do_autodetect)
        self.btn_detect.pack(side="left")

        ttk.Label(bar, text="Porta:").pack(side="left", padx=(12, 3))
        self.cmb_port = ttk.Combobox(bar, width=14, state="readonly")
        self.cmb_port.pack(side="left")
        self._refresh_ports()

        ttk.Label(bar, text="Baud:").pack(side="left", padx=(10, 3))
        self.cmb_baud = ttk.Combobox(bar, width=9, state="readonly",
                                     values=[str(b) for b in BAUDRATES])
        self.cmb_baud.set("250000")
        self.cmb_baud.pack(side="left")

        ttk.Button(bar, text="Atualizar portas",
                   command=self._refresh_ports).pack(side="left", padx=6)
        self.btn_conn = ttk.Button(bar, text="Conectar", command=self.toggle_conn)
        self.btn_conn.pack(side="left", padx=4)

        self.lbl_conn = tk.Label(bar, text="desconectado", bg=BG_IDLE, fg="white",
                                 padx=10, pady=3)
        self.lbl_conn.pack(side="left", padx=10)

        # Parada de emergência sempre visível
        tk.Button(bar, text="PARADA (M112)", bg=BG_ERR, fg="white",
                  font=("TkDefaultFont", 10, "bold"), padx=14,
                  command=self.emergency_stop).pack(side="right")

    def _build_status(self, parent):
        card = ttk.LabelFrame(parent, text="Status", padding=8)
        card.pack(fill="x")

        # temperaturas
        t = ttk.Frame(card)
        t.pack(fill="x")
        self.lbl_hot = tk.Label(t, text="Bico  --- / ---", bg=BG_IDLE, fg="white",
                                font=("TkDefaultFont", 11, "bold"),
                                width=18, pady=6)
        self.lbl_hot.pack(side="left", padx=(0, 6))
        self.lbl_bed = tk.Label(t, text="Mesa  --- / ---", bg=BG_IDLE, fg="white",
                                font=("TkDefaultFont", 11, "bold"),
                                width=18, pady=6)
        self.lbl_bed.pack(side="left")

        # posição
        self.lbl_pos = ttk.Label(card, text="Posição:  X ---   Y ---   Z ---   E ---",
                                 font=("TkFixedFont", 10))
        self.lbl_pos.pack(anchor="w", pady=(8, 4))

        # fins de curso
        ttk.Separator(card).pack(fill="x", pady=4)
        ttk.Label(card, text="Fins de curso e sensores",
                  font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        self.frm_es = ttk.Frame(card)
        self.frm_es.pack(fill="x", pady=4)
        self.es_labels = {}
        ttk.Label(card, text="NF em série: MIN e MAX do mesmo eixo acendem a mesma linha.",
                  foreground="#666").pack(anchor="w")

        m = ttk.Frame(card)
        m.pack(fill="x", pady=(6, 0))
        ttk.Checkbutton(m, text="Monitorar continuamente",
                        variable=self.monitoring,
                        command=self._toggle_monitor).pack(side="left")
        ttk.Button(m, text="Ler agora (M119)",
                   command=lambda: self.send("M119")).pack(side="left", padx=6)

    def _build_actions(self, parent):
        nb = ttk.Notebook(parent)
        nb.pack(fill="both", expand=True, pady=(8, 0))
        nb.add(self._tab_motion(nb), text="Movimento")
        nb.add(self._tab_temp(nb), text="Temperatura")
        nb.add(self._tab_probe(nb), text="Probe / Nivelamento")
        nb.add(self._tab_steps(nb), text="Passos/mm")
        nb.add(self._tab_calib(nb), text="Calibração")
        nb.add(self._tab_firmware(nb), text="Firmware")

    # --- aba movimento ---
    def _scrollable(self, parent):
        """Devolve (container, interior) com rolagem vertical.

        O Notebook do tkinter nao rola sozinho: quando o conteudo de uma aba
        passa da altura da janela, ele simplesmente fica inacessivel.
        """
        outer = ttk.Frame(parent)
        canvas = tk.Canvas(outer, highlightthickness=0, borderwidth=0)
        vsb = ttk.Scrollbar(outer, orient="vertical", command=canvas.yview)
        inner = ttk.Frame(canvas, padding=8)

        inner.bind("<Configure>",
                   lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        win = canvas.create_window((0, 0), window=inner, anchor="nw")
        canvas.bind("<Configure>", lambda e: canvas.itemconfigure(win, width=e.width))
        canvas.configure(yscrollcommand=vsb.set)

        vsb.pack(side="right", fill="y")
        canvas.pack(side="left", fill="both", expand=True)

        # roda do mouse, ativa so enquanto o ponteiro esta sobre esta aba
        def _wheel(e):
            canvas.yview_scroll(int(-e.delta / 120), "units")
        inner.bind("<Enter>", lambda e: canvas.bind_all("<MouseWheel>", _wheel))
        inner.bind("<Leave>", lambda e: canvas.unbind_all("<MouseWheel>"))
        return outer, inner

    def _tab_motion(self, parent):
        outer, f = self._scrollable(parent)

        warn = ttk.Label(
            f, foreground="#a33",
            text="CoreXY: se o eixo andar na diagonal, um motor está invertido.\n"
                 "Teste as direções ANTES de qualquer homing.",
            justify="left")
        warn.pack(anchor="w", pady=(0, 6))

        s = ttk.Frame(f)
        s.pack(fill="x")
        ttk.Label(s, text="Passo (mm):").pack(side="left")
        for v in (0.1, 1, 10, 50):
            ttk.Radiobutton(s, text=str(v), value=float(v),
                            variable=self.jog_step).pack(side="left", padx=2)
        ttk.Label(s, text="  F:").pack(side="left", padx=(8, 2))
        ttk.Spinbox(s, from_=60, to=6000, increment=60, width=6,
                    textvariable=self.jog_feed).pack(side="left")

        grid = ttk.Frame(f)
        grid.pack(pady=10)
        mk = lambda p, txt, ax, sg, r, c: ttk.Button(
            p, text=txt, width=7, command=lambda: self.jog(ax, sg)
        ).grid(row=r, column=c, padx=3, pady=3)

        mk(grid, "Y+", "Y", +1, 0, 1)
        mk(grid, "X-", "X", -1, 1, 0)
        ttk.Button(grid, text="M114", width=7,
                   command=lambda: self.send("M114")).grid(row=1, column=1)
        mk(grid, "X+", "X", +1, 1, 2)
        mk(grid, "Y-", "Y", -1, 2, 1)
        mk(grid, "Z+", "Z", +1, 0, 4)
        mk(grid, "Z-", "Z", -1, 2, 4)
        ttk.Label(grid, text="Z").grid(row=1, column=4)

        ttk.Separator(f).pack(fill="x", pady=6)
        ttk.Label(f, text="Achar a velocidade maxima do mecanismo",
                  font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        ttk.Label(f, foreground="#666", justify="left",
                  text="Marque a posicao do carro, rode os ciclos e veja se ele volta\n"
                       "para a mesma marca. Se saiu do lugar, perdeu passos naquela velocidade.").pack(anchor="w")

        self.var_spd = tk.IntVar(value=60)
        self.var_cyc = tk.IntVar(value=10)
        sp = ttk.Frame(f); sp.pack(fill="x", pady=4)
        ttk.Label(sp, text="mm/s:").pack(side="left")
        ttk.Spinbox(sp, from_=10, to=400, increment=10, width=5,
                    textvariable=self.var_spd).pack(side="left", padx=3)
        ttk.Label(sp, text="ciclos:").pack(side="left", padx=(8, 0))
        ttk.Spinbox(sp, from_=1, to=50, increment=1, width=4,
                    textvariable=self.var_cyc).pack(side="left", padx=3)
        ttk.Button(sp, text="Testar X", width=9,
                   command=lambda: self.speed_test("X")).pack(side="left", padx=3)
        ttk.Button(sp, text="Testar Y", width=9,
                   command=lambda: self.speed_test("Y")).pack(side="left", padx=3)
        ttk.Button(sp, text="Testar XY", width=10,
                   command=lambda: self.speed_test("XY")).pack(side="left", padx=3)

        ttk.Separator(f).pack(fill="x", pady=6)
        ttk.Label(f, text="Diagnostico CoreXY — aciona UM motor de cada vez",
                  font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        ttk.Label(f, foreground="#666", justify="left",
                  text="Motor A = X+Y, Motor B = X-Y. Mover na diagonal zera um dos dois.\n"
                       "Use para identificar motor mudo, bobina trocada ou inversao.").pack(anchor="w")
        cx = ttk.Frame(f); cx.pack(fill="x", pady=4)
        ttk.Button(cx, text="So motor A (conector X)", width=22,
                   command=lambda: self.core_single("A")).pack(side="left", padx=3)
        ttk.Button(cx, text="So motor B (conector Y)", width=22,
                   command=lambda: self.core_single("B")).pack(side="left", padx=3)

        ttk.Separator(f).pack(fill="x", pady=6)
        h = ttk.Frame(f)
        h.pack(fill="x")
        ttk.Label(h, text="Homing (um eixo por vez, com a mão no botão):").pack(anchor="w")
        hb = ttk.Frame(f)
        hb.pack(fill="x", pady=4)
        for txt, cmd in (("G28 X", "G28 X"), ("G28 Y", "G28 Y"), ("G28 Z", "G28 Z")):
            ttk.Button(hb, text=txt, width=9,
                       command=lambda c=cmd: self.confirm_send(
                           c, f"Executar {c}?\n\nConfirme antes que o eixo esteja livre.")
                       ).pack(side="left", padx=3)
        ttk.Button(hb, text="G28 (todos)", width=12,
                   command=lambda: self.confirm_send(
                       "G28", "Homing de TODOS os eixos.\n\n"
                              "Só faça isso depois de validar as direções "
                              "e os fins de curso individualmente.")
                   ).pack(side="left", padx=3)

        ttk.Separator(f).pack(fill="x", pady=6)
        o = ttk.Frame(f)
        o.pack(fill="x")
        ttk.Button(o, text="Desligar motores (M18)",
                   command=lambda: self.send("M18")).pack(side="left", padx=3)
        ttk.Button(o, text="Ligar motores (M17)",
                   command=lambda: self.send("M17")).pack(side="left", padx=3)
        return outer

    # --- aba temperatura ---
    def _tab_temp(self, parent):
        outer, f = self._scrollable(parent)

        self.var_hot = tk.IntVar(value=200)
        self.var_bed = tk.IntVar(value=60)

        r1 = ttk.Frame(f); r1.pack(fill="x", pady=3)
        ttk.Label(r1, text="Bico (°C):", width=12).pack(side="left")
        ttk.Spinbox(r1, from_=0, to=260, increment=5, width=6,
                    textvariable=self.var_hot).pack(side="left")
        ttk.Button(r1, text="Aquecer", command=lambda: self.confirm_send(
            f"M104 S{self.var_hot.get()}",
            f"Aquecer o bico a {self.var_hot.get()}°C?")).pack(side="left", padx=4)
        ttk.Button(r1, text="Desligar",
                   command=lambda: self.send("M104 S0")).pack(side="left")

        r2 = ttk.Frame(f); r2.pack(fill="x", pady=3)
        ttk.Label(r2, text="Mesa (°C):", width=12).pack(side="left")
        ttk.Spinbox(r2, from_=0, to=120, increment=5, width=6,
                    textvariable=self.var_bed).pack(side="left")
        ttk.Button(r2, text="Aquecer", command=lambda: self.confirm_send(
            f"M140 S{self.var_bed.get()}",
            f"Aquecer a mesa a {self.var_bed.get()}°C?")).pack(side="left", padx=4)
        ttk.Button(r2, text="Desligar",
                   command=lambda: self.send("M140 S0")).pack(side="left")

        ttk.Button(f, text="DESLIGAR TUDO (M104 S0 / M140 S0 / M107)",
                   command=self.all_heaters_off).pack(fill="x", pady=8)

        ttk.Separator(f).pack(fill="x", pady=6)
        ttk.Label(f, text="Ventoinha de peça (fan 0 = conector FAN1 da placa):").pack(anchor="w")
        fan = ttk.Frame(f); fan.pack(fill="x", pady=3)
        for txt, val in (("0%", 0), ("50%", 128), ("100%", 255)):
            ttk.Button(fan, text=txt, width=7,
                       command=lambda v=val: self.send(f"M106 P0 S{v}")).pack(side="left", padx=3)

        ttk.Separator(f).pack(fill="x", pady=6)
        ttk.Label(f, text="Identificar qual ventoinha é qual",
                  font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        ttk.Label(f, foreground="#666", justify="left",
                  text="Liga uma de cada vez. Atencao: fan 0 do Marlin = FAN1 da placa,\n"
                       "fan 1 do Marlin = FAN2 da placa (a numeracao e deslocada).").pack(anchor="w")
        fid = ttk.Frame(f); fid.pack(fill="x", pady=4)
        ttk.Button(fid, text="Só FAN1 (M106 P0)", width=20,
                   command=lambda: self.fan_solo(0)).pack(side="left", padx=3)
        ttk.Button(fid, text="Só FAN2 (M106 P1)", width=20,
                   command=lambda: self.fan_solo(1)).pack(side="left", padx=3)
        ttk.Button(fid, text="Desligar as duas", width=16,
                   command=self.fans_off).pack(side="left", padx=3)

        ttk.Separator(f).pack(fill="x", pady=6)
        ttk.Label(f, text="Achar onde a 5015 trava (FAN_MIN_PWM)",
                  font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        ttk.Label(f, foreground="#666", justify="left",
                  text="Desca de cima para baixo. O ultimo valor em que ela ainda gira\n"
                       "firme, mais uma folga, vira o FAN_MIN_PWM.").pack(anchor="w")
        ramp = ttk.Frame(f); ramp.pack(fill="x", pady=4)
        for v in (255, 254, 250, 245, 240, 230, 220, 210, 200):
            ttk.Button(ramp, text=str(v), width=5,
                       command=lambda n=v: self.fan_level(n)).pack(side="left", padx=1)
        ramp2 = ttk.Frame(f); ramp2.pack(fill="x", pady=2)
        for v in (180, 160, 140, 128, 100, 80):
            ttk.Button(ramp2, text=str(v), width=5,
                       command=lambda n=v: self.fan_level(n)).pack(side="left", padx=1)

        ttk.Separator(f).pack(fill="x", pady=6)
        ttk.Label(f, text="Auto-ajuste de PID (leva alguns minutos):").pack(anchor="w")
        pid = ttk.Frame(f); pid.pack(fill="x", pady=3)
        ttk.Button(pid, text="PID bico (M303 E0)",
                   command=lambda: self.confirm_send(
                       f"M303 E0 S{self.var_hot.get()} C8 U1",
                       "Iniciar auto-ajuste de PID do bico?\n"
                       "Demora vários minutos e aquece o hotend.")).pack(side="left", padx=3)
        ttk.Button(pid, text="PID mesa (M303 E-1)",
                   command=lambda: self.confirm_send(
                       f"M303 E-1 S{self.var_bed.get()} C8 U1",
                       "Iniciar auto-ajuste de PID da mesa?\n"
                       "Requer PIDTEMPBED habilitado no firmware.")).pack(side="left", padx=3)

        ttk.Separator(f).pack(fill="x", pady=6)
        ttk.Label(f, text="Extrusão (exige bico acima de EXTRUDE_MINTEMP = 170°C):",
                  foreground="#666").pack(anchor="w")
        ex = ttk.Frame(f); ex.pack(fill="x", pady=3)
        for mm in (10, 50, 100):
            ttk.Button(ex, text=f"Extrudar {mm}mm", width=14,
                       command=lambda m=mm: self.extrude(m)).pack(side="left", padx=3)
        ttk.Button(ex, text="Retrair 10mm",
                   command=lambda: self.extrude(-10)).pack(side="left", padx=3)
        return outer

    # --- aba probe ---
    def _tab_probe(self, parent):
        outer, f = self._scrollable(parent)

        ttk.Label(f, text=f"Probe por servo — ângulos do firmware: "
                       f"{SERVO_DEPLOY_ANGLE}° ativo, {SERVO_STOW_ANGLE}° recolhido",
                  font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        sv = ttk.Frame(f); sv.pack(fill="x", pady=6)
        ttk.Button(sv, text=f"Descer probe (M280 P0 S{SERVO_DEPLOY_ANGLE})",
                   command=lambda: self.send(f"M280 P0 S{SERVO_DEPLOY_ANGLE}")).pack(side="left", padx=3)
        ttk.Button(sv, text=f"Recolher (M280 P0 S{SERVO_STOW_ANGLE})",
                   command=lambda: self.send(f"M280 P0 S{SERVO_STOW_ANGLE}")).pack(side="left", padx=3)
        ttk.Button(sv, text="Testar deploy/stow (M401/M402)",
                   command=lambda: self.send("M401")).pack(side="left", padx=3)

        # ---- Busca do angulo correto ----
        ttk.Separator(f).pack(fill="x", pady=8)
        ttk.Label(f, text="Buscar o angulo certo do servo",
                  font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        ttk.Label(f, foreground="#666", justify="left",
                  text="Mova de pouco em pouco ate achar os dois angulos: o que baixa o\n"
                       "braco totalmente e o que recolhe sem forcar o batente.").pack(anchor="w")

        self.var_ang = tk.IntVar(value=SERVO_STOW_ANGLE)

        a1 = ttk.Frame(f); a1.pack(fill="x", pady=6)
        ttk.Label(a1, text="Angulo:").pack(side="left")
        ttk.Spinbox(a1, from_=0, to=180, increment=1, width=6,
                    textvariable=self.var_ang).pack(side="left", padx=4)
        ttk.Button(a1, text="Enviar (M280 P0)",
                   command=self.servo_send).pack(side="left", padx=3)
        for d in (-10, -5, -1, +1, +5, +10):
            ttk.Button(a1, text=f"{d:+d}", width=4,
                       command=lambda v=d: self.servo_step(v)).pack(side="left", padx=1)

        self.scl_ang = ttk.Scale(f, from_=0, to=180, orient="horizontal",
                                 command=self._servo_slide)
        self.scl_ang.set(SERVO_STOW_ANGLE)
        self.scl_ang.pack(fill="x", pady=4)
        self.scl_ang.bind("<ButtonRelease-1>", lambda _: self.servo_send())
        ttk.Label(f, foreground="#666",
                  text="O slider envia ao soltar o botao do mouse.").pack(anchor="w")

        a2 = ttk.Frame(f); a2.pack(fill="x", pady=4)
        ttk.Button(a2, text="Soltar servo (M282)",
                   command=lambda: self.send("M282 P0")).pack(side="left", padx=3)
        ttk.Button(a2, text="Usar como ATIVO",
                   command=lambda: self._servo_adopt("deploy")).pack(side="left", padx=3)
        ttk.Button(a2, text="Usar como RECOLHIDO",
                   command=lambda: self._servo_adopt("stow")).pack(side="left", padx=3)

        self.lbl_ang = ttk.Label(
            f, foreground="#1f7a3f",
            text=f"Firmware atual:  ativo {SERVO_DEPLOY_ANGLE}°   recolhido {SERVO_STOW_ANGLE}°")
        self.lbl_ang.pack(anchor="w", pady=4)

        ttk.Separator(f).pack(fill="x", pady=8)
        ttk.Label(f, text="Offset Z do probe").pack(anchor="w")
        o = ttk.Frame(f); o.pack(fill="x", pady=3)
        self.var_zoff = tk.DoubleVar(value=0.0)
        ttk.Label(o, text="M851 Z:").pack(side="left")
        ttk.Spinbox(o, from_=-20, to=20, increment=0.05, width=8, format="%.2f",
                    textvariable=self.var_zoff).pack(side="left", padx=4)
        ttk.Button(o, text="Aplicar",
                   command=lambda: self.send(f"M851 Z{self.var_zoff.get():.2f}")).pack(side="left")
        ttk.Button(o, text="Ler atual",
                   command=lambda: self.send("M851")).pack(side="left", padx=4)

        # ---- Medir o offset Z com a folha de papel ----
        ttk.Separator(f).pack(fill="x", pady=8)
        ttk.Label(f, text="Medir o offset Z com a folha de papel",
                  font=("TkDefaultFont", 9, "bold")).pack(anchor="w")

        ttk.Label(f, foreground="#666", justify="left", text=(
            "PARA QUE SERVE\n"
            "  O M851 Z guarda a distancia vertical entre a ponta do bico e a ponta\n"
            "  do probe acionado. Se esse numero estiver errado, o Z=0 do firmware\n"
            "  nao coincide com a mesa: ou a primeira camada fica no ar, ou o bico\n"
            "  raspa. Toda vez que o angulo do servo ou a montagem do probe mudar,\n"
            "  este valor precisa ser medido de novo.\n"
            "\n"
            "COMO O PAINEL MEDE\n"
            "  O botao 'Preparar' zera o M851, faz o home e leva o bico ao Z=0\n"
            "  reportado. Com o offset zerado, a altura que ainda sobra ate a mesa\n"
            "  E o proprio offset. Voce desce de pouco em pouco ate a folha de papel\n"
            "  arrastar com leve atrito, e o botao 'Gravar' converte a descida\n"
            "  acumulada em M851 Z.")).pack(anchor="w", pady=(2, 6))

        ttk.Label(f, foreground="#a33", justify="left", text=(
            "POR QUE A TRAVA PRECISA SAIR\n"
            "  O Z_MIN_POS e 0: o firmware recusa qualquer movimento abaixo disso.\n"
            "  Como o bico precisa descer ate ABAIXO do zero reportado para achar a\n"
            "  mesa, o 'Preparar' envia M211 S0 e desliga os limites por software.\n"
            "  Enquanto estiver assim NAO HA PROTECAO -- nem em Z, nem em X e Y.\n"
            "  Nao movimente a maquina por outras abas durante a medicao. O botao\n"
            "  'Gravar' e o 'Abortar' religam a trava (M211 S1); reiniciar a placa\n"
            "  tambem religa, porque o M211 nao vai para a EEPROM.\n"
            "\n"
            "ANTES DE COMECAR: bico FRIO e SEM filamento. Um residuo na ponta altera\n"
            "a medida e ainda empurra a folha.")).pack(anchor="w", pady=(0, 6))

        self.zoff_travel = 0.0
        self.zoff_armed = False

        z1 = ttk.Frame(f); z1.pack(fill="x", pady=4)
        ttk.Button(z1, text="1. Preparar", width=14,
                   command=self.zoff_prepare).pack(side="left", padx=3)
        ttk.Button(z1, text="Abortar (religa trava e sobe)",
                   command=self.zoff_abort).pack(side="left", padx=3)

        self.lbl_zoff_state = ttk.Label(f, foreground="#666",
                                        text="Estado: nao preparado.")
        self.lbl_zoff_state.pack(anchor="w")

        ttk.Label(f, foreground="#666", justify="left", text=(
            "Desca em degraus grandes ate sentir a folha prender, depois refine.\n"
            "O ponto certo e quando a folha ainda desliza, mas com atrito."
        )).pack(anchor="w", pady=(6, 2))

        z2 = ttk.Frame(f); z2.pack(fill="x", pady=4)
        ttk.Label(z2, text="Descer:").pack(side="left")
        for d in (-0.5, -0.1, -0.05, -0.02):
            ttk.Button(z2, text=f"{d:+.2f}", width=6,
                       command=lambda v=d: self.zoff_step(v)).pack(side="left", padx=2)
        ttk.Label(z2, text="   Subir:").pack(side="left", padx=(10, 0))
        for d in (+0.02, +0.05, +0.1, +0.5):
            ttk.Button(z2, text=f"{d:+.2f}", width=6,
                       command=lambda v=d: self.zoff_step(v)).pack(side="left", padx=2)

        self.lbl_zoff = ttk.Label(f, foreground="#1f7a3f",
                                  font=("TkDefaultFont", 9, "bold"),
                                  text="Descida acumulada: 0.000 mm")
        self.lbl_zoff.pack(anchor="w", pady=4)

        z3 = ttk.Frame(f); z3.pack(fill="x", pady=4)
        ttk.Button(z3, text="2. Gravar offset", width=18,
                   command=self.zoff_save).pack(side="left", padx=3)
        ttk.Button(z3, text="Conferir (M114)",
                   command=self.zoff_check).pack(side="left", padx=3)

        ttk.Label(f, foreground="#666", justify="left", text=(
            "'Gravar' envia M851 Z<medido>, religa a trava com M211 S1 e salva com\n"
            "M500. Depois disso: G28 e G1 Z0 -- a folha tem que arrastar igual.\n"
            "Lembre que o X e o Y do NOZZLE_TO_PROBE_OFFSET tambem mudam quando o\n"
            "arco do servo muda; esses precisam ser remedidos a parte."
        )).pack(anchor="w", pady=(0, 2))

        ttk.Separator(f).pack(fill="x", pady=8)
        b = ttk.Frame(f); b.pack(fill="x", pady=3)
        ttk.Button(b, text="Sondar ponto atual (G30)",
                   command=lambda: self.confirm_send(
                       "G30", "Sondar a mesa no ponto atual?\n"
                              "Exija que o eixo Z já tenha sido referenciado.")).pack(side="left", padx=3)
        ttk.Button(b, text="Repetibilidade (M48)",
                   command=lambda: self.confirm_send(
                       "M48", "Teste de repetibilidade do probe.\n"
                              "Faz várias sondagens no mesmo ponto.")).pack(side="left", padx=3)

        ttk.Separator(f).pack(fill="x", pady=8)
        ttk.Label(f, text="Nivelamento (G29)",
                  font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        ttk.Label(f, foreground="#666", justify="left",
                  text="Montagem do probe baixada: D = 2.82mm, alcance de busca de\n"
                       "2.3mm (Z_PROBE_LOW_POINT -2.3). A inclinacao medida da mesa\n"
                       "e de 0.59mm, entao a mesa inteira cabe no alcance. O quadrado\n"
                       "central continua util para uma verificacao rapida.").pack(anchor="w")

        self.var_mesh = tk.IntVar(value=100)
        g0 = ttk.Frame(f); g0.pack(fill="x", pady=4)
        ttk.Label(g0, text="Quadrado central (mm):").pack(side="left")
        ttk.Spinbox(g0, from_=40, to=200, increment=10, width=5,
                    textvariable=self.var_mesh).pack(side="left", padx=4)
        ttk.Button(g0, text="Nivelar area central", width=20,
                   command=self.mesh_center).pack(side="left", padx=3)
        ttk.Button(g0, text="Mesa inteira", width=14,
                   command=self.mesh_full).pack(side="left", padx=3)

        g = ttk.Frame(f); g.pack(fill="x", pady=3)
        ttk.Button(g, text="Ver malha (M420 V)",
                   command=lambda: self.send("M420 V")).pack(side="left", padx=3)
        ttk.Button(g, text="Ligar nivelamento (M420 S1)",
                   command=lambda: self.send("M420 S1")).pack(side="left", padx=3)
        ttk.Button(g, text="Desligar (M420 S0)",
                   command=lambda: self.send("M420 S0")).pack(side="left", padx=3)
        return outer

    # --- aba calibração ---
    def _tab_steps(self, parent):
        outer, f = self._scrollable(parent)

        ttk.Label(f, text="Calibração de passos por mm",
                  font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        ttk.Label(f, foreground="#666", justify="left",
                  text="Mova, meça o deslocamento REAL e informe abaixo. O ajuste e aplicado\n"
                       "com M92, que vale so ate o proximo reset. Grave na EEPROM so no final.").pack(anchor="w")

        # --- 1. movimento ---
        ttk.Separator(f).pack(fill="x", pady=6)
        ttk.Label(f, text="1. Mover", font=("TkDefaultFont", 9, "bold")).pack(anchor="w")

        m = ttk.Frame(f); m.pack(fill="x", pady=4)
        ttk.Label(m, text="Eixo:").pack(side="left")
        for ax in ("X", "Y", "Z", "E"):
            ttk.Radiobutton(m, text=ax, value=ax,
                            variable=self.cal_axis).pack(side="left", padx=2)
        ttk.Label(m, text="  Sentido:").pack(side="left", padx=(8, 0))
        ttk.Radiobutton(m, text="+", value=1, variable=self.cal_dir).pack(side="left")
        ttk.Radiobutton(m, text="−", value=-1, variable=self.cal_dir).pack(side="left")
        ttk.Label(m, text="  mm:").pack(side="left", padx=(8, 2))
        ttk.Spinbox(m, from_=1, to=300, increment=10, width=6,
                    textvariable=self.cal_dist).pack(side="left")
        ttk.Button(m, text="Mover", width=9, command=self.cal_move).pack(side="left", padx=6)

        ttk.Label(f, foreground="#666",
                  text="Dica: use 100mm ou mais. Em 10mm o erro de leitura da regua mascara o resultado."
                  ).pack(anchor="w")

        # --- 2. medida ---
        ttk.Separator(f).pack(fill="x", pady=6)
        ttk.Label(f, text="2. Informar o real", font=("TkDefaultFont", 9, "bold")).pack(anchor="w")

        d = ttk.Frame(f); d.pack(fill="x", pady=4)
        ttk.Label(d, text="Deslocou de fato (mm):").pack(side="left")
        e = ttk.Entry(d, textvariable=self.cal_measured, width=10)
        e.pack(side="left", padx=4)
        e.bind("<Return>", lambda _: self.cal_compute())
        ttk.Button(d, text="Calcular", command=self.cal_compute).pack(side="left", padx=3)

        self.lbl_cal = ttk.Label(f, justify="left", font=("TkFixedFont", 9),
                                 text="Atual: —    Novo: —    Erro: —")
        self.lbl_cal.pack(anchor="w", pady=6)

        # --- 3. aplicar ---
        ttk.Separator(f).pack(fill="x", pady=6)
        ttk.Label(f, text="3. Aplicar e repetir", font=("TkDefaultFont", 9, "bold")).pack(anchor="w")

        a = ttk.Frame(f); a.pack(fill="x", pady=4)
        self.btn_cal_apply = ttk.Button(a, text="Aplicar (M92)", state="disabled",
                                        command=self.cal_apply)
        self.btn_cal_apply.pack(side="left", padx=3)
        ttk.Button(a, text="Ler do firmware (M503)",
                   command=lambda: self.send("M503")).pack(side="left", padx=3)

        ttk.Label(f, foreground="#666", justify="left",
                  text="Repita o ciclo ate o erro ficar abaixo de ~0,2%. Cada iteracao vai\n"
                       "para o log da sessao, com eixo, comandado, medido e valores.").pack(anchor="w")

        # --- 4. gravar ---
        ttk.Separator(f).pack(fill="x", pady=6)
        ttk.Label(f, text="4. Só no final", font=("TkDefaultFont", 9, "bold")).pack(anchor="w")

        g = ttk.Frame(f); g.pack(fill="x", pady=4)
        ttk.Button(g, text="Gravar na EEPROM (M500)",
                   command=self.cal_save).pack(side="left", padx=3)
        ttk.Button(g, text="Gerar linha do Configuration.h",
                   command=self.cal_emit_config).pack(side="left", padx=3)

        ttk.Label(f, foreground="#a33", justify="left",
                  text="A EEPROM sobrepoe o firmware e NAO e apagada ao regravar.\n"
                       "Prefira levar os valores para o Configuration.h no final."
                  ).pack(anchor="w", pady=6)
        return outer

    # -- calibracao de passos ----------------------------------------------
    def _cal_current(self):
        """Passos/mm atuais do eixo selecionado, conforme a aba Calibração."""
        return float(self.var_steps[self.cal_axis.get()].get())

    def cal_move(self):
        ax = self.cal_axis.get()
        dist = float(self.cal_dist.get()) * int(self.cal_dir.get())
        if ax == "E" and self.temps["h"] < 170:
            if not messagebox.askokcancel(
                    "Bico frio",
                    f"O bico esta a {self.temps['h']:.0f}°C e o Marlin bloqueia\n"
                    "extrusao abaixo de 170°C. Continuar mesmo assim?"):
                return
        # E a 100 mm/min (1.7 mm/s): em velocidade alta a extrusora patina no
        # limite de fusao e a medida de passos/mm sai falseada.
        feed = 100 if ax == "E" else (300 if ax == "Z" else self.jog_feed.get())
        self.cal_commanded = abs(dist)
        self.log(f"[CALIB] {ax}: comandando {dist:+g} mm "
                 f"(passos/mm atual = {self._cal_current():g})", "sys")
        self.send("G91")
        self.send(f"G1 {ax}{dist:g} F{feed}")
        self.send("G90")
        self.send("M400")

    def cal_compute(self):
        ax = self.cal_axis.get()
        try:
            measured = float(self.cal_measured.get())
        except (TypeError, ValueError):
            self.log("[CALIB] Valor medido invalido.", "err")
            return
        commanded = getattr(self, "cal_commanded", None) or float(self.cal_dist.get())
        if measured <= 0:
            self.log("[CALIB] O valor medido precisa ser maior que zero.", "err")
            return

        cur = self._cal_current()
        new = cur * commanded / measured
        err = (measured - commanded) / commanded * 100.0
        self.cal_new = round(new, 3)

        self.lbl_cal.config(
            text=f"Eixo {ax}   comandado {commanded:g} mm   medido {measured:g} mm\n"
                 f"Atual: {cur:.3f}    Novo: {self.cal_new:.3f}    Erro: {err:+.2f}%")
        self.btn_cal_apply.config(state="normal")
        self.log(f"[CALIB] {ax}: comandado {commanded:g}, medido {measured:g}, "
                 f"erro {err:+.2f}%. {cur:.3f} -> {self.cal_new:.3f} passos/mm", "ok")

    def cal_apply(self):
        ax = self.cal_axis.get()
        if self.cal_new is None:
            return
        self.var_steps[ax].set(self.cal_new)
        self.send(f"M92 {ax}{self.cal_new:g}")
        self.cal_history.append((ax, self.cal_new))
        self.log(f"[CALIB] Aplicado M92 {ax}{self.cal_new:g} (volatil, "
                 f"ainda NAO gravado na EEPROM).", "ok")
        self.btn_cal_apply.config(state="disabled")
        self.cal_measured.set(0.0)

    def cal_save(self):
        if not messagebox.askokcancel(
                "Gravar na EEPROM",
                "Gravar os passos/mm atuais na EEPROM com M500?\n\n"
                "A partir daqui a EEPROM passa a mandar, e regravar o\n"
                "firmware nao apaga esses valores (use M502 + M500 para zerar).\n\n"
                "Um resumo completo sera gravado no log da sessao."):
            return
        self.cal_summary()
        self.send("M500")
        self.log("[CALIB] M500 enviado. Valores gravados na EEPROM.", "ok")

    def cal_summary(self):
        """Bloco auto-contido no log, para levar os valores ao Configuration.h."""
        v = self.var_steps
        linha = (f"#define DEFAULT_AXIS_STEPS_PER_UNIT   "
                 f"{{ {v['X'].get():g}, {v['Y'].get():g}, "
                 f"{v['Z'].get():g}, {v['E'].get():g} }}")
        sep = "=" * 66
        self.log(f"[CALIB] {sep}", "ok")
        self.log("[CALIB] RESUMO DA CALIBRACAO DE PASSOS/MM", "ok")
        self.log(f"[CALIB] Data: {time.ctime()}", "ok")
        self.log(f"[CALIB] Firmware: {getattr(self, 'fw_banner', 'nao lido (envie M115)')}", "ok")
        self.log(f"[CALIB] Conexao: {self.link.port} @ {self.link.baud}", "ok")
        self.log("[CALIB] ---- valores finais ----", "ok")
        for ax in ("X", "Y", "Z", "E"):
            self.log(f"[CALIB]   {ax} = {v[ax].get():g} passos/mm", "ok")
        self.log("[CALIB] ---- linha para o Marlin/Configuration.h ----", "ok")
        self.log(f"[CALIB] {linha}", "ok")
        if self.cal_history:
            self.log("[CALIB] ---- iteracoes desta sessao ----", "ok")
            for i, (ax, val) in enumerate(self.cal_history, 1):
                self.log(f"[CALIB]   {i}. {ax} -> {val:g}", "ok")
        else:
            self.log("[CALIB] (nenhuma iteracao aplicada nesta sessao)", "ok")
        self.log(f"[CALIB] {sep}", "ok")

    def cal_emit_config(self):
        v = self.var_steps
        linha = (f"#define DEFAULT_AXIS_STEPS_PER_UNIT   "
                 f"{{ {v['X'].get():g}, {v['Y'].get():g}, "
                 f"{v['Z'].get():g}, {v['E'].get():g} }}")
        self.log("[CALIB] Linha pronta para o Configuration.h:", "ok")
        self.log(f"[CALIB] {linha}", "ok")

    def _tab_firmware(self, parent):
        outer, f = self._scrollable(parent)

        ttk.Label(f, text="Compilar e gravar o firmware",
                  font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        ttk.Label(f, foreground="#666", justify="left",
                  text="O console libera a porta serial sozinho antes de gravar\n"
                       "e reconecta no final. Nao e preciso desconectar na mao.").pack(anchor="w")

        ttk.Label(f, text=f"Projeto: {PROJECT_DIR}", foreground="#666").pack(anchor="w", pady=(6, 0))
        ttk.Label(f, text=f"Ambiente: {PIO_ENV}", foreground="#666").pack(anchor="w")

        b = ttk.Frame(f)
        b.pack(fill="x", pady=10)
        self.btn_build = ttk.Button(b, text="Só compilar", width=16,
                                    command=lambda: self.run_pio(upload=False))
        self.btn_build.pack(side="left", padx=3)
        self.btn_flash = ttk.Button(b, text="Compilar e gravar", width=20,
                                    command=lambda: self.run_pio(upload=True))
        self.btn_flash.pack(side="left", padx=3)

        self.lbl_pio = ttk.Label(f, text="ocioso", foreground="#666")
        self.lbl_pio.pack(anchor="w", pady=4)

        ttk.Label(f, foreground="#a33", justify="left",
                  text="Depois de gravar, lembre que a EEPROM NAO e apagada.\n"
                       "Se um valor salvo com M500 estiver errado, use M502 e M500."
                  ).pack(anchor="w", pady=8)
        return outer

    def run_pio(self, upload):
        """Compila (e opcionalmente grava), liberando a porta antes."""
        if not PIO_EXE.exists():
            self.log(f"PlatformIO nao encontrado em {PIO_EXE}", "err")
            return
        acao = "compilar e gravar" if upload else "compilar"
        if not messagebox.askokcancel("Firmware", f"Deseja {acao} o firmware agora?"):
            return

        self.btn_build.config(state="disabled")
        self.btn_flash.config(state="disabled")

        reconnect = None
        if upload and self.link.connected:
            reconnect = (self.link.port, self.link.baud)
            self.log("Liberando a porta serial para a gravacao...", "sys")
            self.link.send("M155 S0")
            time.sleep(0.2)
            self.link.disconnect()
            self._set_conn_label(False)

        threading.Thread(target=self._pio_worker,
                         args=(upload, reconnect), daemon=True).start()

    def _pio_worker(self, upload, reconnect):
        cmd = [str(PIO_EXE), "run", "-e", PIO_ENV]
        if upload:
            cmd += ["-t", "upload"]
            if reconnect:
                cmd += ["--upload-port", reconnect[0]]
        self.rx.put("\x00SYS" + " ".join(cmd))
        try:
            proc = subprocess.Popen(
                cmd, cwd=str(PROJECT_DIR), stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True, encoding="utf-8",
                errors="replace", bufsize=1,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            for line in proc.stdout:
                line = line.rstrip()
                # So o que interessa, para nao inundar o console
                if any(k in line for k in ("error", "Error", "SUCCESS", "FAILED",
                                           "RAM:", "Flash:", "Hash of data",
                                           "Wrote ", "warning:")):
                    self.rx.put("\x00SYS" + line)
            proc.wait()
            ok = proc.returncode == 0
        except OSError as exc:
            self.rx.put("\x00SYS" + f"Falha ao executar o PlatformIO: {exc}")
            ok = False
        self.rx.put("\x00PIODONE" + ("1" if ok else "0") +
                    (f"|{reconnect[0]}|{reconnect[1]}" if reconnect else ""))

    def _pio_done(self, payload):
        self.btn_build.config(state="normal")
        self.btn_flash.config(state="normal")
        ok = payload[0] == "1"
        self.lbl_pio.config(text="concluido" if ok else "falhou",
                            foreground="#1f7a3f" if ok else "#9c2b2b")
        self.log("PlatformIO: SUCESSO" if ok else "PlatformIO: FALHOU",
                 "ok" if ok else "err")
        parts = payload[1:].split("|")
        if len(parts) == 3 and parts[1]:
            port, baud = parts[1], int(parts[2])
            self.log(f"Reconectando em {port}...", "sys")
            time.sleep(1.5)
            try:
                self.link.connect(port, baud)
                self._set_conn_label(True)
                self.send("M115")
                self.send("M119")
            except (OSError, serial.SerialException) as exc:
                self.log(f"Nao reconectou: {exc}", "err")

    def _tab_calib(self, parent):
        outer, f = self._scrollable(parent)

        ttk.Label(f, text="Passos por mm (M92)",
                  font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        ttk.Label(f, foreground="#666", justify="left",
                  text="Se o eixo andar o dobro ou a metade do pedido, o microstep\n"
                       "dos jumpers do TMC2209 não é 1/16. Confira a placa.").pack(anchor="w")

        sg = ttk.Frame(f); sg.pack(fill="x", pady=6)
        for i, ax in enumerate(("X", "Y", "Z", "E")):
            ttk.Label(sg, text=ax).grid(row=0, column=i * 2, padx=(8 if i else 0, 2))
            ttk.Entry(sg, textvariable=self.var_steps[ax],
                      width=9).grid(row=0, column=i * 2 + 1)

        sb = ttk.Frame(f); sb.pack(fill="x", pady=3)
        ttk.Button(sb, text="Aplicar M92", command=self.apply_steps).pack(side="left", padx=3)
        ttk.Button(sb, text="Ler do firmware (M503)",
                   command=lambda: self.send("M503")).pack(side="left", padx=3)

        ttk.Separator(f).pack(fill="x", pady=8)
        ttk.Label(f, text="EEPROM", font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        ee = ttk.Frame(f); ee.pack(fill="x", pady=4)
        ttk.Button(ee, text="Salvar (M500)",
                   command=lambda: self.confirm_send(
                       "M500", "Gravar os ajustes atuais na EEPROM?")).pack(side="left", padx=3)
        ttk.Button(ee, text="Carregar (M501)",
                   command=lambda: self.send("M501")).pack(side="left", padx=3)
        ttk.Button(ee, text="Restaurar padrões (M502)",
                   command=lambda: self.confirm_send(
                       "M502", "Restaurar TODOS os ajustes aos padrões do firmware?\n"
                               "Isso descarta a calibração salva.\n"
                               "Depois use M500 para gravar.")).pack(side="left", padx=3)
        ttk.Button(ee, text="Relatório (M503)",
                   command=lambda: self.send("M503")).pack(side="left", padx=3)

        ttk.Separator(f).pack(fill="x", pady=8)
        ttk.Label(f, text="Informações", font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        inf = ttk.Frame(f); inf.pack(fill="x", pady=4)
        for txt, cmd in (("Firmware (M115)", "M115"),
                         ("Posição (M114)", "M114"),
                         ("Temperaturas (M105)", "M105"),
                         ("Sensor filamento (M412)", "M412")):
            ttk.Button(inf, text=txt, command=lambda c=cmd: self.send(c)).pack(side="left", padx=3)

        ttk.Separator(f).pack(fill="x", pady=8)
        ttk.Label(f, text="Wi-Fi (ESP3D)", font=("TkDefaultFont", 9, "bold")).pack(anchor="w")
        w = ttk.Frame(f); w.pack(fill="x", pady=4)
        ttk.Button(w, text="Ver IP ([ESP111])",
                   command=lambda: self.send("[ESP111]")).pack(side="left", padx=3)
        ttk.Button(w, text="Status ([ESP420])",
                   command=lambda: self.send("[ESP420]")).pack(side="left", padx=3)
        return outer

    # --- console ---
    def _build_console(self, parent):
        card = ttk.LabelFrame(parent, text="Console", padding=6)
        card.pack(fill="both", expand=True)

        self.txt = tk.Text(card, wrap="none", bg="#101418", fg="#d8dee4",
                           insertbackground="#d8dee4", font=("TkFixedFont", 9))
        sb = ttk.Scrollbar(card, command=self.txt.yview)
        self.txt.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        self.txt.pack(side="top", fill="both", expand=True)

        self.txt.tag_config("tx", foreground="#6fb3ff")
        self.txt.tag_config("rx", foreground="#d8dee4")
        self.txt.tag_config("err", foreground="#ff7b72")
        self.txt.tag_config("ok", foreground="#7ee787")
        self.txt.tag_config("sys", foreground="#c9a227")

        entry = ttk.Frame(card)
        entry.pack(fill="x", pady=(6, 0))
        self.var_cmd = tk.StringVar()
        e = ttk.Entry(entry, textvariable=self.var_cmd, font=("TkFixedFont", 10))
        e.pack(side="left", fill="x", expand=True)
        e.bind("<Return>", lambda _: self.send_manual())
        e.bind("<Up>", self._hist_up)
        e.bind("<Down>", self._hist_down)
        ttk.Button(entry, text="Enviar", command=self.send_manual).pack(side="left", padx=4)
        ttk.Button(entry, text="Limpar",
                   command=lambda: self.txt.delete("1.0", "end")).pack(side="left")
        ttk.Button(entry, text="Logs",
                   command=self.open_log_folder).pack(side="left", padx=4)
        ttk.Button(entry, text="Parar fila",
                   command=self.clear_queue).pack(side="left")
        self.lbl_queue = ttk.Label(entry, text="fila: 0", foreground="#666", width=22)
        self.lbl_queue.pack(side="left", padx=6)

        self.history, self.hist_idx = [], 0
        self.log("Console pronto. Clique em 'Localizar placa' para comecar.", "sys")
        if self.logpath:
            self.log(f"Log desta sessao: {self.logpath}", "sys")

    # -- utilidades --------------------------------------------------------
    def _refresh_ports(self):
        devs = [p.device for p in SerialLink.candidate_ports()]
        self.cmb_port["values"] = devs
        if devs and not self.cmb_port.get():
            self.cmb_port.set(devs[0])

    def _open_logfile(self):
        """Abre um arquivo de log por sessao, em tools-printer/logs/."""
        try:
            LOG_DIR.mkdir(parents=True, exist_ok=True)
            path = LOG_DIR / f"sessao_{time.strftime('%Y%m%d_%H%M%S')}.log"
            self.logfile = io.open(path, "w", encoding="utf-8", buffering=1)
            self.logfile.write(f"# Console Marlin - sessao iniciada {time.ctime()}\n")
            self.logpath = path
        except OSError:
            self.logfile, self.logpath = None, None

    def _to_file(self, text, tag="rx"):
        """Grava no log da sessao, inclusive o que e filtrado da tela."""
        if self.logfile:
            try:
                self.logfile.write(f"{time.strftime('%H:%M:%S')} [{tag}] {text}\n")
            except (OSError, ValueError):
                pass

    def open_log_folder(self):
        if not self.logpath:
            self.log("Log em arquivo indisponivel.", "err")
            return
        try:
            if sys.platform == "win32":
                os.startfile(LOG_DIR)  # noqa: S606
            elif sys.platform == "darwin":
                subprocess.Popen(["open", str(LOG_DIR)])
            else:
                subprocess.Popen(["xdg-open", str(LOG_DIR)])
        except OSError as exc:
            self.log(f"Nao consegui abrir a pasta: {exc}", "err")

    def log(self, text, tag="rx"):
        self.txt.insert("end", f"{time.strftime('%H:%M:%S')}  {text}\n", tag)
        self.txt.see("end")
        self._to_file(text, tag)

    def send(self, cmd):
        """Enfileira um comando. O envio real respeita o 'ok' do Marlin.

        O Marlin tem BUFSIZE=4 comandos e nao usa XON/XOFF nesta configuracao.
        Despejar comandos em rajada estoura o buffer de recepcao e caracteres
        sao perdidos em silencio -- a placa executa comandos truncados.
        Por isso so enviamos o proximo depois do 'ok' do anterior.
        """
        if not self.link.connected:
            self.log("Nao conectado.", "err")
            return
        self.txq.append(cmd)
        self._pump()

    def send_now(self, cmd):
        """Envia imediatamente, furando a fila. So para parada de emergencia."""
        self.txq.clear()
        self.awaiting_ok = False
        self.link.send(cmd)

    def _pump(self):
        """Envia o proximo comando se o anterior ja foi confirmado."""
        if self.awaiting_ok or not self.txq or not self.link.connected:
            self._refresh_queue()
            return
        cmd = self.txq.popleft()
        self.log(f">> {cmd}", "tx")
        if self.link.send(cmd):
            self.awaiting_ok = True
            self.last_rx = time.time()
        else:
            self.log("Falha ao escrever na serial.", "err")
        self._refresh_queue()

    def _refresh_queue(self):
        n = len(self.txq)
        self.lbl_queue.config(
            text=f"fila: {n}" + ("  (aguardando ok)" if self.awaiting_ok else ""),
            foreground="#a33" if n > 20 else "#666")

    def clear_queue(self):
        n = len(self.txq)
        self.txq.clear()
        self.awaiting_ok = False
        self.log(f"Fila limpa ({n} comandos descartados).", "sys")
        self._refresh_queue()

    def send_manual(self):
        cmd = self.var_cmd.get().strip()
        if not cmd:
            return
        self.history.append(cmd)
        self.hist_idx = len(self.history)
        self.send(cmd)
        self.var_cmd.set("")

    def _hist_up(self, _):
        if self.history and self.hist_idx > 0:
            self.hist_idx -= 1
            self.var_cmd.set(self.history[self.hist_idx])

    def _hist_down(self, _):
        if self.hist_idx < len(self.history) - 1:
            self.hist_idx += 1
            self.var_cmd.set(self.history[self.hist_idx])
        else:
            self.hist_idx = len(self.history)
            self.var_cmd.set("")

    def confirm_send(self, cmd, question):
        if messagebox.askokcancel("Confirmar", question):
            self.send(cmd)

    # -- ações -------------------------------------------------------------
    def do_autodetect(self):
        self.btn_detect.config(state="disabled")
        self.log("Procurando placa...", "sys")
        threading.Thread(target=self._autodetect_worker, daemon=True).start()

    def _autodetect_worker(self):
        self._found = SerialLink.autodetect(log=lambda m: self.rx.put("\x00SYS" + m))
        self.rx.put("\x00FOUND")

    def toggle_conn(self):
        if self.link.connected:
            self.link.disconnect()
            self._set_conn_label(False)
            self.log("Desconectado.", "sys")
        else:
            port, baud = self.cmb_port.get(), int(self.cmb_baud.get())
            if not port:
                self.log("Escolha uma porta.", "err")
                return
            try:
                self.link.connect(port, baud)
            except (OSError, serial.SerialException) as exc:
                self.log(f"Erro ao abrir {port}: {exc}", "err")
                return
            self._set_conn_label(True)
            self.log(f"Conectado em {port} @ {baud}.", "ok")
            self.send("M115")
            self.send("M119")

    def _set_conn_label(self, on):
        self.lbl_conn.config(text=f"{self.link.port} @ {self.link.baud}" if on else "desconectado",
                             bg=BG_OK if on else BG_IDLE)
        self.btn_conn.config(text="Desconectar" if on else "Conectar")

    def _toggle_monitor(self):
        if not self.link.connected:
            self.monitoring.set(False)
            self.log("Conecte primeiro.", "err")
            return
        if self.monitoring.get():
            self.send(f"M155 S{TEMP_REPORT_INTERVAL}")
            self.log("Monitoramento ligado.", "sys")
        else:
            self.send("M155 S0")
            self.log("Monitoramento desligado.", "sys")

    def jog(self, axis, sign):
        step = self.jog_step.get() * sign
        self.send("G91")
        self.send(f"G1 {axis}{step:g} F{self.jog_feed.get()}")
        self.send("G90")
        self.send("M114")

    # -- nivelamento --------------------------------------------------------
    # -- offset Z pela folha de papel ---------------------------------------
    ZOFF_LIMIT = -8.0          # descida maxima tolerada sem soft endstop

    def _zoff_state(self):
        if self.zoff_armed:
            self.lbl_zoff_state.config(
                foreground="#a33",
                text="Estado: PREPARADO -- limites por software DESLIGADOS (M211 S0).")
        else:
            self.lbl_zoff_state.config(
                foreground="#666", text="Estado: nao preparado.")
        self.lbl_zoff.config(
            text=f"Descida acumulada: {self.zoff_travel:+.3f} mm"
                 f"     |     Z do firmware: {self.pos['Z']:+.3f} mm")

    def zoff_prepare(self):
        """Zera o M851, referencia a maquina e libera o Z para descer do zero."""
        if not messagebox.askokcancel(
                "Preparar medicao do offset Z",
                "Esta sequencia vai:\n\n"
                "  M851 Z0   zerar o offset do probe\n"
                "  M500      gravar (o G28 usa o valor da EEPROM)\n"
                "  G28       referenciar os tres eixos\n"
                "  M420 S0   desligar a malha de nivelamento\n"
                "  M211 S0   DESLIGAR os limites por software\n"
                "  G1 Z0     levar o bico ao zero reportado\n\n"
                "Com o M211 S0 nao ha protecao contra colisao em nenhum eixo.\n"
                "O bico deve estar FRIO e SEM filamento.\n\n"
                "Continuar?"):
            return
        self.zoff_travel = 0.0
        self.zoff_armed = True
        self.log("[ZOFFSET] preparando: M851 Z0 / G28 / M420 S0 / M211 S0 / G1 Z0", "sys")
        for c in ("M851 Z0", "M500", "G28", "M420 S0", "M211 S0", "G1 Z0 F300", "M114"):
            self.send(c)
        self.after(800, self._zoff_state)
        self._zoff_state()

    def zoff_step(self, delta):
        """Um degrau de Z em modo relativo, acumulando o total percorrido."""
        if not self.zoff_armed:
            self.log("[ZOFFSET] use 'Preparar' primeiro: sem o M211 S0 o firmware "
                     "recusa descer abaixo de Z=0 e o degrau e perdido.", "err")
            messagebox.showwarning(
                "Nao preparado",
                "Clique em 'Preparar' antes de descer.\n\n"
                "Sem o M211 S0 o Marlin bloqueia qualquer Z negativo, e o botao\n"
                "nao teria efeito nenhum.")
            return
        novo = self.zoff_travel + delta
        if novo < self.ZOFF_LIMIT:
            self.log(f"[ZOFFSET] recusado: {novo:+.3f} mm passa do limite "
                     f"de seguranca ({self.ZOFF_LIMIT:+.1f} mm).", "err")
            return
        self.zoff_travel = novo
        self.log(f"[ZOFFSET] degrau {delta:+.2f} -> acumulado {novo:+.3f} mm", "sys")
        self.send("G91")
        self.send(f"G1 Z{delta:.3f} F120")
        self.send("G90")
        self.send("M114")
        self.after(700, self._zoff_state)
        self._zoff_state()

    def zoff_check(self):
        self.send("M114")
        self.after(700, self._zoff_state)

    def zoff_save(self):
        """Converte a descida acumulada em M851 Z e religa a protecao."""
        if not self.zoff_armed:
            messagebox.showwarning("Nao preparado",
                                   "Nao ha medicao em andamento para gravar.")
            return
        if self.zoff_travel >= 0.0:
            messagebox.showwarning(
                "Nada medido",
                "A descida acumulada e zero ou positiva.\n\n"
                "O offset do probe e a distancia que o bico ainda tinha que\n"
                "descer a partir do zero -- precisa ser um valor negativo.")
            return
        novo = round(self.zoff_travel, 3)
        if not messagebox.askokcancel(
                "Gravar offset Z",
                f"Descida acumulada:  {self.zoff_travel:+.3f} mm\n"
                f"Z lido do firmware: {self.pos['Z']:+.3f} mm\n\n"
                f"Os dois devem bater. Se divergirem, algum degrau foi recusado\n"
                f"pelo firmware -- confira o log antes de gravar.\n\n"
                f"Sera enviado:\n"
                f"  M851 Z{novo:.3f}\n"
                f"  M211 S1   (religa os limites por software)\n"
                f"  M500      (grava na EEPROM)\n\n"
                f"Continuar?"):
            return
        self.log(f"[ZOFFSET] gravando M851 Z{novo:.3f}  "
                 f"(acumulado {self.zoff_travel:+.3f}, M114 Z {self.pos['Z']:+.3f})", "sys")
        for c in (f"M851 Z{novo:.3f}", "M211 S1", "M500", "M851"):
            self.send(c)
        self.zoff_armed = False
        self.zoff_travel = 0.0
        self._zoff_state()
        messagebox.showinfo(
            "Offset gravado",
            f"M851 Z{novo:.3f} gravado e limites religados.\n\n"
            "Para conferir: G28 e depois G1 Z0 -- a folha deve arrastar\n"
            "com o mesmo atrito que voce usou na medicao.")

    def zoff_abort(self):
        """Saida de emergencia: religa a trava e afasta o bico da mesa."""
        self.log("[ZOFFSET] abortado: M211 S1 e subindo 10 mm", "sys")
        self.send("M211 S1")
        self.send("G91")
        self.send("G1 Z10 F300")
        self.send("G90")
        self.zoff_armed = False
        self.zoff_travel = 0.0
        self._zoff_state()

    def mesh_center(self):
        """G29 restrito a um quadrado central (parametro H do G29)."""
        n = int(self.var_mesh.get())
        if not messagebox.askokcancel(
                "Nivelar area central",
                f"Sondar um quadrado de {n}x{n} mm no centro da mesa?\n\n"
                "Requer homing completo. O G29 H restringe a area sondada,\n"
                "mantendo o desvio dentro do alcance do probe."):
            return
        self.log(f"[MALHA] G29 H{n} - area central de {n}x{n} mm", "sys")
        self.send(f"G29 H{n}")
        self.send("M420 V")

    def mesh_full(self):
        """G29 sem restricao. Falha se a inclinacao exceder o alcance do probe."""
        if not messagebox.askokcancel(
                "Nivelar mesa inteira",
                "Sondar a MESA INTEIRA?\n\n"
                "Alcance de busca atual: 2.3mm (D = 2.82mm).\n"
                "Inclinacao medida da mesa: 0.59mm. Cabe com folga.\n\n"
                "Em X a sondagem para em 167mm: o probe fica 38mm a\n"
                "esquerda do bico e nao alcanca a borda direita."):
            return
        self.log("[MALHA] G29 sem restricao (mesa inteira)", "sys")
        self.send("G29")
        self.send("M420 V")

    # -- ventoinhas ---------------------------------------------------------
    def fan_solo(self, idx):
        """Liga so uma ventoinha, para identificar qual e qual."""
        rotulo = "FAN1 da placa" if idx == 0 else "FAN2 da placa"
        other = 1 - idx
        self.send(f"M107 P{other}")
        self.send(f"M106 P{idx} S255")
        self.log(f"Ligada apenas a fan {idx} do Marlin = {rotulo}. "
                 f"Veja qual ventoinha girou.", "sys")

    def fans_off(self):
        self.send("M107 P0")
        self.send("M107 P1")

    def fan_level(self, value):
        """Fixa a ventoinha de peca num PWM, para achar o ponto de travamento.

        Mostra tambem o tempo desligado por ciclo, que e o que derruba o driver
        interno de uma 5015. Base: expansor I2S a 1kHz = 250 ticks de 4us.
        """
        pct = round(value * 100 / 255)
        duty = round(value * 250 / 255)
        off_us = (250 - duty) * 4
        extra = "DC puro, sem PWM" if duty >= 250 else f"{off_us}us desligado por ciclo"
        self.send(f"M106 P0 S{value}")
        self.log(f"Ventoinha em S{value} (~{pct}%) - {extra}. Ainda gira firme?", "sys")

    # -- servo do probe -----------------------------------------------------
    def servo_send(self):
        ang = int(self.var_ang.get())
        self.scl_ang.set(ang)
        self.send(f"M280 P0 S{ang}")

    def servo_step(self, delta):
        ang = max(0, min(180, int(self.var_ang.get()) + delta))
        self.var_ang.set(ang)
        self.servo_send()

    def _servo_slide(self, value):
        """Acompanha o slider sem enviar; o envio ocorre ao soltar o mouse."""
        self.var_ang.set(int(float(value)))

    def _servo_adopt(self, which):
        """Registra o angulo encontrado para levar ao firmware."""
        ang = int(self.var_ang.get())
        rotulo = "ATIVO (deploy)" if which == "deploy" else "RECOLHIDO (stow)"
        self.log(f"Angulo {ang}° anotado como {rotulo}. "
                 f"Leve para Z_SERVO_ANGLES no Configuration.h.", "ok")
        if which == "deploy":
            self._ang_deploy = ang
        else:
            self._ang_stow = ang
        d = getattr(self, "_ang_deploy", SERVO_DEPLOY_ANGLE)
        s = getattr(self, "_ang_stow", SERVO_STOW_ANGLE)
        self.lbl_ang.config(text=f"Encontrados:  Z_SERVO_ANGLES {{ {d}, {s} }}")

    def speed_test(self, axes):
        """Vai e volta entre dois pontos N vezes, para achar onde perde passos.

        Termina na mesma coordenada em que comecou: se o carro nao voltar para
        a marca fisica, a maquina perdeu passos naquela velocidade.
        """
        spd, cyc = int(self.var_spd.get()), int(self.var_cyc.get())
        lo, hi = 50, 150                      # dentro da mesa de 200x200
        if not messagebox.askokcancel(
                "Teste de velocidade",
                f"Mover {axes} entre {lo} e {hi} mm, {cyc} vezes, a {spd} mm/s.\n\n"
                "A maquina PRECISA estar referenciada (G28) antes.\n"
                f"O carro vai primeiro para {lo} mm: marque a posicao\n"
                "ANTES de rodar os ciclos, e confira a marca no fim.\n\n"
                "Serao enviados comandos um a um, esperando o 'ok' de cada."):
            return
        feed = spd * 60
        start = " ".join(f"{a}{lo}" for a in axes)
        end = " ".join(f"{a}{hi}" for a in axes)
        total = 2 * cyc
        self.log(f"[VELOC] Inicio: {axes} {lo}<->{hi} mm, {cyc} ciclos "
                 f"({total} movimentos) a {spd} mm/s (F{feed}).", "sys")
        self.send("G90")                      # absoluto
        self.send("M400")                     # esvazia o que estiver pendente
        self.send(f"G1 {start} F{feed}")
        self.send("M400")
        self.send("M114")                     # posicao de partida, para o log
        for _ in range(cyc):
            self.send(f"G1 {end} F{feed}")
            self.send(f"G1 {start} F{feed}")
        self.send("M400")
        self.send("M114")                     # deve ser identica a de partida
        self.log(f"[VELOC] Fim a {spd} mm/s. Os dois M114 devem ser iguais "
                 f"({lo} mm). Se forem e o carro estiver FORA da marca, "
                 f"houve perda de passos.", "sys")

    def core_single(self, motor):
        """Aciona um unico motor da CoreXY. A = X+Y, B = X-Y."""
        d = self.jog_step.get()
        sign = 1 if motor == "A" else -1
        self.log(f"Diagnostico CoreXY: acionando SO o motor {motor}. "
                 f"Apenas um motor deve girar.", "sys")
        self.send("G91")
        self.send(f"G1 X{d:g} Y{d * sign:g} F{self.jog_feed.get()}")
        self.send("G90")
        self.send("M114")

    def extrude(self, mm):
        if self.temps["h"] < 170:
            if not messagebox.askokcancel(
                    "Bico frio",
                    f"O bico esta a {self.temps['h']:.0f}°C.\n"
                    "O Marlin bloqueia extrusao abaixo de 170°C "
                    "(PREVENT_COLD_EXTRUSION).\n\nTentar mesmo assim?"):
                return
        self.send("G91")
        self.send(f"G1 E{mm:g} F120")
        self.send("G90")

    def apply_steps(self):
        v = self.var_steps
        self.send(f"M92 X{v['X'].get():g} Y{v['Y'].get():g} "
                  f"Z{v['Z'].get():g} E{v['E'].get():g}")

    def all_heaters_off(self):
        for c in ("M104 S0", "M140 S0", "M107"):
            self.send(c)

    def emergency_stop(self):
        self.log(">> M112 (PARADA DE EMERGENCIA) - fila descartada", "err")
        self.send_now("M112")
        self.log("A placa precisa ser reiniciada (M999 ou reset) apos o M112.", "sys")

    # -- loop de recepção --------------------------------------------------
    def _drain(self):
        try:
            while True:
                line = self.rx.get_nowait()
                if line.startswith("\x00SYS"):
                    self.log(line[4:], "sys")
                elif line.startswith("\x00FOUND"):
                    self._handle_found()
                elif line.startswith("\x00PIODONE"):
                    self._pio_done(line[8:])
                else:
                    self._handle_line(line)
        except queue.Empty:
            pass

        # Destrava se o 'ok' nunca chegar (placa reiniciada, cabo solto...)
        if self.awaiting_ok and self.last_rx and \
           time.time() - self.last_rx > OK_TIMEOUT:
            self.log(f"Sem resposta da placa ha {OK_TIMEOUT}s. "
                     f"Destravando a fila ({len(self.txq)} pendentes).", "err")
            self.awaiting_ok = False
            self._pump()

        if self.monitoring.get() and self.link.connected and not self.txq:
            now = time.time()
            if now - self.last_endstop_poll > ENDSTOP_POLL_INTERVAL:
                self.last_endstop_poll = now
                self.send("M119")

        self.after(60, self._drain)

    def _handle_found(self):
        self.btn_detect.config(state="normal")
        if not self._found:
            self.log("Nenhuma placa com Marlin encontrada.", "err")
            return
        port, baud, banner = self._found
        self.log(f"Encontrada: {port} @ {baud}", "ok")
        self.log(banner, "rx")
        self.cmb_port.set(port)
        self.cmb_baud.set(str(baud))
        if not self.link.connected:
            self.toggle_conn()

    def _handle_line(self, line):
        self.last_rx = time.time()
        if not line:
            return

        low = line.lower()

        # Controle de fluxo: 'ok' libera o proximo comando da fila.
        if low.startswith("ok") or low.startswith("error"):
            self.awaiting_ok = False
            self.after(0, self._pump)

        tag = "rx"
        if low.startswith("error") or "fail" in low or "kill" in low:
            tag = "err"
        elif low.startswith("ok"):
            tag = "ok"
        elif line.startswith("echo:"):
            tag = "sys"

        # temperaturas: nao poluir o log com auto-report
        m = RE_TEMP.search(line)
        if m:
            self.temps["h"] = float(m.group(1))
            self.temps["ht"] = float(m.group(2))
            if m.group(3) is not None:
                self.temps["b"] = float(m.group(3))
                self.temps["bt"] = float(m.group(4))
            self._refresh_temps()
            if line.startswith(" T:") or line.startswith("T:"):
                # Auto-report: atualiza o card e some da tela, mas fica no log.
                self._to_file(line, "temp")
                return

        m = RE_POS.search(line)
        if m:
            for i, ax in enumerate("XYZE"):
                self.pos[ax] = float(m.group(i + 1))
            self.lbl_pos.config(
                text="Posição:  X {X:<8.2f} Y {Y:<8.2f} Z {Z:<8.2f} E {E:<8.2f}".format(**self.pos))

        m = RE_ENDSTOP.match(line)
        if m:
            self.endstops[m.group(1).lower()] = m.group(2).upper() == "TRIGGERED"
            self._refresh_endstops()

        if "FIRMWARE_NAME" in line:
            self.fw_banner = line.strip()[:160]

        m = RE_STEPS.search(line)
        if m:
            for i, ax in enumerate("XYZE"):
                self.var_steps[ax].set(float(m.group(i + 1)))

        self.log(line, tag)

    def _refresh_temps(self):
        t = self.temps

        def color(cur, tgt):
            if tgt <= 0:
                return BG_IDLE if cur < 50 else BG_WARN
            return BG_OK if abs(cur - tgt) <= 3 else BG_WARN

        self.lbl_hot.config(text=f"Bico  {t['h']:.1f} / {t['ht']:.0f}",
                            bg=color(t["h"], t["ht"]))
        self.lbl_bed.config(text=f"Mesa  {t['b']:.1f} / {t['bt']:.0f}",
                            bg=color(t["b"], t["bt"]))

    def _refresh_endstops(self):
        for w in self.frm_es.winfo_children():
            w.destroy()
        nice = {"x_min": "X", "y_min": "Y", "z_min": "Z",
                "x_max": "X max", "y_max": "Y max", "z_max": "Z max",
                "z_probe": "Probe", "filament": "Filamento"}
        for i, (name, trig) in enumerate(sorted(self.endstops.items())):
            tk.Label(self.frm_es, text=nice.get(name, name),
                     bg=BG_ERR if trig else BG_OK, fg="white",
                     width=11, pady=4).grid(row=i // 4, column=i % 4, padx=2, pady=2)

    def _on_close(self):
        try:
            if self.link.connected:
                self.link.send("M155 S0")
                time.sleep(0.1)
            self.link.disconnect()
        finally:
            if self.logfile:
                try:
                    self.logfile.write(f"# sessao encerrada {time.ctime()}\n")
                    self.logfile.close()
                except (OSError, ValueError):
                    pass
            self.destroy()


if __name__ == "__main__":
    App().mainloop()
