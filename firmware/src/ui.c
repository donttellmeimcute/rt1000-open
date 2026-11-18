#include "ui.h"
#include <stdio.h>
#include <string.h>

const char *UI_MODE_NAMES[5] = {"FM", "AM", "USB", "LSB", "CW"};
const char *UI_POWER_NAMES[3] = {"BAJA", "MEDIA", "ALTA"};

/* Paletas RGB565. KAWAII: fondo #FFE9F3, texto #831843,
 * acento #EC4899, cian #DB2777, gris #C48BA7, oscuro #F9A8D4,
 * amarillo #F59E0B, rojo #E11D48. */
const ui_palette_t UI_PAL_ORIG = {
    LCD_BLACK, LCD_WHITE, LCD_GREEN, UI_CYAN,
    UI_GRAY, UI_DARK, UI_YELLOW, LCD_RED};
const ui_palette_t UI_PAL_KAWAII = {
    0xFF3D, 0x78A8, 0xE232, 0xD12E,
    0xBC54, 0xF539, 0xECE1, 0xD8E8};
static const ui_palette_t *pal = &UI_PAL_KAWAII; /* tema por defecto */

void ui_set_theme(int theme){ pal = theme ? &UI_PAL_KAWAII : &UI_PAL_ORIG; }
int ui_get_theme(void){ return pal == &UI_PAL_KAWAII ? 1 : 0; }
const char *ui_theme_name(void){ return ui_get_theme() ? "KAWAII" : "ORIGINAL"; }

#define C_BG  (pal->bg)
#define C_FG  (pal->fg)
#define C_ACC (pal->acc)
#define C_CYN (pal->cyn)
#define C_GRY (pal->gry)
#define C_DRK (pal->drk)
#define C_YLW (pal->ylw)
#define C_RED (pal->red)

/* Texto escalado: reusa lcd_text para scale=1; para scale>1
 * pinta cada pixel de la fuente 5x7 como un bloque scale x scale.
 * La fuente vive en display.c; aqui se reimplementa el acceso
 * por pixel con lcd_draw_pixel para no tocar ese modulo. */
void ui_text_scaled(uint16_t x, uint16_t y, const char *s, uint8_t scale,
                    uint16_t fg, uint16_t bg){
    if(scale <= 1){ lcd_text(x, y, s, fg, bg); return; }
    while(*s){
        if(*s >= ' ' && *s <= '~'){
            for(int col = 0; col < 5; col++){
                uint8_t bits = lcd_font_col((uint8_t)*s, (uint8_t)col);
                for(int row = 0; row < 7; row++){
                    uint16_t color = (bits >> row) & 1 ? fg : bg;
                    for(int dy = 0; dy < scale; dy++)
                        for(int dx = 0; dx < scale; dx++)
                            lcd_draw_pixel(x + col * scale + dx,
                                           y + row * scale + dy, color);
                }
            }
        }
        x += (uint16_t)(6 * scale);
        s++;
    }
}

void ui_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color){
    lcd_fill_rect(x, y, w, 1, color);
}

/* S-meter: 10 segmentos; S9 ~= -73 dBm, S0 ~= -120 dBm. */
void ui_smeter(uint16_t x, uint16_t y, uint16_t w, int16_t rssi_dbm){
    int s = (int)(rssi_dbm + 120) / 6; /* -120 -> 0, -66 -> 9 */
    if(s < 0) s = 0;
    if(s > 10) s = 10;
    uint16_t seg = w / 10;
    lcd_text(x, y, "S", C_FG, C_BG);
    for(int i = 0; i < 10; i++){
        uint16_t c = C_BG;
        if(i < s) c = (i < 7) ? C_ACC : (i < 9) ? C_YLW : C_RED;
        lcd_fill_rect(x + 10 + i * seg, y + 1, seg - 2, 6,
                      i < s ? c : C_DRK);
    }
    char buf[8];
    snprintf(buf, sizeof buf, "S%d", s > 9 ? 9 : s);
    lcd_text(x + 10 + 10 * seg + 2, y, buf, C_FG, C_BG);
}

static void fmt_freq(char *out, size_t n, uint32_t hz){
    snprintf(out, n, "%lu.%04lu", (unsigned long)(hz / 1000000),
             (unsigned long)((hz % 1000000) / 100));
}

