#include "ui.h"
#include <stdio.h>
#include <string.h>

const char *UI_MODE_NAMES[5] = {"FM", "AM", "USB", "LSB", "CW"};
const char *UI_POWER_NAMES[3] = {"BAJA", "MEDIA", "ALTA"};

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
    lcd_text(x, y, "S", LCD_WHITE, LCD_BLACK);
    for(int i = 0; i < 10; i++){
        uint16_t c = LCD_BLACK;
        if(i < s) c = (i < 7) ? LCD_GREEN : (i < 9) ? UI_YELLOW : LCD_RED;
        lcd_fill_rect(x + 10 + i * seg, y + 1, seg - 2, 6,
                      i < s ? c : UI_DARK);
    }
    char buf[8];
    snprintf(buf, sizeof buf, "S%d", s > 9 ? 9 : s);
    lcd_text(x + 10 + 10 * seg + 2, y, buf, LCD_WHITE, LCD_BLACK);
}

static void fmt_freq(char *out, size_t n, uint32_t hz){
    snprintf(out, n, "%lu.%04lu", (unsigned long)(hz / 1000000),
             (unsigned long)((hz % 1000000) / 100));
}

static void status_bar(const ui_state_t *st){
    lcd_fill_rect(0, 0, LCD_W, 16, LCD_WHITE);
    lcd_text(4, 4, st->callsign, LCD_BLACK, LCD_WHITE);
    if(st->gps_fix){
        char buf[16];
        snprintf(buf, sizeof buf, "%02d:%02d", st->gps_h, st->gps_m);
        lcd_text(200, 4, buf, LCD_BLACK, LCD_WHITE);
        lcd_text(150, 4, "GPS", LCD_GREEN, LCD_WHITE);
    } else {
        lcd_text(150, 4, "SIN GPS", LCD_RED, LCD_WHITE);
    }
    if(st->tx) lcd_text(100, 4, "TX", LCD_RED, LCD_WHITE);
    else lcd_text(100, 4, "RX", LCD_BLACK, LCD_WHITE);
}

void ui_splash(const char *callsign, const char *version){
    lcd_fill(LCD_BLACK);
    ui_text_scaled(30, 96, "RT-1000Pro", 3, LCD_WHITE, LCD_BLACK);
    lcd_text(84, 136, "OPEN FIRMWARE", LCD_GREEN, LCD_BLACK);
    ui_text_scaled(84, 168, callsign, 2, UI_CYAN, LCD_BLACK);
    char buf[32];
    snprintf(buf, sizeof buf, "FW %s", version);
    lcd_text(96, 200, buf, LCD_WHITE, LCD_BLACK);
    lcd_text(60, 232, "donttellmeimcute", UI_GRAY, LCD_BLACK);
}

void ui_main(const ui_state_t *st){
    char buf[40];
    lcd_fill(LCD_BLACK);
    status_bar(st);

    /* VFO A: grande. */
    uint16_t col_a = st->active == 0 ? LCD_GREEN : LCD_WHITE;
    char lab_a[4] = "A";
    ui_text_scaled(8, 24, lab_a, 2, col_a, LCD_BLACK);
    lcd_text(200, 28, UI_MODE_NAMES[st->mode], LCD_WHITE, LCD_BLACK);
    fmt_freq(buf, sizeof buf, st->vfo_a_hz);
    ui_text_scaled(8, 48, buf, 3, st->active == 0 ? LCD_WHITE : UI_GRAY, LCD_BLACK);
    lcd_text(180, 52, "MHz", LCD_WHITE, LCD_BLACK);
    snprintf(buf, sizeof buf, "STEP %lu.%01luk  SQL %d  PWR %s",
             (unsigned long)(st->step_hz / 1000),
             (unsigned long)((st->step_hz % 1000) / 100),
             st->sql, UI_POWER_NAMES[st->power]);
    lcd_text(8, 88, buf, LCD_WHITE, LCD_BLACK);
    ui_smeter(8, 104, 200, st->active == 0 ? st->rssi_dbm : -120);

    ui_hline(0, 124, LCD_W, UI_DARK);

    /* VFO B: compacto. */
    uint16_t col_b = st->active == 1 ? LCD_GREEN : LCD_WHITE;
    ui_text_scaled(8, 136, "B", 2, col_b, LCD_BLACK);
    fmt_freq(buf, sizeof buf, st->vfo_b_hz);
    ui_text_scaled(8, 160, buf, 2, st->active == 1 ? LCD_WHITE : UI_GRAY, LCD_BLACK);
    lcd_text(180, 164, "MHz", LCD_WHITE, LCD_BLACK);
    lcd_text(8, 188, UI_MODE_NAMES[st->mode], LCD_WHITE, LCD_BLACK);
    ui_smeter(8, 204, 200, st->active == 1 ? st->rssi_dbm : -120);

    ui_hline(0, 280, LCD_W, UI_DARK);
    lcd_text(4, 292, "OK MENU V/M VFO # GPS ABC CANAL", LCD_WHITE, LCD_BLACK);
}

