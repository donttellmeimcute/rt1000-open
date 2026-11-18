#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Tonos y codigos del firmware original V1.16.
 * CTCSS: 48 tonos EIA estandar (67.0-254.1 Hz), en decimas de Hz.
 * DCS: 104 codigos estandar (023-754), con bit de polaridad.
 * DTMF: frecuencias de fila/columna por tecla.
 * Las tablas son datos estandar; la APLICACION al BK4829
 * (registros de subtone) sigue sin confirmar en el 1000Pro:
 * bk4829_set_tones() guarda la seleccion y documenta el
 * pendiente. El audio DTMF (DAC) tampoco esta confirmado. */
#define CTCSS_COUNT 48
#define DCS_COUNT 104
uint16_t rt1000_ctcss_tone(int idx); /* idx 0..47 -> Hz*10, 0 si invalido */
uint16_t rt1000_dcs_code(int idx);  /* idx 0..103 -> codigo 3 digitos, 0 si invalido */
bool rt1000_dtmf_freqs(char key, uint16_t *f_row, uint16_t *f_col);

/* Codificacion de tono en settings/canales (16 bits):
 * 0=OFF, 1..48 = CTCSS (idx+1), 49..152 = DCS (idx+49). */
uint16_t rt1000_tone_encode_ctcss(int idx);
uint16_t rt1000_tone_encode_dcs(int idx);
bool rt1000_tone_is_dcs(uint16_t code);
uint16_t rt1000_tone_value(uint16_t code); /* Hz*10 (CTCSS) o codigo (DCS), 0 si OFF */
void rt1000_tone_label(uint16_t code, char *buf, size_t n); /* "OFF", "67.0", "DCS023" */
