"""Converte o bundle do PrusaSlicer (fonte de verdade) em presets do OrcaSlicer.

Cada preset sai com TODOS os valores (a cadeia de heranca do Orca e resolvida
aqui) e herda do perfil de sistema correspondente, como os presets que o
proprio Orca grava. A impressora generica "Generic Marlin Printer" precisa
estar ativada no Orca antes da importacao.
"""
import io, os, glob, json, sys

# Uso:  python gerar_presets_orca.py
# Rode de novo sempre que o bundle do PrusaSlicer mudar: ele e a fonte de verdade.

ORCA = r"C:\Program Files\OrcaSlicer\resources\profiles"
BUNDLE = r"C:\tmp\Marlin\slicer\PrusaSlicer_CoreXY_TinyBee_bundle.ini"
SAIDA = r"C:\tmp\Marlin\slicer\orca"
BS = chr(92)
# O Orca recusa preset de usuario sem "version", e recusa em silencio: a
# janela diz "0 configuracoes importadas" e o log nao registra motivo. Os
# presets de sistema nao tem essa chave (ela fica no arquivo do fabricante).
VERSAO = "2.4.2.0"

# ---------------------------------------------------------------- Orca: indice
por_nome = {}                                   # (pasta_vendor, nome) e nome -> dict
for p in glob.glob(os.path.join(ORCA, "**", "*.json"), recursive=True):
    try:
        d = json.load(io.open(p, encoding="utf-8"))
    except Exception:
        continue
    if "name" not in d or "type" not in d:
        continue
    vendor = os.path.relpath(p, ORCA).split(os.sep)[0]
    por_nome[(vendor, d["name"])] = d
    por_nome.setdefault(d["name"], d)

def achata(nome, vendor):
    d = por_nome.get((vendor, nome)) or por_nome[nome]
    pai = d.get("inherits", "")
    base = achata(pai, vendor) if pai else {}
    out = dict(base)
    out.update(d)
    return out

universo = {"machine": {}, "process": {}, "filament": {}}
for d in list(por_nome.values()):
    if d.get("type") in universo:
        for k, v in d.items():
            universo[d["type"]].setdefault(k, v)

# O Orca 2.5 guarda os perfis em arquivos .opc (formato proprio), sem JSONs
# soltos. Nesse caso a base de cada preset passa a ser o arquivo ja gerado
# antes: ele ja e completo (valores do Orca + os do bundle), e aplicar os valores
# do bundle por cima de novo e idempotente. Os que o bundle mudou entram; o
# restante fica como o Orca 2.4.2 tinha.
def previo(arquivo):
    p = os.path.join(SAIDA, arquivo)
    if not os.path.exists(p):
        sys.exit("sem JSONs do Orca instalado e sem " + arquivo + " para usar de base")
    return json.load(io.open(p, encoding="utf-8"))

if len(universo["machine"]) < 50:
    print("AVISO: Orca sem JSONs soltos (2.5+): usando os arquivos ja gerados como base")
    for arq, tipo in (("CoreXY MKS TinyBee.json", "machine"),
                      ("CoreXY TinyBee 0.20mm.json", "process"),
                      ("PLA Speed Plus GTMax3D.json", "filament"),
                      ("PETG CoreXY TinyBee.json", "filament"),
                      ("PLA CoreXY TinyBee.json", "filament"),
                      ("ABS CoreXY TinyBee.json", "filament")):
        for k, v in previo(arq).items():
            universo[tipo].setdefault(k, v)
    SEM_INSTALACAO = True
else:
    SEM_INSTALACAO = False

def base_de(nome_orca, vendor, arquivo):
    return previo(arquivo) if SEM_INSTALACAO else achata(nome_orca, vendor)

# ---------------------------------------------------------------- Prusa: leitura
sec, cur = {}, None
for ln in io.open(BUNDLE, encoding="utf-8"):
    ln = ln.rstrip("\n")
    if ln.startswith("["):
        cur = ln.strip("[]"); sec[cur] = {}
    elif cur and "=" in ln and not ln.lstrip().startswith("#"):
        k, v = ln.split("=", 1); sec[cur][k.strip()] = v.strip()

def gcode(v):
    v = v.strip()
    if len(v) >= 2 and v[0] == '"' and v[-1] == '"':
        v = v[1:-1]
    return v.replace(BS + "n", "\n")

