#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "display.h"
#include "storage.h"

/* Capa de UI del RT-1000Pro (ST7789V 240x320, vertical).
 * Todas las pantallas reciben sus datos por parametros para
 * poder emularlas en host con valores demo (tests/emu.c).
 * Nada aqui toca hardware: solo dibuja con lcd_*.
 */

#define UI_GRAY   0x8410
#define UI_DARK   0x1082
#define UI_YELLOW 0xFFE0
#define UI_CYAN   0x07FF

/* Temas de color de la UI. La paleta activa la elige ui_set_theme();
 * por defecto es KAWAII (rosa claro). ORIGINAL es el look negro/verde.
 * Los campos son: fondo, texto, acento, cian, gris, oscuro, amarillo, rojo. */
typedef struct {
    uint16_t bg, fg, acc, cyn, gry, drk, ylw, red;
} ui_palette_t;
extern const ui_palette_t UI_PAL_ORIG, UI_PAL_KAWAII;
void ui_set_theme(int theme); /* 0 = ORIGINAL, 1 = KAWAII */
int ui_get_theme(void);
const char *ui_theme_name(void);

typedef struct {
    uint32_t vfo_a_hz, vfo_b_hz;
    uint8_t active;      /* 0=A, 1=B */
    uint8_t mode;        /* 0 FM 1 AM 2 USB 3 LSB 4 CW */
    uint8_t power;       /* 0 Baja 1 Media 2 Alta */
    uint8_t sql;         /* 0-9 */
    uint32_t step_hz;
    int16_t rssi_dbm;    /* S-meter del VFO activo */
    bool tx;
    bool gps_fix;
    int gps_h, gps_m, gps_s;
    double gps_lat, gps_lon, gps_alt_m;
    int gps_sats;
    double speed_kmh, course;
    const char *callsign;
    const char *device_name;
    const char *aprs_state; /* "OFF", "30s", "60s", "5min" */
    bool monitor;        /* Monitor Mode: squelch abierto */
    bool scan;           /* Scanning activo */
    bool modem;          /* Modo Módem MMDVM activo */
    int batt_mv;         /* bateria en mV, 0 si sin ADC */
    uint8_t gps_fmt;     /* 0 grado, 1 grado.min, 2 grado.min.seg */
} ui_state_t;

extern const char *UI_MODE_NAMES[5];
extern const char *UI_POWER_NAMES[3];

/* Texto con la fuente 5x7 escalada (scale 1..4). */
void ui_text_scaled(uint16_t x, uint16_t y, const char *s, uint8_t scale,
                    uint16_t fg, uint16_t bg);
void ui_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void ui_smeter(uint16_t x, uint16_t y, uint16_t w, int16_t rssi_dbm);

void ui_splash(const char *callsign, const char *version);
void ui_main(const ui_state_t *st);
void ui_entry(const ui_state_t *st, const char *buffer);
void ui_menu(const char *labels[], const char *values[], int count, int selected);
void ui_gps_screen(const ui_state_t *st);
void ui_chan_screen(const rt1000_chan_t *chans, const bool *used, int count, int selected, const char *zone_name);
void ui_info_screen(const ui_state_t *st, const char *version);
void ui_fm_screen(uint32_t freq_khz, bool present);
void ui_aprs_screen(const char *callsign, uint8_t ssid, const char *path, const char *fcs);
void ui_editor_screen(const char *title, const char *buffer, int cursor);
void ui_alarm_screen(const char *callsign);
void ui_msg_screen(const char *line1, const char *line2);
void ui_apo_warn_screen(uint32_t seconds_left); /* aviso APO: "Power Off!" + cuenta atras */
void ui_modem_screen(uint8_t proto); /* 0=OFF, 1=DMR, 2=NXDN, 3=P25 */
