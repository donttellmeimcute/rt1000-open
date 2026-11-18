#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* APRS: AX.25 UI frame + position packet. El callsign NO va
 * hardcodeado: se pasa por parametro (por defecto, vacio).
 * AFSK 1200 baud / modulacion es trabajo de hardware (DAC +
 * transceptor), no esta aqui. */
size_t rt1000_ax25_addr(uint8_t *out, const char *callsign, uint8_t ssid, int last);
size_t rt1000_aprs_position(uint8_t *out, size_t cap, const char *src, uint8_t ssid,
    const char *dst, double lat, double lon, double alt_m, const char *comment);
/* Posicion con path de digipeaters ("WIDE1-1,WIDE2-1" o "").
 * El ultimo address lleva el bit last. */
size_t rt1000_aprs_position_path(uint8_t *out, size_t cap, const char *src, uint8_t ssid,
    const char *dst, const char *path, double lat, double lon, double alt_m, const char *comment);
/* FCS AX.25 (CRC-16 X.25: poly 0x8408, init 0xFFFF, xorout 0xFFFF).
 * El original muestra "FCS Check: Success/Fail" sobre el frame. */
uint16_t rt1000_ax25_fcs(const uint8_t *data, size_t len);
bool rt1000_ax25_fcs_ok(const uint8_t *frame, size_t len); /* frame con FCS al final */
