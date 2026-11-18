#pragma once
#include <stdint.h>
#include <stddef.h>
/* APRS: AX.25 UI frame + position packet. El callsign NO va
 * hardcodeado: se pasa por parametro (por defecto, vacio).
 * AFSK 1200 baud / modulacion es trabajo de hardware (DAC +
 * transceptor), no esta aqui. */
size_t rt1000_ax25_addr(uint8_t *out, const char *callsign, uint8_t ssid, int last);
size_t rt1000_aprs_position(uint8_t *out, size_t cap, const char *src, uint8_t ssid,
    const char *dst, double lat, double lon, double alt_m, const char *comment);
