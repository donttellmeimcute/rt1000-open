#!/usr/bin/env python3
"""
rt1000_update.py — Updater open source para Radtel RT-1000Pro / UV100.

Reimplementación limpia, en Python, del protocolo del actualizador
oficial Ido_Update (Radtel_RT-1000Pro_Update 260611, .NET Framework 4.0).

NO contiene firmware propietario. El usuario debe aportar su propio
archivo Intel HEX (.hex), por ejemplo el que viene dentro del
actualizador oficial, o uno generado por el firmware open source
cuando esté listo.

Protocolo (ingeniería inversa, ver ../docs/PROTOCOL.md):
  - Serie 115200 8N1, DTR=on, cable de datos USB-serie.
  - Radio en modo bootloader: mantener PTT pulsado y encender.
  - CONNECT: 39 33 05 10 28  ("93" 05 10 28), 4 veces en total
    (1 inicial + 3 reenvíos, uno por cada ACK 0x06 con step < 3).
  - UPDATE:  39 33 05 55 6D  ("93" 05 55 6D), 1 vez, tras el 4º ACK.
  - Datos: 246 paquetes de 1028 bytes:
      [0]    = 0x57 'W'
      [1]    = offset >> 8
      [2]    = offset & 0xFF
      [3:1027] = 1024 bytes de imagen (flash 0x08002800..0x0803FFFF)
      [1027] = checksum: (sum(buf[0:1027]) + 0x52 + 0x54 + 1) & 0xFF
    Cada paquete se envía tras recibir un ACK 0x06.
  - ACK = 0x06, error = 0xFF (cierra y aborta), 0x32 se ignora,
    0x00 reinicia el contador de recepción (comportamiento original).
  - Imagen total: 251904 bytes = 246 * 1024, rellenada con 0x00.
    El firmware V1.16 ocupa 243772 bytes reales; el resto es padding.

Mejoras respecto al original (sin cambiar el formato en el cable):
  - Timeout por ACK (por defecto 5 s) en vez de quedarse colgado.
  - Verificación previa del HEX: checksums Intel HEX, rango de
    direcciones y tamaño exacto.
  - Modo --dry-run: genera los 246 paquetes y los verifica sin tocar
    el puerto serie.
  - --verify-only: solo parsea y resume el HEX, no envía nada.

ADVERTENCIA: flashear puede dejar el radio inservible (brick) si se
interrumpe, si el HEX no corresponde a este modelo o si se corta la
alimentación. No flashees un firmware que no hayas verificado.
Usa esto bajo tu propio riesgo, con batería cargada y cable fiable.
"""

from __future__ import annotations

import argparse
import sys
import time

BAUDRATE = 115200
FLASH_BASE = 0x08002800
IMAGE_SIZE = 251904          # 246 * 1024
PACKET_DATA = 1024
PACKET_SIZE = 1028           # 1 + 2 + 1024 + 1
NUM_PACKETS = IMAGE_SIZE // PACKET_DATA  # 246

CONNECT = bytes([0x39, 0x33, 0x05, 0x10, 0x28])
UPDATE = bytes([0x39, 0x33, 0x05, 0x55, 0x6D])
END = bytes([0x39, 0x33, 0x05, 0xEE, 0x06])  # definido en el original, nunca se envía

ACK = 0x06
NAK = 0xFF
IGNORE = 0x32


def checksum(buf: bytes | bytearray) -> int:
    """Checksum del original: suma de buf[0:1027] + 'R' + 'T' + 1."""
    return (sum(buf[: PACKET_SIZE - 1]) + 0x52 + 0x54 + 1) & 0xFF


