#!/usr/bin/env python3
"""Reproducible static analysis of Radtel RT-1000Pro firmware V1.16.
Reads ../rt1000-re/firmware.bin (proprietary, NOT included in this repo).
Requires: pip install capstone.  Usage: python3 analyze_v116.py
Prints: vector table, peripheral literal counts, SystemInit decode.
"""
import struct, re, collections, sys
from pathlib import Path
BIN = Path(__file__).resolve().parents[2] / "rt1000-re" / "firmware.bin"
data = BIN.read_bytes()
base = 0x08002800
print(f"firmware.bin: {len(data)} bytes, base 0x{base:08x}")
vecs = struct.unpack_from("<64I", data, 0)
print(f"SP={vecs[0]:#010x} Reset={vecs[1]:#010x}")
for i, v in enumerate(vecs):
    if v and i >= 2:
        print(f"  vec {i:2d} irq {i-16:3d} -> {v:#010x}")
# peripheral literals
vals = collections.Counter()
for i in range(0, len(data)-3, 4):
    v = struct.unpack_from("<I", data, i)[0]
    if 0x40000000 <= v < 0x40100000:
        vals[v] += 1
print("top peripheral literals:")
for v, c in vals.most_common(15):
    print(f"  {v:#010x}: {c}")
# PLLCFGR decode (SystemInit at 0x08022FA8 writes 0x00033002 to RCC+0x04)
v = 0x00033002
print(f"PLLCFGR 0x{v:08x}: PLLM={(v>>0)&0x3F} PLLN={(v>>6)&0x1FF} "
      f"PLLP=/ {[2,4,6,8][(v>>16)&3]} PLLSRC={(v>>22)&1} PLLQ={(v>>24)&0xF}")
print("  HSI 16MHz -> VCOin 8MHz, VCO 1536MHz, SYS 192MHz: FUERA del rango")
print("  STM32F4 (VCO 192-432MHz, VCOin 1-2MHz). Ver RE-ANALYSIS-2026-11-17.md")
# NMEA clues: no $GP strings, but USART IRQ checks '$' (0x24), '*' (0x2A), 'R','M'
for pat in [b"$GP", b"GPRMC", b"GPGGA", b"25Q80"]:
    print(pat, "found" if pat in data else "NOT in binary")
