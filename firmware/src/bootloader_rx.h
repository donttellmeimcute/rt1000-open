#pragma once
#include <stdint.h>
#include <stddef.h>
#include "bootloader_protocol.h"
/* Receptor del protocolo de update, LADO RADIO.
 * Espejo del updater Python: CONNECT (x4) -> UPDATE -> 246
 * paquetes 'W' de 1028 bytes. Responde ACK 0x06 / NAK 0xFF
 * por la UART abstracta y escribe en la flash abstracta.
 * Estado inicial: espera CONNECT. */
typedef struct {
    rt1000_state_t state; uint32_t connects; uint32_t offset;
    uint32_t packets; uint8_t pkt[RT1000_PACKET_SIZE]; size_t pkt_len;
    int error;
} rt1000_rx_t;
void rt1000_rx_init(rt1000_rx_t *rx);
int  rt1000_rx_feed(rt1000_rx_t *rx, const uint8_t *buf, size_t len); /* 0=ok */
