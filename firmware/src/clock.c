#include "clock.h"
rt1000_pll_t rt1000_pll_decode(uint32_t v){
    static const uint32_t divs[4]={2,4,6,8};
    rt1000_pll_t p;
    p.pllm=(v>>0)&0x3F; p.plln=(v>>6)&0x1FF;
    p.pllp_div=divs[(v>>16)&3]; p.pllsrc=(v>>22)&1; p.pllq=(v>>24)&0xF;
    return p;
}
uint32_t rt1000_pll_sysclk_hz(rt1000_pll_t p, uint32_t hsi_hz, int *valid){
    /* VCOin debe ser 1-2 MHz y VCO 192-432 MHz en STM32F4. */
    uint32_t vcoin=hsi_hz/p.pllm;
    uint32_t vco=vcoin*p.plln;
    *valid = (vcoin>=1000000 && vcoin<=2000000 && vco>=192000000 && vco<=432000000 && p.pllq!=0);
    return vco/p.pllp_div;
}
#ifdef RT1000_TARGET_ARM
/* Reproduce EXACTAMENTE los writes de SystemInit V1.16.
 * Solo compila para ARM y solo se llama desde main() cuando
 * el MCU se confirme. Hoy NO se debe flashear. */
void rt1000_system_init_documented(void){
    volatile uint32_t *cpacr=(volatile uint32_t*)0xE000ED88u;
    volatile uint32_t *rcc=(volatile uint32_t*)0x40023800u;
    volatile uint32_t *vtor=(volatile uint32_t*)0xE000ED08u;
    *cpacr |= 0x00F00000u;              /* FPU CP10/CP11 */
    rcc[0] |= 1u;                       /* RCC_CR HSION */
    while(!((rcc[0]>>1)&1u)) { }        /* HSIRDY */
    rcc[2] &= ~3u;                      /* RCC_CFGR SW=HSI */
    while(((rcc[2]>>2)&3u)!=0) { }      /* SWS==HSI */
    rcc[0] &= 0xFEF2FFFFu;              /* apaga PLL/HSE/CSS */
    rcc[2]  = 0x40000000u;              /* RCC_CFGR (valor observado) */
    rcc[1]  = 0x00033002u;              /* RCC_PLLCFGR (valor observado, ver doc) */
    rcc[0xA0/4] = 0x000F0000u;          /* offset +0xA0 (sin equivalente STM32F4) */
    rcc[3]  = 0x009F0000u;              /* RCC_CIR */
    *vtor  = 0x08000000u;               /* VTOR = inicio de flash */
}
#else
void rt1000_system_init_documented(void){ /* host: no-op */ }
#endif
