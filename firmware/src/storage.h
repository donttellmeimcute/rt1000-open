#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Config en flash SPI externa (candidato RT-950 Pro, confirmado
 * byte a byte por jaymart1983 en ese radio; en el 1000Pro la
 * flash existe (lista 25Q en V1.16) pero el layout NO esta
 * verificado). Canal: 32 bytes. Frecuencia: BCD empaquetado
 * little-endian en unidades de 10 Hz (462.5625 MHz =
 * 50 62 25 46). Nombre: 12 bytes ASCII, relleno 0xFF.
 * Zonas: 10 slots de 16 bytes en 0xC000 (NO 0xA200, ese era
 * del RT-900 y rompia los nombres). */
#define CHAN_SIZE 32
#define CHAN_COUNT 999
typedef struct {
    uint32_t rx_hz, tx_hz; /* 0xFFFFFFFF tx = solo RX */
    uint16_t rx_tone, tx_tone;
    uint8_t sig_code, ptt_id, power_scramble, flags;
    char name[12];
} rt1000_chan_t;
bool chan_encode(const rt1000_chan_t *c, uint8_t out[CHAN_SIZE]);
bool chan_decode(const uint8_t in[CHAN_SIZE], rt1000_chan_t *c);
uint32_t bcd_pack_10hz(uint32_t hz);
uint32_t bcd_unpack_10hz(const uint8_t b[4]);