# ---------------------------------------------------------------- utilidades
def ajusta_tipo(base, chave, valor):
    """Copia o formato do Orca: lista quando o Orca usa lista, com o mesmo tamanho."""
    ref = base.get(chave, universo.get(base["type"], {}).get(chave))
    if isinstance(ref, list):
        if isinstance(valor, list):
            return [str(x) for x in valor]
        return [str(valor)] * max(1, len(ref))
    return str(valor)

problemas = []
def aplica(base, sobrescritas, tipo):
    out = dict(base)
    for k, v in sobrescritas.items():
        if k not in universo[tipo] and k not in base:
            problemas.append(f"{tipo}: chave desconhecida no Orca: {k}")
            continue
        out[k] = ajusta_tipo(out, k, v)
    return out

def finaliza(d, nome, tipo, id_chave, pai):
    for k in ("setting_id", "base_id", "printer_model", "printer_variant",
              "default_filament_profile", "default_print_profile", "is_custom_defined"):
        d.pop(k, None)
    d.update({"type": tipo, "name": nome, "inherits": pai, "from": "User",
              "instantiation": "true", "version": VERSAO, "is_custom_defined": "0"})
    if id_chave == "filament_settings_id":
        d[id_chave] = [nome]
    else:
        d[id_chave] = nome
    if tipo != "machine":
        d["compatible_printers"] = []
        d["compatible_printers_condition"] = ""
    return d

def grava(d, arquivo):
    os.makedirs(SAIDA, exist_ok=True)
    json.dump(d, io.open(os.path.join(SAIDA, arquivo), "w", encoding="utf-8"),
              ensure_ascii=False, indent=4, sort_keys=True)

# ---------------------------------------------------------------- impressora
P = sec["printer:CoreXY MKS TinyBee"]
lim = lambda k: P[k].split(",")[0]
# A faixa da frente que o bundle reserva para a purga (bed_shape comeca em Y > 0)
# vira, no Orca, uma ZONA PROIBIDA dentro da mesa, e nao um recorte da mesa: assim a
# purga aparece dentro da area desenhada e nenhuma peca pode ser colocada sobre ela.
# O PrusaSlicer nao tem esse recurso, e la a reserva continua sendo a forma da mesa.
pts = [tuple(float(v) for v in p.split("x")) for p in P["bed_shape"].split(",")]
xmin = min(p[0] for p in pts); xmax = max(p[0] for p in pts)
ymin = min(p[1] for p in pts); ymax = max(p[1] for p in pts)
fmt = lambda v: f"{v:g}"
AREA = [f"{fmt(xmin)}x0", f"{fmt(xmax)}x0", f"{fmt(xmax)}x{fmt(ymax)}", f"{fmt(xmin)}x{fmt(ymax)}"]
EXCL = (["0x0"] if ymin <= 0 else
        [f"{fmt(xmin)}x0", f"{fmt(xmax)}x0", f"{fmt(xmax)}x{fmt(ymin)}", f"{fmt(xmin)}x{fmt(ymin)}"])
# bed_exclude_area nao existia no indice do Orca 2.4.2 usado para montar a base, mas
# o Orca 2.5 grava a chave no rodape do G-code ("bed_exclude_area = 0x0").
universo["machine"].setdefault("bed_exclude_area", ["0x0"])

maquina = aplica(base_de("MyMarlin 0.4 nozzle", "Custom", "CoreXY MKS TinyBee.json"), {
    "printable_area": AREA,
    "bed_exclude_area": EXCL,
    "printable_height": P["max_print_height"],
    "nozzle_diameter": P["nozzle_diameter"],
    "gcode_flavor": "marlin2",
    "use_relative_e_distances": P["use_relative_e_distances"],
    "printer_structure": "corexy",
    "disable_m73": "0",                      # a TFT mostra progresso pelo M73
    "retraction_length": P["retract_length"],
    "retraction_speed": P["retract_speed"],
    "deretraction_speed": P["deretract_speed"],
    "retraction_minimum_travel": P["retract_before_travel"],
    "retract_when_changing_layer": P["retract_layer_change"],
    "z_hop": P["retract_lift"],
    "retract_lift_below": P["retract_lift_below"],
    "retract_restart_extra": P["retract_restart_extra"],
    "wipe": P["wipe"],
    "machine_max_speed_x": lim("machine_max_feedrate_x"),
    "machine_max_speed_y": lim("machine_max_feedrate_y"),
    "machine_max_speed_z": lim("machine_max_feedrate_z"),
    "machine_max_speed_e": lim("machine_max_feedrate_e"),
    "machine_max_acceleration_x": lim("machine_max_acceleration_x"),
    "machine_max_acceleration_y": lim("machine_max_acceleration_y"),
    "machine_max_acceleration_z": lim("machine_max_acceleration_z"),
    "machine_max_acceleration_e": lim("machine_max_acceleration_e"),
    "machine_max_acceleration_extruding": lim("machine_max_acceleration_extruding"),
    "machine_max_acceleration_retracting": lim("machine_max_acceleration_retracting"),
    "machine_max_acceleration_travel": lim("machine_max_acceleration_travel"),
    "machine_max_jerk_x": lim("machine_max_jerk_x"),
    "machine_max_jerk_y": lim("machine_max_jerk_y"),
    "machine_max_jerk_z": lim("machine_max_jerk_z"),
    "machine_max_jerk_e": lim("machine_max_jerk_e"),
    "machine_max_junction_deviation": lim("machine_max_junction_deviation"),
    "machine_start_gcode": gcode(P["start_gcode"]),
    "machine_end_gcode": gcode(P["end_gcode"]),
    "layer_change_gcode": gcode(P["layer_gcode"]),
    "before_layer_change_gcode": "",
    "machine_pause_gcode": P["pause_print_gcode"],
    "printer_notes": gcode(P["printer_notes"]),
}, "machine")
grava(finaliza(maquina, "CoreXY MKS TinyBee", "machine", "printer_settings_id",
               "MyMarlin 0.4 nozzle"),
      "CoreXY MKS TinyBee.json")

