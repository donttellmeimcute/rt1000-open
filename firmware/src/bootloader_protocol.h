#pragma once
/* Protocolo de update, lado radio.
 * Espejo del updater open source (../updater/rt1000_update.py) y del
 * actualizador oficial Ido_Update, verificado por desensamblado.
 * NO es código copiado del firmware original: es una reimplementación
 * del protocolo observado en el cable.
 */
#include <stdint.h>
#include <stddef.h>

#define RT1000_FLASH_BASE   0x08002800u
#define RT1000_IMAGE_SIZE   251904u   /* 246 * 1024 */
#define RT1000_PACKET_DATA  1024u
#define RT1000_PACKET_SIZE  1028u

/* CONNECT: "93" 05 10 28 | UPDATE: "93" 05 55 6D | END: "93" 05 EE 06 (sin uso) */
static const uint8_t RT1000_CONNECT[5] = {0x39, 0x33, 0x05, 0x10, 0x28};
static const uint8_t RT1000_UPDATE[5]  = {0x39, 0x33, 0x05, 0x55, 0x6D};

#define RT1000_ACK 0x06
#define RT1000_NAK 0xFF

/* checksum del protocolo: sum(buf[0:1027]) + 'R' + 'T' + 1 (mod 256) */
uint8_t rt1000_checksum(const uint8_t *buf, size_t len);

/* Valida un paquete de datos: 'W', offset y checksum. Devuelve 0 si OK. */
int rt1000_validate_packet(const uint8_t *pkt, uint32_t expected_offset);

/* Estado del receptor en el bootloader (por implementar con UART real). */
typedef enum {
    RT1000_STATE_WAIT_CONNECT = 0,
    RT1000_STATE_WAIT_UPDATE,
    RT1000_STATE_RECEIVING,
    RT1000_STATE_DONE,
    RT1000_STATE_ERROR,
} rt1000_state_t;
