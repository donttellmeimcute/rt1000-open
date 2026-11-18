#pragma once
/* HAL abstracta RT-1000Pro / UV100.
 * Las direcciones base son las OBSERVADAS en el firmware V1.16
 * (ver docs/RE-ANALYSIS-2026-11-17.md). No son un datasheet:
 * el MCU exacto sigue sin confirmar, asi que ningun driver
 * de aqui se debe flashear sin identificacion por hardware.
 * En host (RT1000_HOST_TEST) los drivers son mocks en memoria.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Bases observadas (STM32F4 / GD32F4 compatibles) */
#define RT1000_RCC_BASE    0x40023800u
#define RT1000_GPIOA_BASE  0x40020000u
#define RT1000_GPIOB_BASE  0x40020400u
#define RT1000_GPIOC_BASE  0x40020800u
#define RT1000_USART2_BASE 0x40004400u  /* 0x40004400: el mas referenciado */
#define RT1000_USART6_BASE 0x40011400u
#define RT1000_SPI2_BASE   0x40003800u
#define RT1000_SPI3_BASE   0x40003C00u
#define RT1000_I2C2_BASE   0x40005800u
#define RT1000_ADC1_BASE   0x40012000u
#define RT1000_DAC_BASE    0x40007400u

/* UART abstracta: en el radio es 115200 8N1 (protocolo de update). */
int  hal_uart_init_115200(void);
int  hal_uart_write(const uint8_t *buf, size_t len);
int  hal_uart_read_byte(uint8_t *out);   /* 0=byte, -1=nada, -2=error */
bool hal_uart_rx_ready(void);

/* SPI abstracta (transceptor / flash externa 25Q, por confirmar). */
int hal_spi_init(void);
int hal_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len);

/* Flash interna: solo la region de app 0x08002800-0x0803FFFF.
 * NUNCA escribir 0x08000000-0x080027FF (bootloader de fabrica). */
#define RT1000_APP_BASE 0x08002800u
#define RT1000_APP_SIZE 251904u
int hal_flash_erase_app(void);
int hal_flash_write(uint32_t addr, const uint8_t *buf, size_t len);

/* Tiempo */
void hal_delay_ms(uint32_t ms);
uint32_t hal_millis(void);