# Variante com estacionamento a cada camada para o time-lapse: igual, so muda o
# G-code de troca de camada (secao propria no bundle).
PT = sec.get("printer:CoreXY MKS TinyBee (time-lapse)")
if PT:
    tl = dict(maquina)
    tl["layer_change_gcode"] = gcode(PT["layer_gcode"])
    tl["printer_notes"] = gcode(PT["printer_notes"])
    grava(finaliza(tl, "CoreXY MKS TinyBee (time-lapse)", "machine", "printer_settings_id",
                   "MyMarlin 0.4 nozzle"),
          "CoreXY MKS TinyBee (time-lapse).json")

# ---------------------------------------------------------------- processo
S = sec["print:CoreXY TinyBee 0.20mm"]
proc = aplica(base_de("0.20mm Standard @MyMarlin", "Custom", "CoreXY TinyBee 0.20mm.json"), {
    "layer_height": S["layer_height"],
    "initial_layer_print_height": S["first_layer_height"],
    "wall_loops": S["perimeters"],
    "top_shell_layers": S["top_solid_layers"],
    "bottom_shell_layers": S["bottom_solid_layers"],
    "sparse_infill_density": S["fill_density"],
    "sparse_infill_pattern": S["fill_pattern"],
    "top_surface_pattern": S["top_fill_pattern"],
    "bottom_surface_pattern": S["bottom_fill_pattern"],
    "wall_generator": "arachne",             # o padrao do PrusaSlicer, usado no benchy
    "outer_wall_speed": S["external_perimeter_speed"],
    "inner_wall_speed": S["perimeter_speed"],
    "small_perimeter_speed": S["small_perimeter_speed"],
    "sparse_infill_speed": S["infill_speed"],
    "internal_solid_infill_speed": S["solid_infill_speed"],
    "top_surface_speed": S["top_solid_infill_speed"],
    "support_speed": S["support_material_speed"],
    "bridge_speed": S["bridge_speed"],
    "gap_infill_speed": S["gap_fill_speed"],
    "travel_speed": S["travel_speed"],
    "travel_speed_z": S["travel_speed_z"],
    "initial_layer_speed": S["first_layer_speed"],
    "initial_layer_infill_speed": S["first_layer_speed"],
    # o skirt so existe na primeira camada; o padrao do Orca e 50 mm/s, o Prusa
    # usa a velocidade da primeira camada (20)
    "skirt_speed": S["first_layer_speed"],
    "default_acceleration": S["default_acceleration"],
    "outer_wall_acceleration": S["external_perimeter_acceleration"],
    "inner_wall_acceleration": S["perimeter_acceleration"],
    "sparse_infill_acceleration": S["infill_acceleration"],
    "internal_solid_infill_acceleration": S["solid_infill_acceleration"],
    "top_surface_acceleration": S["top_solid_infill_acceleration"],
    "initial_layer_acceleration": S["first_layer_acceleration"],
    "travel_acceleration": S["travel_acceleration"],
    "bridge_acceleration": S["bridge_acceleration"],
    # Prusa: overhang_speed_N e a velocidade para N*25% de APOIO; Orca: overhang_N_4
    # e a velocidade para N/4 da linha NO AR. Por isso a ordem se inverte.
    "enable_overhang_speed": S["enable_dynamic_overhang_speeds"],
    "overhang_1_4_speed": S["overhang_speed_3"],
    "overhang_2_4_speed": S["overhang_speed_2"],
    "overhang_3_4_speed": S["overhang_speed_1"],
    "overhang_4_4_speed": S["overhang_speed_0"],
    "line_width": S["extrusion_width"],
    "initial_layer_line_width": S["first_layer_extrusion_width"],
    "inner_wall_line_width": S["perimeter_extrusion_width"],
    "outer_wall_line_width": S["external_perimeter_extrusion_width"],
    "sparse_infill_line_width": S["infill_extrusion_width"],
    "internal_solid_infill_line_width": S["solid_infill_extrusion_width"],
    "top_surface_line_width": S["top_infill_extrusion_width"],
    "support_line_width": S["support_material_extrusion_width"],
    "seam_position": S["seam_position"],
    "elefant_foot_compensation": S["elefant_foot_compensation"],
    "skirt_loops": S["skirts"],
    "skirt_distance": S["skirt_distance"],
    "skirt_height": S["skirt_height"],
    "brim_type": "no_brim",
    "brim_width": S["brim_width"],
    "enable_support": S["support_material"],
    "reduce_crossing_wall": S["avoid_crossing_perimeters"],
    "reduce_infill_retraction": S["only_retract_when_crossing_perimeters"],
}, "process")
grava(finaliza(proc, "CoreXY TinyBee 0.20mm", "process", "print_settings_id",
               "0.20mm Standard @MyMarlin"),
      "CoreXY TinyBee 0.20mm.json")