void ui_entry(const ui_state_t *st, const char *buffer){
    ui_main(st);
    lcd_fill_rect(8, 128, 224, 72, LCD_WHITE);
    lcd_text(16, 136, "ENTRAR FREC. MHz:", LCD_BLACK, LCD_WHITE);
    char buf[24];
    snprintf(buf, sizeof buf, "%s_", buffer);
    ui_text_scaled(16, 156, buf, 2, LCD_BLACK, LCD_WHITE);
    lcd_text(16, 184, "OK confirma  BACK cancela", LCD_BLACK, LCD_WHITE);
}

void ui_menu(const char *labels[], const char *values[], int count, int selected){
    lcd_fill(LCD_BLACK);
    lcd_fill_rect(0, 0, LCD_W, 16, LCD_WHITE);
    lcd_text(4, 4, "MENU", LCD_BLACK, LCD_WHITE);
    for(int i = 0; i < count; i++){
        uint16_t y = 24 + i * 24;
        if(i == selected){
            lcd_fill_rect(0, y, LCD_W, 22, LCD_WHITE);
            lcd_text(8, y + 7, labels[i], LCD_BLACK, LCD_WHITE);
            if(values[i]) lcd_text(150, y + 7, values[i], LCD_BLACK, LCD_WHITE);
        } else {
            lcd_text(8, y + 7, labels[i], LCD_WHITE, LCD_BLACK);
            if(values[i]) lcd_text(150, y + 7, values[i], LCD_GREEN, LCD_BLACK);
        }
    }
    lcd_text(4, 292, "UP/DOWN mueve  L/R cambia  OK entra", LCD_WHITE, LCD_BLACK);
}

void ui_gps_screen(const ui_state_t *st){
    char buf[40];
    lcd_fill(LCD_BLACK);
    lcd_fill_rect(0, 0, LCD_W, 16, LCD_WHITE);
    lcd_text(4, 4, "GPS", LCD_BLACK, LCD_WHITE);
    if(st->gps_fix){
        snprintf(buf, sizeof buf, "%02d:%02d:%02d", st->gps_h, st->gps_m, st->gps_s);
        ui_text_scaled(8, 32, buf, 3, LCD_WHITE, LCD_BLACK);
        snprintf(buf, sizeof buf, "LAT %.4f", st->gps_lat);
        ui_text_scaled(8, 80, buf, 2, LCD_GREEN, LCD_BLACK);
        snprintf(buf, sizeof buf, "LON %.4f", st->gps_lon);
        ui_text_scaled(8, 108, buf, 2, LCD_GREEN, LCD_BLACK);
        snprintf(buf, sizeof buf, "ALT %.0f m  SAT %d", st->gps_alt_m, st->gps_sats);
        lcd_text(8, 140, buf, LCD_WHITE, LCD_BLACK);
    } else {
        ui_text_scaled(8, 32, "SIN FIX", 3, LCD_RED, LCD_BLACK);
        lcd_text(8, 80, "Buscando satelites...", LCD_WHITE, LCD_BLACK);
        lcd_text(8, 96, "GPS en USART3 9600", UI_GRAY, LCD_BLACK);
    }
    snprintf(buf, sizeof buf, "APRS %s: %s", st->callsign, st->aprs_state);
    lcd_text(8, 180, buf, LCD_WHITE, LCD_BLACK);
    lcd_text(8, 196, "APRS TX por AFSK: pendiente", UI_GRAY, LCD_BLACK);
    lcd_text(4, 292, "BACK vuelve", LCD_WHITE, LCD_BLACK);
}

void ui_chan_screen(const rt1000_chan_t *chans, const bool *used, int count, int selected){
    char buf[40];
    lcd_fill(LCD_BLACK);
    lcd_fill_rect(0, 0, LCD_W, 16, LCD_WHITE);
    lcd_text(4, 4, "CANALES", LCD_BLACK, LCD_WHITE);
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
            lcd_fill_rect(0, y, LCD_W, 24, LCD_WHITE);
            lcd_text(8, y + 8, buf, LCD_BLACK, LCD_WHITE);
        } else {
            lcd_text(8, y + 8, buf, used[i] ? LCD_WHITE : UI_GRAY, LCD_BLACK);
        }
    }
    lcd_text(4, 292, "OK recupera  ABC guarda  BACK sale", LCD_WHITE, LCD_BLACK);
}

void ui_info_screen(const char *callsign, const char *version){
    lcd_fill(LCD_BLACK);
    lcd_fill_rect(0, 0, LCD_W, 16, LCD_WHITE);
    lcd_text(4, 4, "INFO", LCD_BLACK, LCD_WHITE);
    char buf[40];
    snprintf(buf, sizeof buf, "RT-1000Pro open %s", version);
    lcd_text(8, 32, buf, LCD_WHITE, LCD_BLACK);
    snprintf(buf, sizeof buf, "Callsign: %s", callsign);
    lcd_text(8, 56, buf, LCD_GREEN, LCD_BLACK);
    lcd_text(8, 88, "HW: SIN CONFIRMAR", LCD_RED, LCD_BLACK);
    lcd_text(8, 104, "MCU ?  BK4829 ?  LCD ?", UI_GRAY, LCD_BLACK);
    lcd_text(8, 136, "Flashear tests 3 y 4", LCD_WHITE, LCD_BLACK);
    lcd_text(8, 152, "antes que la app.", LCD_WHITE, LCD_BLACK);
    lcd_text(4, 292, "BACK vuelve", LCD_WHITE, LCD_BLACK);
}
