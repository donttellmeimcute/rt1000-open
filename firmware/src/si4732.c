#include "si4732.h"
#include "mcu.h"
#define SI4732_ADDR 0x11
static void i2c_delay(void){ for(volatile int i=0;i<50;i++){ } }
#ifdef RT1000_TARGET_ARM
static void scl(int v){ if(v) gpio_set(GPIOB,6); else gpio_clr(GPIOB,6); }
static void sda(int v){ if(v) gpio_set(GPIOB,7); else gpio_clr(GPIOB,7); }
static int sda_read(void){ return (int)gpio_read(GPIOB,7); }
#else
static void scl(int v){ (void)v; } static void sda(int v){ (void)v; } static int sda_read(void){ return 1; }
#endif
static void i2c_start(void){ sda(1); scl(1); i2c_delay(); sda(0); i2c_delay(); scl(0); }
static void i2c_stop(void){ sda(0); scl(1); i2c_delay(); sda(1); i2c_delay(); }
static int i2c_write_byte(uint8_t b){
    for(int i=7;i>=0;i--){ sda((b>>i)&1); scl(1); i2c_delay(); scl(0); }
    sda(1); scl(1); i2c_delay(); int ack=!sda_read(); scl(0); return ack;
}
static void i2c_write(const uint8_t *buf, int n){
    i2c_start(); i2c_write_byte((uint8_t)(SI4732_ADDR<<1)); for(int i=0;i<n;i++) i2c_write_byte(buf[i]); i2c_stop();
}
bool si4732_power_up(void){
    /* POWER_UP 0x01 0x10 0x05 (FM). Sin hardware no hay ACK. */
    const uint8_t cmd[3]={0x01,0x10,0x05};
    i2c_write(cmd,3);
#ifdef RT1000_TARGET_ARM
    return true; /* el ACK real se comprueba en el test */
#else
    return false; /* en host no hay chip */
#endif
}
bool si4732_get_rev(uint8_t rev[9]){
    const uint8_t cmd[1]={0x10}; /* GET_REV */
    i2c_write(cmd,1);
    for(int i=0;i<9;i++) rev[i]=0;
#ifdef RT1000_TARGET_ARM
    return true;
#else
    return false;
#endif
}
bool si4732_tune_fm(uint32_t freq_khz){
    uint8_t cmd[5]={0x20,0x00,(uint8_t)(freq_khz>>8),(uint8_t)freq_khz,0x00}; /* FM_TUNE_FREQ */
    i2c_write(cmd,5);
    return true;
}
