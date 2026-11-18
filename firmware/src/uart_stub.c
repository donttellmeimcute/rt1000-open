/* Stub de UART: el USART exacto y sus pines dependen del MCU
 * (por confirmar por hardware). En el firmware oficial el puerto
 * de update es 115200 8N1 con DTR. Este stub existe solo para que
 * el esqueleto compile; hay que sustituirlo por el driver real
 * (USART2 en el mapa STM32F4-like, por verificar).
 */
#include <stdint.h>
#include <stddef.h>

int uart_init_115200(void) { return -1; /* TODO: driver real */ }
int uart_write(const uint8_t *buf, size_t len) { (void)buf; (void)len; return -1; }
int uart_read_byte(uint8_t *out) { (void)out; return -1; }
