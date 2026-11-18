#include "app.h"
#include "hal.h"
#include "clock.h"
#include "display.h"
#include "bk4829.h"
#include "si4732.h"
#include "keypad.h"
#include "nmea.h"
#include "aprs.h"
#include "vfo.h"
#include "storage.h"
#include <string.h>
#include <stdio.h>

static rt1000_vfo_t vfo_a = {146520000u, 0, 2};
static rt1000_vfo_t vfo_b = {435000000u, 0, 2};
static rt1000_gps_t gps;
static char callsign[7] = "XE1ARS"; /* configurable por menu, no quemado en flash */

static void ui_draw_main(void){
    char line[32];
    lcd_fill(LCD_BLACK);
    lcd_text(4, 4, "RT-1000Pro open", LCD_WHITE, LCD_BLACK);
    snprintf(line, sizeof line, "A %lu.%03lu MHz", (unsigned long)(vfo_a.freq_hz/1000000), (unsigned long)((vfo_a.freq_hz%1000000)/1000));
    lcd_text(4, 20, line, LCD_GREEN, LCD_BLACK);
    snprintf(line, sizeof line, "B %lu.%03lu MHz", (unsigned long)(vfo_b.freq_hz/1000000), (unsigned long)((vfo_b.freq_hz%1000000)/1000));
    lcd_text(4, 36, line, LCD_GREEN, LCD_BLACK);
    if(gps.valid){
        snprintf(line, sizeof line, "GPS %02d:%02d:%02d %.4f %.4f", gps.hour, gps.min, gps.sec, gps.lat, gps.lon);
        lcd_text(4, 52, line, LCD_WHITE, LCD_BLACK);
    } else {
        lcd_text(4, 52, "GPS sin fix", LCD_RED, LCD_BLACK);
    }
    lcd_text(4, 300, "MENU: OK  PTT: TX", LCD_WHITE, LCD_BLACK);
}

static void aprs_beacon(void){
    if(!gps.valid) return;
    uint8_t frame[128];
    size_t n = rt1000_aprs_position(frame, sizeof frame, callsign, 7, "APRS", gps.lat, gps.lon, gps.alt_m, "RT-1000Pro open");
    (void)n; /* el frame se envia por AFSK/BK4829 cuando el driver de audio este confirmado */
}

int app_main(void){
    rt1000_system_init_documented();
    hal_uart_init_115200();
    hal_spi_init();
    lcd_init();
    bk4829_init();
    ui_draw_main();
    for(;;){
        int key = keypad_get_key();
        if(key==10){ /* OK: menu (placeholder, la jerarquia completa va en menu.c) */
            lcd_text(4, 68, "MENU (por implementar)", LCD_WHITE, LCD_BLACK);
        } else if(key==20){ /* Up: subir frecuencia VFO A 12.5 kHz */
            vfo_a.freq_hz += 12500; vfo_a.freq_hz = rt1000_freq_clamp_rx(vfo_a.freq_hz);
            bk4829_set_freq(0, vfo_a.freq_hz); ui_draw_main();
        } else if(key==21){ /* Down */
            vfo_a.freq_hz -= 12500; vfo_a.freq_hz = rt1000_freq_clamp_rx(vfo_a.freq_hz);
            bk4829_set_freq(0, vfo_a.freq_hz); ui_draw_main();
        }
        /* GPS: el parser NMEA corre con los bytes que llegan por UART. */
        if(hal_uart_rx_ready()){
            uint8_t b; if(hal_uart_read_byte(&b)==0){ (void)b; }
        }
        aprs_beacon();
        hal_delay_ms(50);
    }
    return 0;
}
