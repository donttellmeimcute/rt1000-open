"""Tests del updater open source (sin hardware).

Usa el HEX oficial extraído solo como fixture local: NO se incluye
en el repo. Para correrlos, pon un firmware.hex válido junto al test
o pasa su ruta con la variable RT1000_HEX.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import rt1000_update as u  # noqa: E402

HEX_PATH = os.environ.get("RT1000_HEX", "")


def test_constants():
    assert u.BAUDRATE == 115200
    assert u.IMAGE_SIZE == 251904
    assert u.NUM_PACKETS == 246
    assert u.CONNECT == bytes([0x39, 0x33, 0x05, 0x10, 0x28])
    assert u.UPDATE == bytes([0x39, 0x33, 0x05, 0x55, 0x6D])
    assert u.END == bytes([0x39, 0x33, 0x05, 0xEE, 0x06])


def test_checksum_known_packet():
    pkt = bytearray(u.PACKET_SIZE)
    pkt[0] = 0x57
    # checksum = sum(buf[0:1027]) + 'R' + 'T' + 1
    assert u.checksum(pkt) == (0x57 + 0x52 + 0x54 + 1) & 0xFF


def test_parse_and_packets():
    if not HEX_PATH or not os.path.exists(HEX_PATH):
        print("saltado: pon RT1000_HEX=/ruta/a/firmware.hex")
        return
    img = u.parse_intel_hex(open(HEX_PATH).read())
    assert len(img) == u.IMAGE_SIZE
    assert int.from_bytes(img[0:4], "little") == 0x2000AEC8
    assert int.from_bytes(img[4:8], "little") == 0x08002AC1
    for off in range(0, u.IMAGE_SIZE, u.PACKET_DATA):
        pkt = u.build_packet(img, off)
        assert len(pkt) == u.PACKET_SIZE
        assert pkt[0] == 0x57
        assert pkt[1] == (off >> 8) & 0xFF and pkt[2] == off & 0xFF
        assert pkt[-1] == u.checksum(pkt)


if __name__ == "__main__":
    test_constants()
    test_checksum_known_packet()
    test_parse_and_packets()
    print("tests OK")