# ---------------------------------------------------------------- filamentos
PLACAS = [k for k in ("hot_plate_temp", "cool_plate_temp", "eng_plate_temp",
                      "textured_plate_temp", "supertack_plate_temp",
                      "textured_cool_plate_temp") if k in universo["filament"]]

def filamento(secao, nome, base, extra=None):
    F = sec[secao]
    s = {
        "filament_type": F["filament_type"],
        "filament_diameter": F["filament_diameter"],
        "filament_density": F["filament_density"],
        "filament_cost": F["filament_cost"],
        "filament_flow_ratio": F.get("extrusion_multiplier", "1"),
        "nozzle_temperature": F["temperature"],
        "nozzle_temperature_initial_layer": F["first_layer_temperature"],
        "filament_max_volumetric_speed": F["filament_max_volumetric_speed"],
        "slow_down_for_layer_cooling": F["cooling"],
        "fan_min_speed": F["min_fan_speed"],
        "fan_max_speed": F["max_fan_speed"],
        "overhang_fan_speed": F["bridge_fan_speed"],
        "close_fan_the_first_x_layers": F["disable_fan_first_layers"],
        "full_fan_speed_layer": F["full_fan_speed_layer"],
        "fan_cooling_layer_time": F["fan_below_layer_time"],
        "slow_down_layer_time": F["slowdown_below_layer_time"],
        "slow_down_min_speed": F["min_print_speed"],
        "enable_pressure_advance": "0",
        "filament_start_gcode": gcode(F["start_filament_gcode"]),
        "filament_end_gcode": gcode(F["end_filament_gcode"]),
    }
    for k in PLACAS:                                # mesma temperatura em todo tipo de placa
        s[k] = F["bed_temperature"]
        if k + "_initial_layer" in universo["filament"]:
            s[k + "_initial_layer"] = F["first_layer_bed_temperature"]
    s.update(extra or {})
    d = aplica(base_de(base, "OrcaFilamentLibrary", nome + ".json"), s, "filament")
    grava(finaliza(d, nome, "filament", "filament_settings_id", base), nome + ".json")

filamento("filament:PLA Speed Plus GTMax3D", "PLA Speed Plus GTMax3D", "Generic PLA @System",
          {"nozzle_temperature_range_low": "190", "nozzle_temperature_range_high": "220",
           "filament_vendor": "GTMax3D"})
filamento("filament:PLA CoreXY TinyBee", "PLA CoreXY TinyBee", "Generic PLA @System")
filamento("filament:PETG CoreXY TinyBee", "PETG CoreXY TinyBee", "Generic PETG @System")
filamento("filament:ABS CoreXY TinyBee", "ABS CoreXY TinyBee", "Generic ABS @System")

print("\n".join(problemas) if problemas else "todas as chaves existem no Orca 2.4")
for f in sorted(os.listdir(SAIDA)):
    print("  ", f)