static void status_bar(const ui_state_t *st){
    lcd_fill_rect(0, 0, LCD_W, 16, C_FG);
    lcd_text(4, 4, st->callsign, C_BG, C_FG);
    if(st->gps_fix){
        char buf[16];
        snprintf(buf, sizeof buf, "%02d:%02d", st->gps_h, st->gps_m);
        lcd_text(200, 4, buf, C_BG, C_FG);
        lcd_text(150, 4, "GPS", C_ACC, C_FG);
    } else {
        lcd_text(150, 4, "SIN GPS", C_RED, C_FG);
    }
    if(st->tx) lcd_text(100, 4, "TX", C_RED, C_FG);
    else lcd_text(100, 4, "RX", C_BG, C_FG);
    /* Un solo indicador en x=120: MDM > SCN > MON. */
    if(st->modem) lcd_text(120, 4, "MDM", C_CYN, C_FG);
    else if(st->scan) lcd_text(120, 4, "SCN", C_YLW, C_FG);
    else if(st->monitor) lcd_text(120, 4, "MON", C_YLW, C_FG);
}

void ui_splash(const char *callsign, const char *version){
    lcd_fill(C_BG);
    ui_text_scaled(30, 96, "RT-1000Pro", 3, C_FG, C_BG);
    lcd_text(84, 136, "OPEN FIRMWARE", C_ACC, C_BG);
    ui_text_scaled(84, 168, callsign, 2, C_CYN, C_BG);
    char buf[32];
    snprintf(buf, sizeof buf, "FW %s", version);
    lcd_text(96, 200, buf, C_FG, C_BG);
    lcd_text(60, 232, "donttellmeimcute", C_GRY, C_BG);
}

void ui_main(const ui_state_t *st){
    char buf[40];
    lcd_fill(C_BG);
    status_bar(st);

    /* VFO A: grande. */
    uint16_t col_a = st->active == 0 ? C_ACC : C_FG;
    char lab_a[4] = "A";
    ui_text_scaled(8, 24, lab_a, 2, col_a, C_BG);
    lcd_text(200, 28, UI_MODE_NAMES[st->mode], C_FG, C_BG);
    fmt_freq(buf, sizeof buf, st->vfo_a_hz);
    ui_text_scaled(8, 48, buf, 3, st->active == 0 ? C_FG : C_GRY, C_BG);
    lcd_text(180, 52, "MHz", C_FG, C_BG);
    snprintf(buf, sizeof buf, "STEP %lu.%01luk  SQL %d  PWR %s",
             (unsigned long)(st->step_hz / 1000),
             (unsigned long)((st->step_hz % 1000) / 100),
             st->sql, UI_POWER_NAMES[st->power]);
    lcd_text(8, 88, buf, C_FG, C_BG);
    ui_smeter(8, 104, 200, st->active == 0 ? st->rssi_dbm : -120);

    ui_hline(0, 124, LCD_W, C_DRK);

    /* VFO B: compacto. */
    uint16_t col_b = st->active == 1 ? C_ACC : C_FG;
    ui_text_scaled(8, 136, "B", 2, col_b, C_BG);
    fmt_freq(buf, sizeof buf, st->vfo_b_hz);
    ui_text_scaled(8, 160, buf, 2, st->active == 1 ? C_FG : C_GRY, C_BG);
    lcd_text(180, 164, "MHz", C_FG, C_BG);
    lcd_text(8, 188, UI_MODE_NAMES[st->mode], C_FG, C_BG);
    ui_smeter(8, 204, 200, st->active == 1 ? st->rssi_dbm : -120);

    ui_hline(0, 280, LCD_W, C_DRK);
    lcd_text(4, 292, "OK MENU V/M VFO # GPS ABC CANAL", C_FG, C_BG);
}

void ui_entry(const ui_state_t *st, const char *buffer){
    ui_main(st);
    lcd_fill_rect(8, 128, 224, 72, C_FG);
    lcd_text(16, 136, "ENTRAR FREC. MHz:", C_BG, C_FG);
    char buf[24];
    snprintf(buf, sizeof buf, "%s_", buffer);
    ui_text_scaled(16, 156, buf, 2, C_BG, C_FG);
    lcd_text(16, 184, "OK confirma  BACK cancela", C_BG, C_FG);
}