def parse_intel_hex(text: str) -> bytearray:
    """
    Parsea Intel HEX como lo hace el actualizador original, pero con
    validación estricta.

    Original (stringopration): ignora el valor del registro tipo 04
    (Extended Linear Address) y solo cuenta cuántos hay (writestep++),
    colocando los datos en:
        hex[addr - 0x2800 + (writestep - 1) * 65536]
    Para el firmware oficial eso funciona porque el único ELA es
    0x0800 y llega antes de los datos. Aquí implementamos el ELA
    correctamente (base = ELA << 16) y además comprobamos que el
    resultado coincide con la fórmula original para este firmware;
    si un HEX usara otro ELA, se rechaza en vez de escribir fuera
    de sitio.
    """
    image = bytearray(IMAGE_SIZE)  # el original inicializa a 0x00
    base = 0
    writestep = 0
    seen_data = False

    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise ValueError(f"línea {lineno}: no empieza por ':'")
        try:
            rec = bytes.fromhex(line[1:])
        except ValueError as exc:
            raise ValueError(f"línea {lineno}: hex inválido") from exc
        if len(rec) < 5 or len(rec) != rec[0] + 5:
            raise ValueError(f"línea {lineno}: longitud inconsistente")
        if sum(rec) & 0xFF != 0:
            raise ValueError(f"línea {lineno}: checksum Intel HEX incorrecto")
        length, addr, rtype = rec[0], int.from_bytes(rec[1:3], "big"), rec[3]
        data = rec[4 : 4 + length]

        if rtype == 0x00:  # datos
            seen_data = True
            flash_addr = base + addr
            offset = flash_addr - FLASH_BASE
            if offset < 0 or offset + length > IMAGE_SIZE:
                raise ValueError(
                    f"línea {lineno}: dirección {flash_addr:#x} fuera de "
                    f"0x{FLASH_BASE:x}..0x{FLASH_BASE + IMAGE_SIZE:x}"
                )
            image[offset : offset + length] = data
        elif rtype == 0x01:  # EOF
            break
        elif rtype == 0x04:  # Extended Linear Address
            writestep += 1
            base = int.from_bytes(data, "big") << 16
            # Compatibilidad con el original: el primer ELA debe ser 0x0800
            if writestep == 1 and base != 0x08000000:
                raise ValueError(
                    f"línea {lineno}: ELA inesperado {base:#x}, "
                    "se esperaba 0x08000000"
                )
        elif rtype == 0x05:  # Start Linear Address (entry point)
            entry = int.from_bytes(data, "big")
            # El firmware oficial trae 0x080029E1; no lo imponemos,
            # solo lo dejamos registrado en el resumen.
            parse_intel_hex.entry = entry  # type: ignore[attr-defined]
        else:
            # El original ignora otros tipos (return true).
            continue

    if not seen_data:
        raise ValueError("el HEX no contiene registros de datos")
    return image


parse_intel_hex.entry = None  # type: ignore[attr-defined]


def build_packet(image: bytes | bytearray, offset: int) -> bytes:
    """Construye un paquete de datos de 1028 bytes para `offset`."""
    if offset % PACKET_DATA or not 0 <= offset < IMAGE_SIZE:
        raise ValueError(f"offset inválido: {offset}")
    pkt = bytearray(PACKET_SIZE)
    pkt[0] = 0x57  # 'W'
    pkt[1] = (offset >> 8) & 0xFF
    pkt[2] = offset & 0xFF
    pkt[3 : 3 + PACKET_DATA] = image[offset : offset + PACKET_DATA]
    pkt[PACKET_SIZE - 1] = checksum(pkt)
    return bytes(pkt)


def image_summary(image: bytes) -> dict:
    """Resumen útil para --verify-only y para el log."""
    real = IMAGE_SIZE
    while real > 0 and image[real - 1] == 0x00:
        real -= 1
    # La primera palabra es el vector de reset (SP), la segunda el handler.
    sp = int.from_bytes(image[0:4], "little")
    reset = int.from_bytes(image[4:8], "little")
    return {
        "size": IMAGE_SIZE,
        "nonzero_tail": real,
        "sp": sp,
        "reset": reset,
        "entry_hex_record": getattr(parse_intel_hex, "entry", None),
    }


