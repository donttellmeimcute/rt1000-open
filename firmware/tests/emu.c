/* Emulador host del display ST7789V 240x320 del RT-1000Pro.
 *
 * Renderiza las pantallas reales de la app usando el driver LCD
 * y la capa UI de verdad (ui.c), con valores demo documentados,
 * y vuelca cada pantalla a PPM (+ PNG con `make emu`).
 * NO es el radio: es el driver y la UI reales corriendo en host.
 *
 *   make emu   # deja *.png en emu-out/
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "display.h"
#include "ui.h"

extern const uint16_t *lcd_fb(void);

static void dump_ppm(const char *path){
    FILE *f = fopen(path, "wb");
    const uint16_t *fb = lcd_fb();
    if(!f){ fprintf(stderr, "emu: no puedo escribir %s\n", path); return; }
    fprintf(f, "P6\n%d %d\n255\n", LCD_W, LCD_H);
    for(int i = 0; i < LCD_W * LCD_H; i++){
        uint16_t c = fb[i];
        uint8_t rgb[3] = {
            (uint8_t)(((c >> 11) & 0x1F) * 255 / 31),
            (uint8_t)(((c >> 5) & 0x3F) * 255 / 63),
            (uint8_t)((c & 0x1F) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("escrito %s\n", path);
}

static void demo_state(ui_state_t *s, int gps_fix, int16_t rssi){
    memset(s, 0, sizeof *s);
    s->vfo_a_hz = 146520000u;
    s->vfo_b_hz = 435000000u;
    s->active = 0;
    s->mode = 0;      /* FM */
    s->power = 2;     /* ALTA */
    s->sql = 2;
    s->step_hz = 12500;
    s->rssi_dbm = rssi;
    s->tx = false;
    s->gps_fix = gps_fix;
    s->gps_h = 9; s->gps_m = 30; s->gps_s = 0;
    s->gps_lat = 19.5145; s->gps_lon = -98.8756;
    s->gps_alt_m = 2250; s->gps_sats = 8;
    s->speed_kmh = 41.5; s->course = 84.4;
    s->gps_fmt = 0;
    s->callsign = "XE1ARS";
    s->device_name = "RT-1000Pro";
    s->aprs_state = "60s";
    s->monitor = false; s->scan = false;
    s->batt_mv = 0; /* sin ADC en host */
}

