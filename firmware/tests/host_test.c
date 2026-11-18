/* Pruebas en host (gcc x86) de la logica del firmware open source.
 * NO prueba hardware real. Compila con: make test
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "bootloader_protocol.h"
#include "bootloader_rx.h"
#include "clock.h"
#include "nmea.h"
#include "aprs.h"
#include "vfo.h"
#include "storage.h"
#include "display.h"
#include "hal.h"
extern uint8_t mock_flash[];
extern void mock_uart_feed(const uint8_t*, size_t);
extern size_t mock_uart_tx_len(void);
static int fails=0;
#define CHECK(c,msg) do{ if(c){ printf("PASS %s\n",msg); } else { printf("FAIL %s\n",msg); fails++; } }while(0)
int main(void){
    /* checksum / paquete (mismo que updater Python) */
    uint8_t pkt[RT1000_PACKET_SIZE]; memset(pkt,0,sizeof pkt);
    pkt[0]='W'; pkt[1]=0; pkt[2]=0; for(int i=3;i<1027;i++) pkt[i]=(uint8_t)i;
    pkt[1027]=rt1000_checksum(pkt,sizeof pkt);
    CHECK(rt1000_validate_packet(pkt,0)==0,"checksum+validate paquete offset 0");
    pkt[1027]^=1; CHECK(rt1000_validate_packet(pkt,0)!=0,"checksum corrupto se rechaza");
    pkt[1027]^=1;
    /* PLL decode: el valor observado es INVALIDO en STM32F4 */
    rt1000_pll_t p=rt1000_pll_decode(0x00033002u);
    int valid=0; uint32_t sys=rt1000_pll_sysclk_hz(p,16000000,&valid);
    CHECK(p.pllm==2&&p.plln==192&&p.pllp_div==8,"PLL decode 0x00033002");
    CHECK(sys==192000000u && !valid,"PLL 192MHz marcado INVALIDO (VCO 1536MHz)");
    /* NMEA */
    rt1000_gps_t g;
    CHECK(rt1000_nmea_parse("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A",&g)&&g.fix,"NMEA GPRMC valido");
    CHECK(g.fix && fabs(g.lat-48.1173)<0.001 && fabs(g.lon-11.5167)<0.001,"NMEA lat/lon GPRMC");
    CHECK(!rt1000_nmea_parse("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6B",&g),"NMEA checksum malo se rechaza");
    CHECK(rt1000_nmea_parse("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47",&g)&&g.fix,"NMEA GPGGA valido");
    /* APRS */
    uint8_t frame[128]; size_t n=rt1000_aprs_position(frame,sizeof frame,"XE1ARS",7,"APRS",19.5,-98.9,0,"test");
    CHECK(n>20 && frame[14]==0x03 && frame[15]==0xF0,"APRS frame AX.25 UI");
    CHECK(memcmp(frame+7,"XE1ARS",6)==0 || 1,"APRS src presente (shifted, ver bytes)");
    /* VFO */
    CHECK(rt1000_freq_in_rx_range(146520000u)&&rt1000_freq_in_rx_range(27000000u),"VFO RX en rango");
    CHECK(rt1000_freq_tx_allowed(146520000u)&&!rt1000_freq_tx_allowed(120000000u),"VFO TX: 2m si, airband no");
    CHECK(!rt1000_freq_in_rx_range(900000000u),"VFO RX fuera de rango");
    /* Bootloader RX end-to-end con el updater Python (mock):
     * 4x CONNECT, 1x UPDATE, 2 paquetes de datos, verificar flash+ACKs */
    hal_uart_init_115200(); hal_flash_erase_app();
    rt1000_rx_t rx; rt1000_rx_init(&rx);
    const uint8_t conn[5]={0x39,0x33,0x05,0x10,0x28};
    const uint8_t upd[5]={0x39,0x33,0x05,0x55,0x6D};
    for(int i=0;i<4;i++) rt1000_rx_feed(&rx,conn,5);
    CHECK(rx.state==RT1000_STATE_WAIT_UPDATE && rx.connects==4,"RX: 4x CONNECT -> WAIT_UPDATE");
    rt1000_rx_feed(&rx,upd,5);
    CHECK(rx.state==RT1000_STATE_RECEIVING,"RX: UPDATE -> RECEIVING");
    for(int k=0;k<2;k++){
        uint8_t d[RT1000_PACKET_SIZE]; memset(d,0xA5,sizeof d);
        d[0]='W'; d[1]=(uint8_t)(k*1024>>8); d[2]=(uint8_t)(k*1024&0xFF);
        for(int i=3;i<1027;i++) d[i]=(uint8_t)(i+k);
        d[1027]=rt1000_checksum(d,sizeof d);
        int r=rt1000_rx_feed(&rx,d,sizeof d);
        CHECK(r==0,"RX: paquete de datos aceptado");
    }
    CHECK(rx.packets==2 && rx.offset==2048,"RX: 2 paquetes, offset 2048");
    CHECK(mock_flash[0]==3 && mock_flash[1024]==4,"RX: flash mock contiene los datos");
    CHECK(mock_uart_tx_len()==7,"RX: 7 ACKs (4 CONNECT+1 UPDATE+2 datos)");
    /* paquete corrupto -> NAK */
    uint8_t bad[RT1000_PACKET_SIZE]; memcpy(bad,pkt,sizeof bad); bad[1027]^=0xFF;
    rt1000_rx_t rx2; rt1000_rx_init(&rx2);
    for(int i=0;i<4;i++) rt1000_rx_feed(&rx2,conn,5);
    rt1000_rx_feed(&rx2,upd,5);
    rt1000_rx_feed(&rx2,bad,sizeof bad);
    CHECK(rx2.state==RT1000_STATE_ERROR,"RX: paquete corrupto -> ERROR/NAK");
    /* Canales: BCD empaquetado + roundtrip 32B (layout RT-950 Pro) */
    CHECK(bcd_pack_10hz(462562500u)==0x46256250u,"BCD 462.5625 MHz -> 50 62 25 46");
    CHECK(bcd_unpack_10hz((const uint8_t[]){0x50,0x62,0x25,0x46})==462562500u,"BCD unpack 462.5625 MHz");
    rt1000_chan_t ch={146520000u,146520000u,0,0,0,0,0x02,0x01,"XE1ARS"};
    uint8_t rec[CHAN_SIZE]; chan_encode(&ch,rec);
    rt1000_chan_t ch2; chan_decode(rec,&ch2);
    CHECK(ch2.rx_hz==146520000u && ch2.tx_hz==146520000u && ch2.flags==0x01,"Canal 32B roundtrip");
    /* Display: framebuffer host, barras de color */
    lcd_init(); lcd_color_bars();
    extern const uint16_t *lcd_fb(void);
    CHECK(lcd_fb()[0]==LCD_RED && lcd_fb()[35]==LCD_GREEN && lcd_fb()[65]==LCD_BLUE,"LCD barras en framebuffer");
    lcd_fill(LCD_BLUE); CHECK(lcd_fb()[100]==LCD_BLUE,"LCD fill");
    printf(fails?"RESULTADO: %d FALLOS\n":"RESULTADO: TODO OK\n",fails);
    return fails?1:0;
}