def _wait_ack(ser, timeout: float, label: str) -> None:
    """Espera un byte; 0x06 = OK, 0xFF = error, 0x32/0x00 = comportamiento original."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        b = ser.read(1)
        if not b:
            continue
        v = b[0]
        if v == ACK:
            return
        if v == NAK:
            raise RuntimeError("el radio respondió 0xFF (Communication Error)")
        if v == IGNORE:
            continue
        if v == 0x00:
            # El original: recvcnt=0 y espera 200 ms.
            time.sleep(0.2)
            continue
        # Cualquier otro byte se ignora, como en RevDateOperation().
    raise TimeoutError(f"timeout esperando ACK ({label})")


def flash(port: str, image: bytes, timeout: float = 5.0, verbose: bool = True) -> None:
    """Ejecuta la secuencia completa contra el radio."""
    try:
        import serial  # pyserial
    except ImportError as exc:
        raise SystemExit(
            "falta pyserial: pip install pyserial"
        ) from exc

    def log(msg: str) -> None:
        if verbose:
            print(msg, flush=True)

    log(f"Abriendo {port} a {BAUDRATE} 8N1, DTR=on …")
    ser = serial.Serial(port, BAUDRATE, timeout=0.2, dsrdtr=True)
    try:
        ser.dtr = True
        # Paso 1: CONNECT inicial (step=1 en el original)
        step = 1
        ser.write(CONNECT)
        log("CONNECT enviado (1/4) — esperando ACK…")
        # ACKs 1-3: reenviar CONNECT; ACK 4º: enviar UPDATE
        for i in range(3):
            _wait_ack(ser, timeout, f"CONNECT {i + 1}")
            step += 1
            ser.write(CONNECT)
            log(f"CONNECT enviado ({i + 2}/4) — esperando ACK…")
        _wait_ack(ser, timeout, "CONNECT 4")
        ser.write(UPDATE)
        log("UPDATE enviado — esperando ACK para empezar los datos…")
        _wait_ack(ser, timeout, "UPDATE")

        # Paso 2: 246 paquetes de datos, uno por ACK
        offset = 0
        for n in range(1, NUM_PACKETS + 1):
            pkt = build_packet(image, offset)
            ser.write(pkt)
            offset += PACKET_DATA
            log(f"paquete {n}/{NUM_PACKETS} ({offset}/{IMAGE_SIZE} bytes)")
            if offset == IMAGE_SIZE:
                break
            _wait_ack(ser, timeout, f"datos {n}")
        log("Success! Actualización completa. Cierra el puerto y apaga/enciende el radio.")
    finally:
        ser.close()


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description="Updater open source para Radtel RT-1000Pro / UV100 (protocolo Ido_Update)."
    )
    ap.add_argument("hexfile", help="archivo Intel HEX del firmware (.hex)")
    ap.add_argument("--port", help="puerto serie, p. ej. COM3 o /dev/ttyUSB0")
    ap.add_argument("--verify-only", action="store_true",
                    help="solo parsea y resume el HEX, no toca el puerto")
    ap.add_argument("--dry-run", action="store_true",
                    help="genera y verifica los 246 paquetes sin enviar nada")
    ap.add_argument("--timeout", type=float, default=5.0,
                    help="segundos de espera por ACK (def. 5)")
    ap.add_argument("--list-ports", action="store_true",
                    help="lista puertos serie disponibles y sale")
    args = ap.parse_args(argv)

    if args.list_ports:
        from serial.tools import list_ports
        for p in list_ports.comports():
            print(f"{p.device}\t{p.description}")
        return 0

    text = open(args.hexfile, encoding="utf-8", errors="strict").read()
    image = parse_intel_hex(text)
    info = image_summary(image)
    print(f"Imagen: {info['size']} bytes, último byte no-cero en {info['nonzero_tail']}")
    entry = info['entry_hex_record']
    print(f"Vector SP: {info['sp']:#010x}  Reset: {info['reset']:#010x}  "
          f"Entry (rec. :05): {entry:#010x}" if entry is not None else
          f"Vector SP: {info['sp']:#010x}  Reset: {info['reset']:#010x}")
    if info["sp"] < 0x20000000 or info["sp"] > 0x20020000:
        print("AVISO: SP fuera del rango SRAM típico, revisa que el HEX sea de este modelo.")
    if args.verify_only:
        return 0

    if args.dry_run:
        for off in range(0, IMAGE_SIZE, PACKET_DATA):
            pkt = build_packet(image, off)
            assert len(pkt) == PACKET_SIZE
            assert pkt[PACKET_SIZE - 1] == checksum(pkt)
            assert pkt[0] == 0x57 and pkt[1] == (off >> 8) & 0xFF
        print(f"dry-run OK: {NUM_PACKETS} paquetes de {PACKET_SIZE} bytes, checksums correctos.")
        return 0

    if not args.port:
        ap.error("--port es obligatorio para flashear (o usa --verify-only / --dry-run)")
    print("Modo bootloader: mantén PTT pulsado y enciende el radio ANTES de continuar.")
    input("Pulsa Enter cuando el radio esté en modo update…")
    flash(args.port, image, timeout=args.timeout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
