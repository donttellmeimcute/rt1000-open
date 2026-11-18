#!/usr/bin/env python3
"""
rt1000_patch.py — Hacer modificable el firmware V1.16 del RT-1000Pro / UV100.

Idea: reescribir el firmware desde cero es un proyecto de meses y
necesita el hardware (MCU exacto y transceptor sin confirmar).
Lo que SÍ se puede hacer hoy, con seguridad y verificable, es
parchear el firmware oficial en los datos que son modificables
sin tocar código: textos, versión/fecha y tablas de frecuencias
candidatas. Este script hace eso y regenera un Intel HEX válido
que se flashea con el updater open source (`../updater/`).

NO incluye firmware propietario: trabaja sobre el .hex que aporta
el usuario (el extraído del actualizador oficial, para uso personal).

Qué se puede modificar, verificado:
  - version: campo "VER :V1.16      " en 0x0800D234, 16 bytes.
    El valor tras "VER :" admite hasta 11 caracteres.
  - date: campo "DATE:2026-06-11 " en 0x0800D24C, 16 bytes.
    El valor tras "DATE:" admite hasta 11 caracteres.
  - strings: cualquier cadena ASCII del firmware, reemplazada por
    otra DE LA MISMA LONGITUD (o más corta, rellenando con espacios).
    Nunca más larga: sobrescribiría el byte NUL o el dato siguiente.
    El script busca la cadena por texto exacto o por dirección flash.
  - bands (SIN VERIFICAR, solo con --force): tablas de valores
    uint32 LE en unidades de 10 Hz, en:
      0x08007984, 0x08013A70, 0x08024DF4, 0x08008170
    La primera empieza 136, 174, 400, 480 MHz (límites VHF/UHF),
    seguida de otros valores redondos en MHz. No se ha confirmado
    qué función usa cada tabla ni si cambiarla cambia los límites
    de TX/RX, de escaneo o de calibración. Cambiar frecuencias
    puede dejar el radio fuera de banda, dañar el frontal o
    transmitir donde tu licencia no lo permite. Úsalo solo para
    investigar, con backup, y nunca para transmitir fuera de
    las bandas de radioaficionado autorizadas en México.

Qué NO se puede modificar con este script (a propósito):
  - La vector table (SP / Reset) ni el rango del bootloader.
  - Código ARM: no hay desensamblado a C, y parchear instrucciones
    a ciegas brickea el radio.
  - El callsign APRS: no está en el firmware, está en la
    configuración del radio (CPS / memoria). No lo busques aquí.

Seguridad:
  - Siempre se guarda backup del .hex original (no se sobrescribe
    sin --output).
  - El HEX generado se re-parsea con el mismo parser del updater
    y se comprueba SP/Reset, tamaño 251904 y checksums Intel HEX.
  - Para flashear, usa `../updater/rt1000_update.py --dry-run`
    primero, y luego el flasheo normal. Si algo va mal, reflashea
    el HEX original: el bootloader de fábrica no se toca.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "updater"))
import rt1000_update as upd  # noqa: E402

FLASH_BASE = upd.FLASH_BASE
IMAGE_SIZE = upd.IMAGE_SIZE

VER_ADDR = 0x0800D234   # "VER :V1.16      " (16 bytes, NUL después)
DATE_ADDR = 0x0800D24C  # "DATE:2026-06-11 " (16 bytes, NUL después)
FIELD_LEN = 16

# Tablas candidatas de frecuencias (uint32 LE, unidades de 10 Hz).
# Valores leídos del firmware V1.16; SIN VERIFICAR su función.
BAND_TABLES = {
    0x08007984: [136, 174, 400, 480, 18, 64, 45, 240, 320, 560, 660, 840, 920, 1000, 1080, 1160, 1240, 1300],
    0x08013A70: [136, 174, 135, 5, 400, 480, 18, 64, 15, 60, 240, 170, 320, 260, 560, 660, 840, 920],
    0x08024DF4: [64, 18, 136, 174, 240, 320, 400, 480, 560, 620, 920, 840, 1000],
    0x08008170: [108, 136],
}


def flash_to_off(addr: int) -> int:
    off = addr - FLASH_BASE
    if not 0 <= off < IMAGE_SIZE:
        raise ValueError(f"dirección {addr:#x} fuera de la imagen")
    return off


def load_image(path: str) -> bytearray:
    text = Path(path).read_text(encoding="utf-8")
    return upd.parse_intel_hex(text)


def find_string(image: bytes, text: str) -> list[int]:
    """Direcciones flash donde aparece `text` tal cual (bytes ASCII)."""
    needle = text.encode("ascii")
    out, start = [], 0
    while True:
        i = image.find(needle, start)
        if i < 0:
            return out
        out.append(FLASH_BASE + i)
        start = i + 1


def list_strings(image: bytes, min_len: int = 4):
    """Todas las cadenas ASCII imprimibles, con dirección y longitud."""
    for m in re.finditer(rb"[ -~]{%d,}" % min_len, bytes(image)):
        yield FLASH_BASE + m.start(), m.group().decode("ascii")


def patch_string_at(image: bytearray, addr: int, new: str, field_len: int | None = None) -> None:
    off = flash_to_off(addr)
    raw = new.encode("ascii")
    if field_len is None:
        # El campo termina en el primer NUL a partir de `off`.
        end = image.find(b"\x00", off)
        if end < 0:
            raise ValueError(f"sin NUL tras {addr:#x}")
        field_len = end - off
    if len(raw) > field_len:
        raise ValueError(
            f"'{new}' ({len(raw)}) no cabe en el campo de {field_len} bytes en {addr:#x}; "
            "usa un texto igual o más corto"
        )
    image[off : off + field_len] = raw + b" " * (field_len - len(raw))


def patch_field(image: bytearray, addr: int, prefix: str, value: str) -> None:
    """Campo de 16 bytes tipo 'VER :<valor>' / 'DATE:<valor>'."""
    if len(prefix) + len(value) > FIELD_LEN:
        raise ValueError(f"valor '{value}' demasiado largo para el campo en {addr:#x}")
    off = flash_to_off(addr)
    current = bytes(image[off : off + FIELD_LEN]).decode("ascii", "replace")
    if not current.startswith(prefix):
        raise ValueError(
            f"campo en {addr:#x} no es el esperado: {current!r} (¿otro firmware?)"
        )
    text = (prefix + value).ljust(FIELD_LEN)
    image[off : off + FIELD_LEN] = text.encode("ascii")


def patch_bands(image: bytearray, addr: int, values_mhz: list[float]) -> None:
    if addr not in BAND_TABLES:
        raise ValueError(
            f"tabla {addr:#x} no está en la lista de candidatas: "
            + ", ".join(f"{a:#x}" for a in BAND_TABLES)
        )
    expected = BAND_TABLES[addr]
    if len(values_mhz) != len(expected):
        raise ValueError(
            f"tabla {addr:#x} tiene {len(expected)} valores, diste {len(values_mhz)}"
        )
    for v in values_mhz:
        # La tabla original ya trae valores hasta 1300, así que el
        # límite es 2000; su significado por encima de 999 MHz no
        # está verificado (ver README).
        if not 1 <= v <= 2000:
            raise ValueError(f"frecuencia fuera de rango 1-2000 MHz: {v}")
    off = flash_to_off(addr)
    for i, mhz in enumerate(values_mhz):
        # Unidades de 10 Hz: MHz * 100000. Debe ser entero.
        raw = int(round(mhz * 100000))
        if raw != mhz * 100000:
            raise ValueError(f"{mhz} MHz no es múltiplo exacto de 10 Hz en esta tabla")
        struct.pack_into("<I", image, off + i * 4, raw)


def write_hex(image: bytes, path: str) -> None:
    """Regenera Intel HEX: ELA 0x0800, registros de 16 bytes, entry y EOF."""
    def hex_record(rec: bytes) -> str:
        return ":" + rec.hex().upper() + f"{(-sum(rec)) & 0xFF:02X}"

    lines = [hex_record(bytes([2, 0, 0, 0x04, 0x08, 0x00]))]
    # Escribimos toda la imagen (251904 bytes) para que el HEX
    # parcheado sea idéntico en tamaño al original.
    base = FLASH_BASE
    for off in range(0, IMAGE_SIZE, 16):
        chunk = bytes(image[off : off + 16])
        full_addr = base + off
        # Cambio de segmento de 64 KiB: emitir ELA (tipo 04) cada vez
        # que la dirección cruza un límite de 64 KiB, como hace el
        # HEX oficial (ELA 0x0800, 0x0801, 0x0802, 0x0803).
        if off > 0 and full_addr % 0x10000 == 0:
            ela = full_addr >> 16
            lines.append(hex_record(bytes([2, 0, 0, 0x04, (ela >> 8) & 0xFF, ela & 0xFF])))
        addr = full_addr & 0xFFFF
        rec = bytes([len(chunk), (addr >> 8) & 0xFF, addr & 0xFF, 0x00]) + chunk
        lines.append(hex_record(rec))
    # Entry point del firmware oficial (registro :05)
    entry = struct.pack(">I", 0x080029E1)
    rec = bytes([4, 0, 0, 0x05]) + entry
    lines.append(":" + rec.hex().upper() + f"{(-sum(rec)) & 0xFF:02X}")
    lines.append(":00000001FF")
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")


def verify(image: bytes, original: bytes) -> dict:
    sp = struct.unpack_from("<I", image, 0)[0]
    reset = struct.unpack_from("<I", image, 4)[0]
    if sp != struct.unpack_from("<I", original, 0)[0] or reset != struct.unpack_from("<I", original, 4)[0]:
        raise ValueError("¡SP o Reset cambiados! El parche tocó la vector table, abortado.")
    changed = sum(1 for a, b in zip(image, original) if a != b)
    return {"sp": sp, "reset": reset, "changed_bytes": changed,
            "sha256": hashlib.sha256(bytes(image)).hexdigest()}


def apply_config(image: bytearray, cfg: dict, force_bands: bool = False) -> list[str]:
    log: list[str] = []
    if "version" in cfg:
        patch_field(image, VER_ADDR, "VER :", cfg["version"])
        log.append(f"version -> {cfg['version']!r} en {VER_ADDR:#x}")
    if "date" in cfg:
        patch_field(image, DATE_ADDR, "DATE:", cfg["date"])
        log.append(f"date -> {cfg['date']!r} en {DATE_ADDR:#x}")
    for item in cfg.get("strings", []):
        new = item["new"]
        if "address" in item:
            addr = int(item["address"], 16) if isinstance(item["address"], str) else item["address"]
            patch_string_at(image, addr, new)
            log.append(f"string en {addr:#x} -> {new!r}")
        elif "old" in item:
            hits = find_string(image, item["old"])
            if not hits:
                raise ValueError(f"cadena no encontrada: {item['old']!r}")
            if len(hits) > 1:
                raise ValueError(f"cadena {item['old']!r} aparece {len(hits)} veces, usa 'address'")
            patch_string_at(image, hits[0], new, field_len=len(item["old"]))
            log.append(f"string {item['old']!r} -> {new!r} en {hits[0]:#x}")
        else:
            raise ValueError(f"entrada de strings sin 'old' ni 'address': {item}")
    for addr_s, values in cfg.get("bands", {}).items():
        if not force_bands:
            raise ValueError(
                "parchear 'bands' requiere --force-bands (tablas sin verificar, ver README)"
            )
        addr = int(addr_s, 16) if isinstance(addr_s, str) else addr_s
        patch_bands(image, addr, [float(v) for v in values])
        log.append(f"bands en {addr:#x} -> {values}")
    return log


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Patcher de firmware RT-1000Pro / UV100 V1.16 (modificable).")
    ap.add_argument("hexfile", help=".hex original del firmware")
    ap.add_argument("--list-strings", action="store_true", help="lista cadenas patchables y sale")
    ap.add_argument("--list-bands", action="store_true", help="muestra las tablas de frecuencias candidatas")
    ap.add_argument("--config", help="JSON con los parches a aplicar")
    ap.add_argument("--output", help=".hex parcheado de salida")
    ap.add_argument("--force-bands", action="store_true", help="permite parchear 'bands' (sin verificar)")
    args = ap.parse_args(argv)

    image = load_image(args.hexfile)
    if args.list_bands:
        for addr, vals in BAND_TABLES.items():
            print(f"{addr:#x}: {vals} MHz  (unidades 10 Hz, SIN VERIFICAR)")
        return 0
    if args.list_strings:
        for addr, s in list_strings(image):
            print(f"{addr:#x} ({len(s):3d}) {s!r}")
        return 0
    if not args.config or not args.output:
        ap.error("--config y --output son obligatorios para parchear")
    cfg = json.loads(Path(args.config).read_text(encoding="utf-8"))
    original = bytes(image)
    log = apply_config(image, cfg, force_bands=args.force_bands)
    info = verify(image, original)
    write_hex(image, args.output)
    # Re-parsear el HEX generado con el parser del updater: si esto
    # falla, el archivo no sirve para flashear.
    reparsed = load_image(args.output)
    assert bytes(reparsed) == bytes(image), "el HEX regenerado no re-parsea igual"
    print("\n".join(log))
    print(f"bytes cambiados: {info['changed_bytes']}")
    print(f"SP {info['sp']:#010x} Reset {info['reset']:#010x} (sin cambios)")
    print(f"SHA-256 imagen parcheada: {info['sha256']}")
    print(f"HEX escrito en {args.output} y verificado (re-parse OK).")
    print("Flashea con: python3 ../updater/rt1000_update.py "
          f"{args.output} --dry-run  (y luego --port ...)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
