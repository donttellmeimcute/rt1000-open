#pragma once
#include <stdint.h>
/* Reloj, documentado desde SystemInit en 0x08022FA8.
 * ADVERTENCIA: el PLL decodificado (PLLM=2, PLLN=192, PLLP=/8, HSI)
 * da VCO 1536 MHz con HSI 16 MHz, fuera de rango STM32F4.
 * Ver docs/RE-ANALYSIS-2026-11-17.md. No usar 192 MHz como dato.
 */
typedef struct { uint32_t pllm, plln, pllp_div, pllsrc, pllq; } rt1000_pll_t;
rt1000_pll_t rt1000_pll_decode(uint32_t pllcfgr);
uint32_t rt1000_pll_sysclk_hz(rt1000_pll_t p, uint32_t hsi_hz, int *valid);
void rt1000_system_init_documented(void); /* solo ARM, reproduce los writes observados */
