#include "bk4829.h"
#include "hal.h"
#include "mcu.h"
/* BK4829, mapa de registros del RT-950 Pro (Hertzz58, verificado
 * en V0.27 de ese radio; en el 1000Pro CANDIDATO).
 * REG_07/08: frecuencia (PLL), REG_0C: RSSI, REG_30: modo
 * (STANDBY 0x0000, RX 0xBFF1, TX 0xC1FE, AM_RX 0x0302,
 * SCAN 0xC3FA), REG_36: potencia TX, REG_47/48: AGC por modo,
 * REG_49 0x2AB2 y REG_4A 0x5430 fijos en init.
 * Pines (950 Pro): CS0=PE8, CS1=PE15, SCK=PE10, SDA=PE11
 * (bit-bang, SDA bidireccional). */
#define BK_REG_FREQ_LO 0x07
#define BK_REG_FREQ_HI 0x08
#define BK_REG_RSSI    0x0C
#define BK_REG_MODE    0x30
#define BK_REG_TXPWR   0x36
#define BK_REG_47      0x47
#define BK_REG_48      0x48
#define BK_REG_49      0x49
#define BK_REG_4A      0x4A

static void bb_clk(int v){
#ifdef RT1000_TARGET_ARM
    if(v) gpio_set(GPIOE,10); else gpio_clr(GPIOE,10);
#else
    (void)v;
#endif
}
static void bb_sda_out(int v){
#ifdef RT1000_TARGET_ARM
    gpio_mode_out(GPIOE,11);
    if(v) gpio_set(GPIOE,11); else gpio_clr(GPIOE,11);
#else
    (void)v;
#endif
}
static int bb_sda_in(void){
#ifdef RT1000_TARGET_ARM
    gpio_mode_in(GPIOE,11);
    return (int)gpio_read(GPIOE,11);
#else
    return 0;
#endif
}
static void bb_cs(uint8_t chip, int v){
#ifdef RT1000_TARGET_ARM
    if(v) gpio_set(GPIOE, chip?15:8); else gpio_clr(GPIOE, chip?15:8);
#else
    (void)chip; (void)v;
#endif
}
/* Transferencia de 16 bits: bit 15 = 1 lectura, 0 escritura;
 * bits 14-8 = registro (7 bits), bits 7-0 = dato. */
static uint16_t bk_xfer(uint8_t chip, uint16_t cmd){
    bb_cs(chip,0);
    for(int i=15;i>=0;i--){ bb_sda_out((cmd>>i)&1); bb_clk(1); bb_clk(0); }
    uint16_t out=0;
    for(int i=15;i>=0;i--){ bb_clk(1); out=(uint16_t)((out<<1)|(uint16_t)bb_sda_in()); bb_clk(0); }
    bb_cs(chip,1);
    return out;
}
static void bk_write(uint8_t chip, uint8_t reg, uint16_t val){
    (void)bk_xfer(chip, (uint16_t)(((reg&0x7F)<<8) | (val&0xFF)));
}
static uint16_t bk_read(uint8_t chip, uint8_t reg){
    return bk_xfer(chip, (uint16_t)(0x8000 | ((reg&0x7F)<<8)));
}
void bk4829_init(void){
    bb_cs(0,1); bb_cs(1,1); bb_clk(0);
    for(uint8_t c=0;c<2;c++){
        bk_write(c, BK_REG_49, 0x2AB2);
        bk_write(c, BK_REG_4A, 0x5430);
        bk_write(c, BK_REG_MODE, 0x0000); /* standby */
    }
}
bool bk4829_read_id(uint8_t chip, uint16_t *id){
    /* El BK4829 no tiene registro de ID dedicado; la presencia
     * se confirma leyendo REG_0C (RSSI) y REG_30: un chip
     * ausente devuelve 0x0000/0xFFFF en todo. */
    uint16_t r0c=bk_read(chip, BK_REG_RSSI);
    uint16_t r30=bk_read(chip, BK_REG_MODE);
    *id=r0c;
    return !(r0c==0x0000 && r30==0x0000) && !(r0c==0xFFFF && r30==0xFFFF);
}
bool bk4829_set_freq(uint8_t chip, uint32_t freq_hz){
    /* PLL BK4829: freq = (N + frac) * 10 kHz aprox; el calculo
     * exacto depende del cristal de referencia del radio
     * (sin confirmar). Se programa en pasos de 10 Hz en
     * REG_07 (low) / REG_08 (high) como en el driver del 950. */
    uint32_t f=freq_hz/10;
    bk_write(chip, BK_REG_FREQ_LO, (uint16_t)(f & 0xFFFF));
    bk_write(chip, BK_REG_FREQ_HI, (uint16_t)((f>>16) & 0xFFFF));
    return true;
}
bool bk4829_set_tx_power(uint8_t chip, uint8_t level){
    if(level>2) return false;
    bk_write(chip, BK_REG_TXPWR, (uint16_t)(level<<4));
    return true;
}
int16_t bk4829_rssi_dbm(uint8_t chip){
    uint16_t v=bk_read(chip, BK_REG_RSSI);
    return (int16_t)(v & 0xFF) - 120;
}
void bk4829_tx_enable(uint8_t chip, bool on){
    bk_write(chip, BK_REG_MODE, on ? 0xC1FE : 0xBFF1);
}

/* Selecciones de funciones sin registro confirmado (ver .h).
 * Se guardan por chip para que la UI y los tests sean
 * coherentes; NO se escribe ningun registro inventado. */
static bool cfg_wide[2];
static uint8_t cfg_vox[2];
static bool cfg_burst[2];
static uint16_t cfg_rx_ctcss[2], cfg_tx_ctcss[2], cfg_rx_dcs[2], cfg_tx_dcs[2];
bool bk4829_set_bandwidth(uint8_t chip, bool wide){
    if(chip > 1) return false;
    cfg_wide[chip] = wide; return true;
}
bool bk4829_get_bandwidth_wide(uint8_t chip){
    return chip <= 1 ? cfg_wide[chip] : false;
}
bool bk4829_set_vox(uint8_t chip, uint8_t level){
    if(chip > 1 || level > 9) return false;
    cfg_vox[chip] = level; return true;
}
uint8_t bk4829_get_vox(uint8_t chip){
    return chip <= 1 ? cfg_vox[chip] : 0;
}
bool bk4829_set_tone_burst(uint8_t chip, bool on_1750hz){
    if(chip > 1) return false;
    cfg_burst[chip] = on_1750hz; return true;
}
bool bk4829_set_tones(uint8_t chip, uint16_t rx_ctcss_x10, uint16_t tx_ctcss_x10,
                      uint16_t rx_dcs, uint16_t tx_dcs){
    if(chip > 1) return false;
    cfg_rx_ctcss[chip] = rx_ctcss_x10; cfg_tx_ctcss[chip] = tx_ctcss_x10;
    cfg_rx_dcs[chip] = rx_dcs; cfg_tx_dcs[chip] = tx_dcs; return true;
}
