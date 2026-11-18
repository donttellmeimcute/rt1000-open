#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Modelo VFO/canal. Las bandas son las del producto RT-1000Pro
 * y las tablas candidatas de PROTOCOL.md (SIN VERIFICAR su
 * funcion en el firmware). No son limites de TX desbloqueados:
 * airband TX sigue deshabilitado por fabrica (ver PROTOCOL.md 6). */
typedef struct { uint32_t freq_hz; uint8_t mode; /*0=FM 1=AM 2=USB 3=LSB 4=CW*/ uint8_t power; } rt1000_vfo_t;
bool rt1000_freq_in_rx_range(uint32_t hz);
bool rt1000_freq_tx_allowed(uint32_t hz); /* solo bandas de aficionado, segun specs */
uint32_t rt1000_freq_clamp_rx(uint32_t hz);

/* Repetidor: shift -1/0/+1 y offset en Hz. Talkaround ignora el
 * shift (TX = RX). Si el resultado no es banda de TX permitida,
 * devuelve 0 (Prohibit TX). */
uint32_t rt1000_tx_freq(uint32_t rx_hz, int shift_dir, uint32_t offset_hz, bool talkaround);

/* NOAA Weather Radio (EE.UU.): 7 canales, solo RX. */
#define NOAA_CH_COUNT 7
uint32_t rt1000_noaa_freq(int ch); /* ch 1..7, 0 si fuera de rango */
const char *rt1000_noaa_name(int ch);

/* Scan: siguiente frecuencia desde `from` con `step_hz`,
 * con wrap dentro de la banda RX (136-174 o 400-480 MHz). */
uint32_t rt1000_scan_next(uint32_t from, uint32_t step_hz);
