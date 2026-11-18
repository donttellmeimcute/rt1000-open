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