void ui_menu(const char *labels[], const char *values[], int count, int selected){
    lcd_fill(C_BG);
    lcd_fill_rect(0, 0, LCD_W, 16, C_FG);
    lcd_text(4, 4, "MENU", C_BG, C_FG);
    /* Ventana con scroll: 10 filas visibles de 24 px. */
    int start = 0;
    if(count > 10){
        start = selected - 9;
        if(start < 0) start = 0;
        if(start > count - 10) start = count - 10;
    }
    int shown = count - start < 10 ? count - start : 10;
    for(int i = 0; i < shown; i++){
        int idx = start + i;
        uint16_t y = 24 + i * 24;
        if(idx == selected){
            lcd_fill_rect(0, y, LCD_W, 22, C_FG);
            lcd_text(8, y + 7, labels[idx], C_BG, C_FG);
            if(values[idx]) lcd_text(150, y + 7, values[idx], C_BG, C_FG);
        } else {
            lcd_text(8, y + 7, labels[idx], C_FG, C_BG);
            if(values[idx]) lcd_text(150, y + 7, values[idx], C_ACC, C_BG);
        }
    }
    if(start > 0) lcd_text(224, 24, "^", C_GRY, C_BG);
    if(start + shown < count) lcd_text(224, 264, "v", C_GRY, C_BG);
    lcd_text(4, 292, "UP/DOWN mueve  L/R cambia  OK entra", C_FG, C_BG);
}

static void fmt_coord(char *out, size_t n, double deg, bool lat, uint8_t fmt){
    double a = deg < 0 ? -deg : deg;
    char hemi = lat ? (deg < 0 ? 'S' : 'N') : (deg < 0 ? 'W' : 'E');
    if(fmt == 1){
        int d = (int)a; double m = (a - d) * 60.0;
        snprintf(out, n, "%d %06.3f%c", d, m, hemi);
    } else if(fmt == 2){
        int d = (int)a; double mf = (a - d) * 60.0; int m = (int)mf;
        double s = (mf - m) * 60.0;
        snprintf(out, n, "%d %02d %04.1f%c", d, m, s, hemi);
    } else {
        snprintf(out, n, "%.4f %c", deg, hemi);
    }
}

void ui_gps_screen(const ui_state_t *st){
    char buf[48];
    lcd_fill(C_BG);
    lcd_fill_rect(0, 0, LCD_W, 16, C_FG);
    lcd_text(4, 4, "GPS", C_BG, C_FG);
    if(st->gps_fix){
        snprintf(buf, sizeof buf, "%02d:%02d:%02d", st->gps_h, st->gps_m, st->gps_s);
        ui_text_scaled(8, 32, buf, 3, C_FG, C_BG);
        char c[24];
        fmt_coord(c, sizeof c, st->gps_lat, true, st->gps_fmt);
        snprintf(buf, sizeof buf, "LAT %s", c);
        ui_text_scaled(8, 80, buf, 2, C_ACC, C_BG);
        fmt_coord(c, sizeof c, st->gps_lon, false, st->gps_fmt);
        snprintf(buf, sizeof buf, "LON %s", c);
        ui_text_scaled(8, 108, buf, 2, C_ACC, C_BG);
        snprintf(buf, sizeof buf, "ALT %.0f m  SAT %d", st->gps_alt_m, st->gps_sats);
        lcd_text(8, 140, buf, C_FG, C_BG);
        snprintf(buf, sizeof buf, "VEL %.0f km/h  RUMBO %03.0f", st->speed_kmh, st->course);
        lcd_text(8, 156, buf, C_FG, C_BG);
    } else {
        ui_text_scaled(8, 32, "SIN FIX", 3, C_RED, C_BG);
        lcd_text(8, 80, "Buscando satelites...", C_FG, C_BG);
        lcd_text(8, 96, "GPS en USART3 9600", C_GRY, C_BG);
    }
    snprintf(buf, sizeof buf, "APRS %s: %s", st->callsign, st->aprs_state);
    lcd_text(8, 180, buf, C_FG, C_BG);
    lcd_text(8, 196, "APRS TX por AFSK: pendiente", C_GRY, C_BG);
    lcd_text(4, 292, "BACK vuelve", C_FG, C_BG);
}

