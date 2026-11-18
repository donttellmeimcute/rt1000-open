#pragma once
#include <stdint.h>
/* MCU del RT-1000Pro: familia STM32F4 (mapa de registros),
 * modelo exacto SIN CONFIRMAR.
 *
 * EVIDENCIA contra AT32F403A (el MCU del RT-950 Pro):
 * el AT32F403A usa GPIOA=0x40010800, GPIOB=0x40010C00,
 * GPIOC=0x40011000, GPIOD=0x40011400, GPIOE=0x40011800
 * (ver /tmp/rt950 pinmap, Hertzz58). El V1.16 del 1000Pro
 * referencia GPIOA=0x40020000 (44 veces), GPIOB=0x40020400,
 * GPIOC=0x40020800, RCC=0x40023800, USART2=0x40004400,
 * USART6=0x40011400, SPI2=0x40003800. Ese es el mapa
 * STM32F4 / GD32F4 / APM32F4, NO el del AT32F403A.
 * Por eso el firmware del RT-950 Pro NO se puede portar
 * tal cual: ni las bases de GPIO coinciden, ni la base de
 * app (0x08002800 vs 0x08003000), ni el tamano de flash
 * (256 KiB vs 1 MB), ni el formato de update (HEX plano
 * Ido_Update vs .BTF cifrado).
 *
 * Candidatos con mapa STM32F4: STM32F401/405/407, GD32F403/
 * GD32F405, APM32F407 y clones. El modelo exacto solo se
 * confirma con el marcado del chip + SWD IDCODE.
 */
#define RT1000_MCU_CANDIDATE "STM32F4-family (GD32F4/APM32F4), modelo sin confirmar; AT32F403A DESCARTADO"
#define RT1000_MCU_FLASH_KB  256
#define RT1000_MCU_RAM_KB    48    /* hipotesis: SP observado 0x2000AEC8 */
#define RT1000_SYSCLK_TARGET_HZ 120000000u

/* Mapa STM32F4: MODER+0x00 OTYPER+0x04 OSPEEDR+0x08 PUPDR+0x0C
 * IDR+0x10 ODR+0x14 BSRR+0x18 LCKR+0x1C AFRL+0x20 AFRH+0x24 */
typedef struct { volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFRL, AFRH; } rt1000_gpio_t;
#define GPIOA ((rt1000_gpio_t*)0x40020000u)
#define GPIOB ((rt1000_gpio_t*)0x40020400u)
#define GPIOC ((rt1000_gpio_t*)0x40020800u)
#define GPIOD ((rt1000_gpio_t*)0x40020C00u)
#define GPIOE ((rt1000_gpio_t*)0x40021000u)
#define GPIOF ((rt1000_gpio_t*)0x40021400u)

static inline void gpio_set(volatile rt1000_gpio_t *g, uint32_t pin){ g->BSRR = (1u<<pin); }
static inline void gpio_clr(volatile rt1000_gpio_t *g, uint32_t pin){ g->BSRR = (1u<<(pin+16)); }
static inline uint32_t gpio_read(volatile rt1000_gpio_t *g, uint32_t pin){ return (g->IDR>>pin)&1u; }
static inline void gpio_mode_out(volatile rt1000_gpio_t *g, uint32_t pin){
    g->MODER &= ~(3u<<(pin*2)); g->MODER |= (1u<<(pin*2));
}
static inline void gpio_mode_in(volatile rt1000_gpio_t *g, uint32_t pin){
    g->MODER &= ~(3u<<(pin*2));
}
