#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Receptor de broadcast candidato: SI4732 (FM/AM/SSB).
 * Fuente: RT-950 Pro (Hertzz58 / jaymart1983): I2C bit-bang
 * en PB6/PB7, direccion 7-bit 0x11. En el RT-1000Pro NO
 * VERIFICADO. Test TEST=5: GET_REV tras POWER_UP.
 */
bool si4732_power_up(void);
bool si4732_get_rev(uint8_t rev[9]);
bool si4732_tune_fm(uint32_t freq_khz);