void ui_chan_screen(const rt1000_chan_t *chans, const bool *used, int count, int selected, const char *zone_name){
    char buf[48];
    lcd_fill(C_BG);
    lcd_fill_rect(0, 0, LCD_W, 16, C_FG);
    snprintf(buf, sizeof buf, "CANALES %s", zone_name ? zone_name : "");
    lcd_text(4, 4, buf, C_BG, C_FG);
    int shown = count < 8 ? count : 8;
    for(int i = 0; i < shown; i++){
        uint16_t y = 24 + i * 28;
        if(used[i]){
            char f[16];
            fmt_freq(f, sizeof f, chans[i].rx_hz);
            snprintf(buf, sizeof buf, "%02d %-11s %s", i + 1, chans[i].name, f);
        } else {
            snprintf(buf, sizeof buf, "%02d ------------", i + 1);
        }
        if(i == selected){
            lcd_fill_rect(0, y, LCD_W, 24, C_FG);
            lcd_text(8, y + 8, buf, C_BG, C_FG);
        } else {
            lcd_text(8, y + 8, buf, used[i] ? C_FG : C_GRY, C_BG);
        }
    }
    lcd_text(4, 292, "OK recupera  ABC guarda  BACK sale", C_FG, C_BG);
}

void ui_info_screen(const ui_state_t *st, const char *version){
    lcd_fill(C_BG);
    lcd_fill_rect(0, 0, LCD_W, 16, C_FG);
    lcd_text(4, 4, "INFO", C_BG, C_FG);
    char buf[48];
    snprintf(buf, sizeof buf, "RT-1000Pro open %s", version);
    lcd_text(8, 32, buf, C_FG, C_BG);
    snprintf(buf, sizeof buf, "Callsign: %s", st->callsign);
    lcd_text(8, 48, buf, C_ACC, C_BG);
    snprintf(buf, sizeof buf, "Nombre: %s", st->device_name);
    lcd_text(8, 64, buf, C_FG, C_BG);
    if(st->batt_mv > 0)
        snprintf(buf, sizeof buf, "BATT: %.1fV", st->batt_mv / 1000.0);
    else
        snprintf(buf, sizeof buf, "BATT: --.-V (ADC sin conf.)");
    lcd_text(8, 80, buf, C_FG, C_BG);
    lcd_text(8, 104, "HW: SIN CONFIRMAR", C_RED, C_BG);
    lcd_text(8, 120, "MCU ?  BK4829 ?  LCD ?", C_GRY, C_BG);
    lcd_text(8, 152, "Flashear tests 3 y 4", C_FG, C_BG);
    lcd_text(8, 168, "antes que la app.", C_FG, C_BG);
    lcd_text(4, 292, "BACK vuelve", C_FG, C_BG);
}

void ui_fm_screen(uint32_t freq_khz, bool present){
    char buf[48];
    lcd_fill(C_BG);
    lcd_fill_rect(0, 0, LCD_W, 16, C_FG);
    lcd_text(4, 4, "RADIO FM", C_BG, C_FG);
    snprintf(buf, sizeof buf, "%lu.%02lu MHz", (unsigned long)(freq_khz / 1000),
             (unsigned long)((freq_khz % 1000) / 10));
    ui_text_scaled(8, 48, buf, 3, C_FG, C_BG);
    snprintf(buf, sizeof buf, "SI4732: %s", present ? "detectado" : "no detectado");
    lcd_text(8, 104, buf, present ? C_FG : C_RED, C_BG);
    lcd_text(8, 136, "FM broadcast 87.5-108", C_GRY, C_BG);
    lcd_text(4, 292, "UP/DOWN sintonia  BACK sale", C_FG, C_BG);
}

void ui_aprs_screen(const char *callsign, uint8_t ssid, const char *path, const char *fcs){
    char buf[48];
    lcd_fill(C_BG);
    lcd_fill_rect(0, 0, LCD_W, 16, C_FG);
    lcd_text(4, 4, "APRS DETALLE", C_BG, C_FG);
    snprintf(buf, sizeof buf, "%s-%d", callsign, (int)ssid);
    ui_text_scaled(8, 32, buf, 2, C_ACC, C_BG);
    snprintf(buf, sizeof buf, "Path: %s", path);
    lcd_text(8, 80, buf, C_FG, C_BG);
    snprintf(buf, sizeof buf, "FCS Check: %s", fcs);
    lcd_text(8, 104, buf, C_FG, C_BG);
    lcd_text(8, 136, "Call Locate: GPS -> APRS", C_FG, C_BG);
    lcd_text(8, 160, "Simbolo: radioaficionado", C_GRY, C_BG);
    lcd_text(8, 192, "APRS TX por AFSK: pendiente", C_GRY, C_BG);
    lcd_text(4, 292, "BACK vuelve", C_FG, C_BG);
}

