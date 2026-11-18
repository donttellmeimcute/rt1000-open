#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Parser NMEA minimo, derivado del handler USART de V1.16:
 * busca '$', luego "GPRMC"/"GPGGA", valida checksum XOR tras '*'.
 * Sin cadenas literales "$GPRMC" en el firmware original: el
 * parser original compara byte a byte ('$','R','M',...). */
typedef struct {
    bool valid, fix; double lat, lon; double speed_kmh, course;
    int hour, min, sec; int day, month, year; double alt_m;
} rt1000_gps_t;
uint8_t rt1000_nmea_checksum(const char *s); /* XOR entre '$' y '*' */
bool rt1000_nmea_parse(const char *sentence, rt1000_gps_t *out);
