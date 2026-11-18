/* HAL real para el MCU candidato STM32F4-family (ver src/mcu.h).
 * AT32F403A DESCARTADO: sus bases GPIO son 0x40010800, no 0x40020000.
 * Compila SOLO para ARM (RT1000_TARGET_ARM). USART2 es el
 * puerto mas referenciado en V1.16 (8 refs a 0x40004400) y es
 * el puerto de update 115200 8N1. SPI2 es el bus de la flash
 * externa. La flash interna se escribe con los registros
 * FLASH en 0x40023C00 (STM32F4-like). TODO SIN VERIFICAR en
 * el RT-1000Pro: el primer test de hardware (TEST=2, TEST=6)
 * confirma UART y flash antes de usar la app.
 */
#include "hal.h"
#ifdef RT1000_TARGET_ARM
#define USART2 ((volatile uint32_t*)0x40004400u)
#define SPI2   ((volatile uint32_t*)0x40003800u)
#define FLASHR ((volatile uint32_t*)0x40023C00u)
#define RCC    ((volatile uint32_t*)0x40023800u)
/* USART: SR+0x00 DR+0x04 BRR+0x08 CR1+0x0C CR2+0x10 CR3+0x14 */
static void usart2_init(void){
    RCC[0x40/4] |= (1u<<17); /* APB1ENR USART2EN */
    /* BRR para 115200 con PCLK1 60 MHz (120 MHz / 2): 60000000/115200 = 520.8 -> 521 */
    USART2[0x08/4] = 521;
    USART2[0x0C/4] = (1u<<13) | (1u<<3) | (1u<<2); /* UE, TE, RE */
}
int hal_uart_init_115200(void){ usart2_init(); return 0; }
int hal_uart_write(const uint8_t *buf, size_t len){
    for(size_t i=0;i<len;i++){
        while(!(USART2[0] & (1u<<7))) { } /* TXE */
        USART2[0x04/4] = buf[i];
    }
    while(!(USART2[0] & (1u<<6))) { } /* TC */
    return (int)len;
}
bool hal_uart_rx_ready(void){ return (USART2[0] & (1u<<5)) != 0; }
int hal_uart_read_byte(uint8_t *out){
    if(!hal_uart_rx_ready()) return -1;
    *out = (uint8_t)USART2[0x04/4]; return 0;
}
int hal_spi_init(void){
    RCC[0x40/4] |= (1u<<14); /* APB1ENR SPI2EN */
    SPI2[0] = (1u<<6) | (1u<<2) | (1u<<1) | (1u<<0); /* SPE, MSTR, CPOL, CPHA, BR fPCLK/4 */
    return 0;
}
int hal_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len){
    for(size_t i=0;i<len;i++){
        while(!(SPI2[0x08/4] & (1u<<1))) { } /* TXE (SR+0x08) */
        SPI2[0x0C/4] = tx ? tx[i] : 0xFF;
        while(!(SPI2[0x08/4] & (1u<<0))) { } /* RXNE */
        uint8_t v = (uint8_t)SPI2[0x0C/4];
        if(rx) rx[i]=v;
    }
    return 0;
}
/* Flash interna: sectores de 16 KiB en 0x08002800... (STM32F4-like).
 * NUNCA se borra/escribe por debajo de 0x08002800. */
int hal_flash_erase_app(void){
    FLASHR[0x04/4] = 0x45670123u; FLASHR[0x04/4] = 0xCDEF89ABu; /* KEYR unlock */
    for(uint32_t addr=RT1000_APP_BASE; addr<RT1000_APP_BASE+RT1000_APP_SIZE; addr+=16384){
        while(FLASHR[0x0C/4] & (1u<<16)) { } /* BSY */
        FLASHR[0x10/4] = 0x00000002u; /* SER, sector erase, PSIZE x32 */
        FLASHR[0x14/4] = (addr-0x08000000u)/16384; /* SNR */
        FLASHR[0x10/4] |= (1u<<16); /* STRT */
        while(FLASHR[0x0C/4] & (1u<<16)) { }
        FLASHR[0x10/4] = 0;
    }
    FLASHR[0x08/4] = (1u<<31); /* LOCK */
    return 0;
}
int hal_flash_write(uint32_t addr, const uint8_t *buf, size_t len){
    if(addr<RT1000_APP_BASE || addr+len>RT1000_APP_BASE+RT1000_APP_SIZE) return -1;
    if(len%4) return -1;
    FLASHR[0x04/4] = 0x45670123u; FLASHR[0x04/4] = 0xCDEF89ABu;
    FLASHR[0x10/4] = 0x00000002u; /* PG, PSIZE x32 */
    for(size_t i=0;i<len;i+=4){
        while(FLASHR[0x0C/4] & (1u<<16)) { }
        *(volatile uint32_t*)(addr+i) = *(const uint32_t*)(buf+i);
        while(FLASHR[0x0C/4] & (1u<<16)) { }
    }
    FLASHR[0x10/4] = 0;
    FLASHR[0x08/4] = (1u<<31);
    return 0;
}
void hal_delay_ms(uint32_t ms){
    /* SysTick a 120 MHz: 120000 ciclos por ms. */
    for(uint32_t i=0;i<ms;i++) for(volatile uint32_t j=0;j<120000;j++){ }
}
int hal_adc_batt_mv(void){
    /* ADC1 existe en V1.16 (IRQ 18), pero el canal del divisor
     * de bateria no esta confirmado: devolver 0 hasta TEST=7. */
    return 0;
}
static volatile uint32_t tick_ms;
uint32_t hal_millis(void){ return tick_ms; }
void SysTick_Handler(void){ tick_ms++; }

void hal_power_off(void){
    /* Sin latch de energia confirmado no hay corte fisico:
     * se duerme el MCU. El llamador (app.c) ya puso el BK4829
     * en standby y apago el LCD antes de llegar aqui. Con las
     * interrupciones apagadas el WFI no despierta por SysTick;
     * solo un reset / ciclo de energia vuelve a arrancar. */
    __asm__ volatile("cpsid i");
    for(;;) __asm__ volatile("wfi");
}
#endif
