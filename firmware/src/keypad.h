#pragma once
#include <stdint.h>
/* Teclado candidato (RT-950 Pro, jaymart1983, medido en hardware):
 * columnas PC0-PC3, filas PD4-PD7. La tabla original estaba mal
 * en todas las posiciones; la correcta (medida):
 * PC0: OK ABC Back V/M | PC1: 3 6 9 # | PC2: 2 5 8 0 |
 * PC3: Up Down Left Right | todo HIGH: 1 4 7 *
 * Settle time: las filas usan pull-up ~40k, necesitan mas de
 * 1 us. En el RT-1000Pro NO VERIFICADO.
 */
#define KEY_NONE 0
uint16_t keypad_scan_raw(void); /* bitmask de filas por columna */
int keypad_get_key(void);       /* codigo de tecla o KEY_NONE */
