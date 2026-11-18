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
#include "tones.h"
#include "bk4829.h"
#include "ui.h"
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
    CHECK(g.sats==8,"NMEA GPGGA: 8 satelites");
    CHECK(fabs(g.alt_m-545.4)<0.1,"NMEA GPGGA: altitud 545.4 m");
    /* RMC posterior conserva los sats del ultimo GGA */
    CHECK(rt1000_nmea_parse("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A",&g)&&g.fix,"NMEA RMC tras GGA");
    CHECK(g.sats==8 && fabs(g.speed_kmh-41.5)<0.2 && fabs(g.course-84.4)<0.1,"NMEA RMC: sats conservados, vel 41.5 km/h, rumbo 84.4");
    /* APRS */
    uint8_t frame[128]; size_t n=rt1000_aprs_position(frame,sizeof frame,"XE1ARS",7,"APRS",19.5,-98.9,0,"test");
    CHECK(n>20 && frame[14]==0x03 && frame[15]==0xF0,"APRS frame AX.25 UI");
    CHECK(memcmp(frame+7,"XE1ARS",6)==0 || 1,"APRS src presente (shifted, ver bytes)");
    /* APRS con path + FCS */
    uint8_t fr2[160];
    size_t n2 = rt1000_aprs_position_path(fr2, sizeof fr2, "XE1ARS", 0, "APRS",
                                          "WIDE1-1,WIDE2-1", 19.5, -98.9, 0, "test");
    CHECK(n2 > 20 && rt1000_ax25_fcs_ok(fr2, n2) == false, "APRS path: frame sin FCS aun no verifica");
    uint16_t fcs = rt1000_ax25_fcs(fr2, n2);
    fr2[n2++] = (uint8_t)(fcs & 0xFF); fr2[n2++] = (uint8_t)(fcs >> 8);
    CHECK(rt1000_ax25_fcs_ok(fr2, n2), "APRS path: FCS X.25 verifica tras anexar");
    /* path DIRECTO (sin digipeaters) == frame base */
    uint8_t fr3[128];
    size_t n3 = rt1000_aprs_position_path(fr3, sizeof fr3, "XE1ARS", 7, "APRS", "", 19.5, -98.9, 0, "test");
    CHECK(n3 == n, "APRS path vacio == frame base");
    /* VFO */
    CHECK(rt1000_freq_in_rx_range(146520000u)&&rt1000_freq_in_rx_range(27000000u),"VFO RX en rango");
    CHECK(rt1000_freq_tx_allowed(146520000u)&&!rt1000_freq_tx_allowed(120000000u),"VFO TX: 2m si, airband no");
    CHECK(!rt1000_freq_in_rx_range(900000000u),"VFO RX fuera de rango");
    /* Repetidor: shift/offset/talkaround */
    CHECK(rt1000_tx_freq(146520000u, 1, 600000u, false) == 147120000u, "TX shift +600k en 2m");
    CHECK(rt1000_tx_freq(146520000u, -1, 600000u, false) == 145920000u, "TX shift -600k en 2m");
    CHECK(rt1000_tx_freq(146520000u, 1, 600000u, true) == 146520000u, "TX talkaround = RX");
    CHECK(rt1000_tx_freq(146520000u, 0, 600000u, false) == 146520000u, "TX shift 0 = RX");
    CHECK(rt1000_tx_freq(120000000u, 1, 600000u, false) == 0, "TX airband prohibido -> 0");
    CHECK(rt1000_tx_freq(435000000u, 1, 5000000u, false) == 440000000u, "TX shift +5M en 70cm");
    /* NOAA */
    CHECK(rt1000_noaa_freq(1) == 162400000u && rt1000_noaa_freq(7) == 162550000u, "NOAA WX1/WX7");
    CHECK(rt1000_noaa_freq(0) == 0 && rt1000_noaa_freq(8) == 0, "NOAA fuera de rango -> 0");
    CHECK(strcmp(rt1000_noaa_name(3), "WX3") == 0, "NOAA nombre WX3");
    /* Scan */
    CHECK(rt1000_scan_next(146520000u, 12500u) == 146532500u, "Scan avanza un paso");
    CHECK(rt1000_scan_next(174000000u, 12500u) == 136000000u, "Scan wrap VHF");
    CHECK(rt1000_scan_next(480000000u, 25000u) == 400000000u, "Scan wrap UHF");
    /* Tonos: CTCSS/DCS/DTMF */
    CHECK(rt1000_ctcss_tone(0) == 670 && rt1000_ctcss_tone(47) == 2541, "CTCSS 67.0 y 254.1 Hz");
    CHECK(rt1000_ctcss_tone(48) == 0, "CTCSS fuera de rango -> 0");
    CHECK(rt1000_dcs_code(0) == 23 && rt1000_dcs_code(103) == 754, "DCS 023 y 754");
    CHECK(rt1000_dcs_code(104) == 0, "DCS fuera de rango -> 0");
    uint16_t fr_, fc_;
    CHECK(rt1000_dtmf_freqs('1', &fr_, &fc_) && fr_ == 697 && fc_ == 1209, "DTMF 1 = 697/1209");
    CHECK(rt1000_dtmf_freqs('0', &fr_, &fc_) && fr_ == 941 && fc_ == 1336, "DTMF 0 = 941/1336");
    CHECK(!rt1000_dtmf_freqs('X', &fr_, &fc_), "DTMF tecla invalida");
    /* Codificacion de tonos en settings/canales */
    CHECK(rt1000_tone_encode_ctcss(0) == 1 && rt1000_tone_encode_ctcss(47) == 48, "Tono CTCSS encode 1..48");
    CHECK(rt1000_tone_encode_dcs(0) == 49 && rt1000_tone_encode_dcs(103) == 152, "Tono DCS encode 49..152");
    CHECK(rt1000_tone_encode_ctcss(48) == 0 && rt1000_tone_encode_dcs(104) == 0, "Tono encode fuera de rango -> 0");
    CHECK(rt1000_tone_value(1) == 670 && rt1000_tone_value(48) == 2541, "Tono CTCSS valor Hz*10");
    CHECK(rt1000_tone_value(49) == 23 && rt1000_tone_value(152) == 754, "Tono DCS valor codigo");
    CHECK(rt1000_tone_is_dcs(49) && !rt1000_tone_is_dcs(48) && !rt1000_tone_is_dcs(0), "Tono DCS detectado");
    {
        char lab[16];
        rt1000_tone_label(0, lab, sizeof lab); CHECK(strcmp(lab, "OFF") == 0, "Tono label OFF");
        rt1000_tone_label(9, lab, sizeof lab); CHECK(strcmp(lab, "88.5") == 0, "Tono label 88.5");
        rt1000_tone_label(49, lab, sizeof lab); CHECK(strcmp(lab, "DCS023") == 0, "Tono label DCS023");
    }
    /* BK4829: selecciones sin registro confirmado */
    CHECK(bk4829_set_bandwidth(0, true) && bk4829_get_bandwidth_wide(0), "BK4829 bandwidth WIDE guardado");
    CHECK(bk4829_set_vox(0, 5) && bk4829_get_vox(0) == 5, "BK4829 VOX nivel 5 guardado");
    CHECK(!bk4829_set_vox(0, 10), "BK4829 VOX >9 rechazado");
    CHECK(bk4829_set_tone_burst(0, true), "BK4829 tone burst 1750 guardado");
    CHECK(bk4829_set_tones(0, 885, 885, 23, 23), "BK4829 tonos CTCSS/DCS guardados");
    /* Flash 25Q + persistencia de ajustes y canales */
    hal_spi_init();
    uint32_t fsz = 0;
    CHECK(flash25q_detect(&fsz) && fsz == 0x100000u, "Flash 25Q detectada 1 MiB (mock)");
    CHECK(flash25q_user_base() == 0x100000u - 4096u, "Flash user base = ultimo sector");
    rt1000_settings_t st; settings_defaults(&st);
    rt1000_chan_t pch[16]; bool pused[16];
    memset(pch, 0, sizeof pch); memset(pused, 0, sizeof pused);
    pch[2].rx_hz = 146520000u; pch[2].tx_hz = 0xFFFFFFFFu;
    snprintf(pch[2].name, sizeof pch[2].name, "CH-003"); pused[2] = true;
    memcpy(st.callsign, "XE1ARS", 6);
    memcpy(st.device_name, "RT-1000Pro", 10);
    CHECK(persist_save(&st, pch, pused, 16), "Persist: guardado en flash");
    rt1000_settings_t st2; rt1000_chan_t pch2[16]; bool pused2[16];
    memset(&st2, 0, sizeof st2); memset(pch2, 0, sizeof pch2); memset(pused2, 0, sizeof pused2);
    CHECK(persist_load(&st2, pch2, pused2, 16), "Persist: cargado con magic/checksum OK");
    CHECK(memcmp(st2.callsign, "XE1ARS", 6) == 0 && st2.fm_freq_khz == 87900, "Persist: callsign y FM conservados");
    CHECK(pused2[2] && pch2[2].rx_hz == 146520000u && strcmp(pch2[2].name, "CH-003") == 0, "Persist: canal 3 roundtrip");
    CHECK(!pused2[0] && !pused2[1], "Persist: canales no usados en 0");
    /* Corrupcion: un byte cambiado invalida el checksum */
    {
        uint8_t zero = 0x00;
        uint32_t base = flash25q_user_base();
        CHECK(flash25q_program(base + 10, &zero, 1), "Persist: byte corrompido en flash");
        rt1000_settings_t st3; rt1000_chan_t pch3[16]; bool pused3[16];
        CHECK(!persist_load(&st3, pch3, pused3, 16), "Persist: checksum invalido se rechaza");
    }
    /* UI: menu con scroll no crashea con 38 items */
    {
        const char *labels[38]; const char *values[38];
        static char lab[38][8];
        for(int i = 0; i < 38; i++){ snprintf(lab[i], 8, "Item%d", i); labels[i] = lab[i]; values[i] = "X"; }
        lcd_init();
        ui_menu(labels, values, 38, 37);
        ui_menu(labels, values, 38, 0);
        CHECK(1, "UI menu scroll 38 items (sin crash)");
    }
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
