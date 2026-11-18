#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Transceptor candidato: BK4829 (x2 en el RT-950 Pro).
 * Fuente: Hertzz58 / jaymart1983 (RT-950 Pro, confirmado).
 * En el RT-1000Pro NO VERIFICADO: el V1.16 no nombra ningun
 * chip. Pines candidatos del 950 Pro: CS0=PE8, CS1=PE15,
 * CLK=PE10, DATA=PE11 (bit-bang SPI, compartido).
 * El test TEST=4 lee el chip ID y confirma o descarta el chip.
 *
 * Mapa de registros BK4829 (publico, familia BK4819/4829):
 * 0x00: modo, 0x02: freq baja, 0x03: freq alta, 0x30: TX,
 * 0x37: RSSI, 0x39: estado. Los valores exactos de calibracion
 * viven en la flash externa del radio, no aqui.
 */
void bk4829_init(void);
bool bk4829_read_id(uint8_t chip, uint16_t *id);
bool bk4829_set_freq(uint8_t chip, uint32_t freq_hz);
bool bk4829_set_tx_power(uint8_t chip, uint8_t level); /* 0=bajo 1=medio 2=alto */
int16_t bk4829_rssi_dbm(uint8_t chip);
void bk4829_tx_enable(uint8_t chip, bool on);