int main(void){
    ui_state_t st;
    lcd_init();

    /* 0: prueba de fuente: todo el ASCII imprimible 0x20-0x7E. */
    {
        char line[41];
        lcd_fill(LCD_BLACK);
        for(int l = 0; l < 3; l++){
            int n = 0;
            for(int i = 0; i < 40 && l * 40 + i < 95; i++)
                line[n++] = (char)(0x20 + l * 40 + i);
            line[n] = 0;
            lcd_text(0, 4 + l * 12, line, LCD_WHITE, LCD_BLACK);
        }
        dump_ppm("emu-out/00-fuente.ppm");
    }

    /* 1: test de hardware 3 (barras de color). */
    lcd_color_bars();
    dump_ppm("emu-out/01-barras.ppm");

    /* 2: splash de encendido. */
    ui_splash("XE1ARS", "v1.0-open");
    dump_ppm("emu-out/02-splash.ppm");

    /* 3: principal al encender (sin fix GPS, S-meter demo). */
    demo_state(&st, 0, -87);
    ui_main(&st);
    dump_ppm("emu-out/03-principal.ppm");

    /* 4: principal con fix GPS. */
    demo_state(&st, 1, -73);
    ui_main(&st);
    dump_ppm("emu-out/04-principal-gps.ppm");

    /* 5: menu (seleccion en "Tema"). Tema KAWAII por defecto. */
    {
        static const char *labels[] = {
            "VFO activo", "Paso", "Potencia", "Modo", "Squelch",
            "GPS", "APRS beacon", "Luz pantalla", "Tema",
            "Callsign", "Nombre", "Scan", "NOAA CH", "Radio FM",
            "APRS SSID", "APRS Path", "APRS detalle",
            "DTMF", "MDC ID", "VOX", "Ancho banda", "Tone Burst",
            "CTCSS RX", "CTCSS TX",
            "Shift", "Offset", "Talkaround", "Reverse",
            "Monitor", "GPS formato", "TNC", "Zona",
            "Prioridad TX", "Multi standby",
            "Alarma", "Backup", "Reset", "Canales", "Info"};
        static const char *values[] = {
            "A", "12.5k", "ALTA", "FM", "2",
            "ON", "60s", "ON", "KAWAII",
            "XE1ARS", "RT-1000Pro", ">", "OFF", ">",
            "0", "WIDE1-1,WIDE2-1", ">",
            "1234", "0001", "OFF", "NARROW", "OFF",
            "OFF", "OFF",
            "0", "600k", "OFF", "OFF",
            "OFF", "GRADO", "OFF", "ZONA 1",
            "OFF", "OFF",
            ">", ">", ">", ">", ">"};
        ui_menu(labels, values, 38, 8);
        dump_ppm("emu-out/05-menu.ppm");
    }

    /* 6: pantalla GPS. */
    demo_state(&st, 1, -73);
    ui_gps_screen(&st);
    dump_ppm("emu-out/06-gps.ppm");

    /* 7: canales (2 demo, zona 1). */
    {
        rt1000_chan_t chans[16];
        bool used[16] = {0};
        memset(chans, 0, sizeof chans);
        chans[0].rx_hz = 146520000u; snprintf(chans[0].name, 12, "CH-001");
        chans[1].rx_hz = 435000000u; snprintf(chans[1].name, 12, "CH-002");
        used[0] = used[1] = true;
        ui_chan_screen(chans, used, 4, 0, "ZONA 1");
        dump_ppm("emu-out/07-canales.ppm");
    }

    /* 8: entrada de frecuencia. */
    demo_state(&st, 0, -87);
    ui_entry(&st, "146.53");
    dump_ppm("emu-out/08-entry.ppm");

    /* 9: principal con el tema ORIGINAL (para comparar con KAWAII). */
    ui_set_theme(0);
    demo_state(&st, 0, -87);
    ui_main(&st);
    dump_ppm("emu-out/09-principal-original.ppm");
    ui_set_theme(1);

    /* 10: radio FM (SI4732). */
    ui_fm_screen(87900, true);
    dump_ppm("emu-out/10-fm.ppm");

    /* 11: detalle APRS. */
    ui_aprs_screen("XE1ARS", 0, "WIDE1-1,WIDE2-1", "Success");
    dump_ppm("emu-out/11-aprs.ppm");

    /* 12: editor de callsign. */
    ui_editor_screen("CALLSIGN", "XE1ARS", 2);
    dump_ppm("emu-out/12-editor.ppm");

    /* 13: alarma de emergencia. */
    ui_alarm_screen("XE1ARS");
    dump_ppm("emu-out/13-alarma.ppm");

    /* 14: info (bateria sin ADC en host). */
    demo_state(&st, 1, -73);
    ui_info_screen(&st, "v1.0-open");
    dump_ppm("emu-out/14-info.ppm");

    /* 15: menu con scroll (seleccion abajo, item 30). */
    {
        static const char *labels[] = {
            "VFO activo", "Paso", "Potencia", "Modo", "Squelch",
            "GPS", "APRS beacon", "Luz pantalla", "Tema",
            "Callsign", "Nombre", "Scan", "NOAA CH", "Radio FM",
            "APRS SSID", "APRS Path", "APRS detalle",
            "DTMF", "MDC ID", "VOX", "Ancho banda", "Tone Burst",
            "CTCSS RX", "CTCSS TX",
            "Shift", "Offset", "Talkaround", "Reverse",
            "Monitor", "GPS formato", "TNC", "Zona",
            "Prioridad TX", "Multi standby",
            "Alarma", "Backup", "Reset", "Canales", "Info"};
        static const char *values[] = {
            "A", "12.5k", "ALTA", "FM", "2",
            "ON", "60s", "ON", "KAWAII",
            "XE1ARS", "RT-1000Pro", ">", "OFF", ">",
            "0", "WIDE1-1,WIDE2-1", ">",
            "1234", "0001", "OFF", "NARROW", "OFF",
            "OFF", "OFF",
            "0", "600k", "OFF", "OFF",
            "OFF", "GRADO", "OFF", "ZONA 1",
            "OFF", "OFF",
            ">", ">", ">", ">", ">"};
        ui_menu(labels, values, 38, 30);
        dump_ppm("emu-out/15-menu-scroll.ppm");
    }

    return 0;
}
