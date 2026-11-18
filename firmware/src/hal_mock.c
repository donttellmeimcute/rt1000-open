/* Mock de HAL para pruebas en host (RT1000_HOST_TEST).
 * UART: FIFO en memoria. Flash: buffer en RAM de 251904 bytes.
 * No toca hardware real.
 */
#include "hal.h"
#include <string.h>
#ifdef RT1000_HOST_TEST
static uint8_t uart_rx[4096]; static size_t rx_head, rx_tail;
static uint8_t uart_tx[65536]; static size_t tx_len;
uint8_t mock_flash[RT1000_APP_SIZE];
void mock_uart_feed(const uint8_t *buf, size_t len){
    for(size_t i=0;i<len;i++){ uart_rx[rx_tail++%sizeof uart_rx]=buf[i]; }
}
size_t mock_uart_tx_len(void){ return tx_len; }
const uint8_t *mock_uart_tx(void){ return uart_tx; }
void mock_uart_tx_clear(void){ tx_len=0; }
int hal_uart_init_115200(void){ rx_head=rx_tail=tx_len=0; memset(mock_flash,0xFF,sizeof mock_flash); return 0; }
int hal_uart_write(const uint8_t *buf, size_t len){
    if(tx_len+len>sizeof uart_tx) return -1;
    memcpy(uart_tx+tx_len,buf,len); tx_len+=len; return (int)len;
}
bool hal_uart_rx_ready(void){ return rx_head!=rx_tail; }
int hal_uart_read_byte(uint8_t *out){
    if(rx_head==rx_tail) return -1;
    *out=uart_rx[rx_head++%sizeof uart_rx]; return 0;
}
int hal_spi_init(void){ return 0; }
/* Mock de flash SPI externa W25Q80 (1 MiB, JEDEC EF 40 14).
 * Comandos: 0x9F JEDEC, 0x03 READ, 0x02 PAGE PROGRAM (AND,
 * solo 1->0, max 256 B sin cruzar pagina), 0x20 SECTOR ERASE
 * 4 KiB, 0x06 WRITE ENABLE. Sin WEL, program/erase se ignoran. */
#define MOCK_EXT_FLASH_SIZE 0x100000u
static uint8_t mock_ext_flash[MOCK_EXT_FLASH_SIZE];
static bool ext_init, ext_wel;
int hal_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len){
    if(!ext_init){ memset(mock_ext_flash, 0xFF, sizeof mock_ext_flash); ext_init = true; }
    if(!tx || len == 0){ if(rx) memset(rx, 0xFF, len); return 0; }
    if(rx) memset(rx, 0xFF, len);
    uint8_t cmd = tx[0];
    if(cmd == 0x9F && len >= 4 && rx){ rx[1] = 0xEF; rx[2] = 0x40; rx[3] = 0x14; return 0; }
    if(cmd == 0x06){ ext_wel = true; return 0; }
    if(len < 4) return 0;
    uint32_t addr = ((uint32_t)tx[1] << 16) | ((uint32_t)tx[2] << 8) | tx[3];
    if(cmd == 0x03 && rx){
        size_t n = len - 4;
        if(addr + n > sizeof mock_ext_flash) n = sizeof mock_ext_flash - addr;
        memcpy(rx + 4, mock_ext_flash + addr, n);
    } else if(cmd == 0x02 && ext_wel){
        size_t n = len - 4;
        if(addr + n > sizeof mock_ext_flash) n = sizeof mock_ext_flash - addr;
        for(size_t i = 0; i < n; i++) mock_ext_flash[addr + i] &= tx[4 + i];
        ext_wel = false;
    } else if(cmd == 0x20 && ext_wel){
        uint32_t base = addr & ~(uint32_t)0xFFF;
        if(base < sizeof mock_ext_flash)
            memset(mock_ext_flash + base, 0xFF, 4096);
        ext_wel = false;
    }
    return 0;
}
int hal_flash_erase_app(void){ memset(mock_flash,0xFF,sizeof mock_flash); return 0; }
int hal_flash_write(uint32_t addr, const uint8_t *buf, size_t len){
    if(addr<RT1000_APP_BASE || addr+len>RT1000_APP_BASE+RT1000_APP_SIZE) return -1;
    memcpy(mock_flash+(addr-RT1000_APP_BASE),buf,len); return 0;
}
void hal_delay_ms(uint32_t ms){ (void)ms; }
static uint32_t t; uint32_t hal_millis(void){ return ++t; }
int hal_adc_batt_mv(void){ return 0; } /* sin ADC en host */
static bool power_off_flag;
void hal_power_off(void){ power_off_flag = true; }
bool hal_power_off_called(void){ return power_off_flag; }
#endif
