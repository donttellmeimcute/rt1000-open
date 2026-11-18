#include "bootloader_protocol.h"

uint8_t rt1000_checksum(const uint8_t *buf, size_t len)
{
    /* El original suma buf[0 .. len-2] y luego 'R' (82) + 'T' (84) + 1.
     * len es 1028 para un paquete de datos. */
    uint8_t sum = 0;
    for (size_t i = 0; i + 1 < len; i++) {
        sum += buf[i];
    }
    sum += 82;
    sum += 84;
    return (uint8_t)(sum + 1);
}

int rt1000_validate_packet(const uint8_t *pkt, uint32_t expected_offset)
{
    if (pkt[0] != 0x57) return -1; /* 'W' */
    if (pkt[1] != (uint8_t)(expected_offset >> 8)) return -2;
    if (pkt[2] != (uint8_t)(expected_offset & 0xFF)) return -3;
    if (pkt[RT1000_PACKET_SIZE - 1] != rt1000_checksum(pkt, RT1000_PACKET_SIZE)) return -4;
    return 0;
}
