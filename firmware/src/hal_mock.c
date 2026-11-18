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
int hal_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len){
    (void)tx; if(rx) memset(rx,0xFF,len); return 0; /* sin chip: devuelve 0xFF */
}
int hal_flash_erase_app(void){ memset(mock_flash,0xFF,sizeof mock_flash); return 0; }
int hal_flash_write(uint32_t addr, const uint8_t *buf, size_t len){
    if(addr<RT1000_APP_BASE || addr+len>RT1000_APP_BASE+RT1000_APP_SIZE) return -1;
    memcpy(mock_flash+(addr-RT1000_APP_BASE),buf,len); return 0;
}
void hal_delay_ms(uint32_t ms){ (void)ms; }
static uint32_t t; uint32_t hal_millis(void){ return ++t; }
#endif
