/* Emulador host del display ST7789V 240x320 del RT-1000Pro.
 *
 * Renderiza las pantallas reales de app.c usando el driver LCD
 * de verdad (misma fuente 5x7, mismo framebuffer RGB565) y
 * vuelca cada pantalla a PPM. NO es el radio: es el driver
 * real corriendo en host, con los mismos strings y coordenadas
 * que ui_draw_main() en app.c. Sirve para ver como se vera la
 * UI antes de flashear el hardware real.
 *
 *   gcc -std=c11 -Wall -Wextra -Os -DRT1000_HOST_TEST=1 -Isrc \
 *       tests/emu.c src/display.c src/hal_mock.c -o build/emu
 *   ./build/emu   # deja *.ppm en emu-out/
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "display.h"

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

/* Replica exacta de ui_draw_main() en app.c (mismos strings,
 * mismas coordenadas, mismos colores). gps_fix=1 pinta la
 * linea GPS con fix; gps_fix=0 pinta "GPS sin fix". */
static void ui_main(int gps_fix, unsigned long fa_hz, unsigned long fb_hz){
    char line[64];
    lcd_fill(LCD_BLACK);
    lcd_text(4, 4, "RT-1000Pro open", LCD_WHITE, LCD_BLACK);
    snprintf(line, sizeof line, "A %lu.%03lu MHz",
             fa_hz / 1000000, (fa_hz % 1000000) / 1000);
    lcd_text(4, 20, line, LCD_GREEN, LCD_BLACK);
    snprintf(line, sizeof line, "B %lu.%03lu MHz",
             fb_hz / 1000000, (fb_hz % 1000000) / 1000);
    lcd_text(4, 36, line, LCD_GREEN, LCD_BLACK);
    if(gps_fix){
        snprintf(line, sizeof line, "GPS %02d:%02d:%02d %.4f %.4f",
                 9, 30, 0, 19.5145, -98.8756);
        lcd_text(4, 52, line, LCD_WHITE, LCD_BLACK);
    } else {
        lcd_text(4, 52, "GPS sin fix", LCD_RED, LCD_BLACK);
    }
    lcd_text(4, 300, "MENU: OK  PTT: TX", LCD_WHITE, LCD_BLACK);
}

int main(void){
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

    /* 2: pantalla principal al encender (sin fix GPS). */
    ui_main(0, 146520000UL, 435000000UL);
    dump_ppm("emu-out/02-principal.ppm");

    /* 3: principal con fix GPS (Texcoco). */
    ui_main(1, 146520000UL, 435000000UL);
    dump_ppm("emu-out/03-gps-fix.ppm");

    /* 4: principal con OK pulsado (menu placeholder). */
    ui_main(0, 146520000UL, 435000000UL);
    lcd_text(4, 68, "MENU (por implementar)", LCD_WHITE, LCD_BLACK);
    dump_ppm("emu-out/04-menu.ppm");

    /* 5: principal tras pulsar Up una vez (+12.5 kHz en VFO A). */
    ui_main(0, 146532500UL, 435000000UL);
    dump_ppm("emu-out/05-vfo-up.ppm");

    return 0;
}
