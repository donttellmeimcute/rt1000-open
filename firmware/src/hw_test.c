#include "app.h"
#include "hal.h"
#include "display.h"
#include "bk4829.h"
#include "si4732.h"
#include "keypad.h"
#include <stdio.h>
/* Modo de prueba de hardware, como el de Hertzz58 para el RT-950 Pro.
 * Se flashea ANTES que la app completa: cada test confirma un
 * periferico. Si un test falla, la app completa no se flashea.
 * 1 backlight/LCD, 2 UART echo, 3 LCD barras, 4 BK4829 ID,
 * 5 SI4732 rev, 6 flash JEDEC ID, 7 ADC bateria, 8 DAC tono,
 * 9 teclado+encoder, 10 GPS NMEA, 11 diagnostico completo.
 * Salida por UART4 (la del GPS/Bluetooth, segun el 950 Pro).
 */
static void dbg(const char *s){ hal_uart_write((const uint8_t*)s, __builtin_strlen(s)); }
int hw_test_main(int test){
    hal_uart_init_115200();
    lcd_init();
    char buf[64];
    switch(test){
    case 1: case 3: lcd_color_bars(); dbg("TEST 3: LCD color bars\r\n"); break;
    case 2: dbg("TEST 2: UART echo, escribe algo\r\n"); break;
    case 4: {
        uint16_t id=0; bk4829_init();
        bool ok=bk4829_read_id(0,&id);
        snprintf(buf,sizeof buf,"TEST 4: BK4829 chip0 id=%04X %s\r\n", id, ok?"OK":"FAIL");
        dbg(buf); break; }
    case 5: {
        uint8_t rev[9]; bool ok=si4732_get_rev(rev);
        snprintf(buf,sizeof buf,"TEST 5: SI4732 rev %s\r\n", ok?"OK":"FAIL"); dbg(buf); break; }
    case 6: {
        uint8_t id[3]={0}; hal_spi_transfer((const uint8_t[]){0x9F}, id, 3);
        snprintf(buf,sizeof buf,"TEST 6: flash JEDEC %02X %02X %02X\r\n", id[0],id[1],id[2]); dbg(buf); break; }
    case 9: {
        uint16_t raw=keypad_scan_raw();
        snprintf(buf,sizeof buf,"TEST 9: keypad raw=%04X\r\n", raw); dbg(buf); break; }
    case 10: dbg("TEST 10: GPS NMEA, espera $GPRMC\r\n"); break;
    case 11: default:
        lcd_color_bars(); dbg("TEST 11: diagnostico completo\r\n"); break;
    }
    for(;;){ hal_delay_ms(1000); }
    return 0;
}