void ui_editor_screen(const char *title, const char *buffer, int cursor){
    lcd_fill(C_BG);
    lcd_fill_rect(0, 0, LCD_W, 16, C_FG);
    lcd_text(4, 4, title, C_BG, C_FG);
    char buf[24];
    snprintf(buf, sizeof buf, "%s", buffer);
    ui_text_scaled(16, 96, buf, 2, C_FG, C_BG);
    /* Subrayado bajo el caracter del cursor (cada char 12 px a 2x). */
    lcd_fill_rect(16 + cursor * 12, 124, 10, 3, C_ACC);
    lcd_text(8, 168, "UP/DOWN cambia letra", C_FG, C_BG);
    lcd_text(8, 184, "L/R mueve cursor", C_FG, C_BG);
    lcd_text(4, 292, "OK guarda  BACK cancela", C_FG, C_BG);
}

void ui_alarm_screen(const char *callsign){
    char buf[48];
    lcd_fill(C_BG);
    lcd_fill_rect(0, 0, LCD_W, 16, C_RED);
    lcd_text(4, 4, "ALARMA", LCD_WHITE, C_RED);
    ui_text_scaled(8, 48, "EMERGENCIA", 2, C_RED, C_BG);
    snprintf(buf, sizeof buf, "%s pide ayuda", callsign);
    lcd_text(8, 96, buf, C_FG, C_BG);
    lcd_text(8, 128, "TX tono de alarma", C_FG, C_BG);
    lcd_text(8, 152, "APRS: ! Emergency", C_GRY, C_BG);
    lcd_text(4, 292, "Cualquier tecla para", C_FG, C_BG);
}

void ui_msg_screen(const char *line1, const char *line2){
    lcd_fill(C_BG);
    lcd_fill_rect(0, 0, LCD_W, 16, C_FG);
    lcd_text(4, 4, "MENSAJE", C_BG, C_FG);
    if(line1) ui_text_scaled(8, 80, line1, 2, C_FG, C_BG);
    if(line2) lcd_text(8, 128, line2, C_GRY, C_BG);
    lcd_text(4, 292, "OK confirma  BACK cancela", C_FG, C_BG);
}

void ui_apo_warn_screen(uint32_t seconds_left){
    char buf[24];
    lcd_fill(C_BG);
    lcd_fill_rect(0, 0, LCD_W, 16, C_RED);
    lcd_text(4, 4, "APO", LCD_WHITE, C_RED);
    /* "Power Off!" es el mensaje del firmware original V1.16. */
    ui_text_scaled(8, 48, "Power Off!", 2, C_RED, C_BG);
    snprintf(buf, sizeof buf, "Apagado en %lu s", (unsigned long)seconds_left);
    lcd_text(8, 104, buf, C_FG, C_BG);
    lcd_text(8, 128, "Sin actividad: el radio", C_GRY, C_BG);
    lcd_text(8, 144, "se apaga solo (APO).", C_GRY, C_BG);
    lcd_text(4, 292, "Cualquier tecla cancela", C_FG, C_BG);
}

void ui_modem_screen(uint8_t proto){
    static const char *NAMES[] = {"OFF", "DMR", "NXDN", "P25"};
    lcd_fill(C_BG);
    lcd_fill_rect(0, 0, LCD_W, 16, C_FG);
    lcd_text(4, 4, "MODO MODEM", C_BG, C_FG);
    ui_text_scaled(8, 32, (char *)NAMES[proto > 3 ? 0 : proto], 3, C_ACC, C_BG);
    lcd_text(8, 88, "Nodo MMDVM (no terminal)", C_FG, C_BG);
    lcd_text(8, 112, "Squelch abierto", C_FG, C_BG);
    lcd_text(8, 128, "CTCSS/DCS off  WIDE", C_FG, C_BG);
    lcd_text(8, 152, "Audio plano: DISC / MOD", C_FG, C_BG);
    lcd_text(8, 176, "PTT + COS al modem", C_GRY, C_BG);
    lcd_text(8, 200, "Ver docs/DMR-CONVERSION.md", C_GRY, C_BG);
    lcd_text(4, 292, "L/R protocolo  BACK sale", C_FG, C_BG);
}
