#pragma once
#include <stdint.h>
#include <stddef.h>
/* Display candidato: ST7789V 240x320, bus paralelo 8080.
 * Fuente: RT-950 Pro (Hertzz58, confirmado en hardware).
 * En el RT-1000Pro NO VERIFICADO: pines de datos PD0-PD15
 * y control son CANDIDATOS. El primer test de hardware
 * (hw_test TEST=3) pinta barras de color y confirma el
 * display antes de usarlo en la app.
 */
#define LCD_W 240
#define LCD_H 320
void lcd_init(void);
void lcd_power_off(void); /* SLPIN + DISPOFF + backlight off (ARM); negro en host */
void lcd_fill(uint16_t color);
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_text(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg);
uint8_t lcd_font_col(uint8_t c, uint8_t col); /* columna de la fuente 5x7, para texto escalado */
void lcd_color_bars(void); /* test de hardware */
#define RGB565(r,g,b) (uint16_t)(((r&0xF8)<<8)|((g&0xFC)<<3)|((b)>>3))
#define LCD_BLACK 0x0000
#define LCD_WHITE 0xFFFF
#define LCD_RED   0xF800
#define LCD_GREEN 0x07E0
#define LCD_BLUE  0x001F
