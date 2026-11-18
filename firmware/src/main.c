/* Esqueleto de firmware open source RT-1000Pro / UV100.
 *
 * ADVERTENCIA: esto NO es un firmware funcional. No inicializa
 * radio, display, teclado, audio ni GPS. No flashear.
 *
 * Lo que sí documenta, verificado por ingeniería inversa:
 *  - vector table en 0x08002800, SP inicial 0x2000AEC8
 *  - Reset_Handler del firmware oficial es un trampolín:
 *      LDR r0, [PC, #0x24]; BLX r0   -> init en 0x08022FA9
 *      LDR r0, [PC, #0x24]; BX  r0   -> entry  en 0x080029E1
 *    Aquí se reproduce esa estructura a propósito, para que el
 *    día que haya un firmware real el entry point sea el mismo.
 *  - protocolo de update en bootloader_protocol.c
 */
#include <stdint.h>
#include "bootloader_protocol.h"
#include "app.h"
#include "clock.h"

extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;

void Reset_Handler(void);
void Default_Handler(void) { while (1) { } }
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));

/* Vector table: solo los primeros 16 vectores Cortex-M, el resto
 * son handlers por defecto. El firmware oficial tiene 57 vectores
 * no nulos de 64; aquí se deja el mínimo para compilar. */
__attribute__((section(".isr_vector")))
const void *isr_vector[] = {
    (void *)&_estack,
    (void *)Reset_Handler,
    (void *)NMI_Handler,
    (void *)HardFault_Handler,
    0, 0, 0, 0, 0, 0, 0,
    (void *)Default_Handler, /* SVCall */
    0, 0,
    (void *)Default_Handler, /* PendSV */
    (void *)Default_Handler, /* SysTick */
};

static void system_init_stub(void)
{
    /* SystemInit documentado desde V1.16 (clock.c). El PLL
     * decodificado es anomalo (192 MHz fuera de rango), asi que
     * el reloj real se configura en app_main con el objetivo
     * de 120 MHz del MCU candidato. */
    rt1000_system_init_documented();
}

void Reset_Handler(void)
{
    /* Copia .data y limpia .bss, como cualquier startup Cortex-M. */
    for (uint32_t *s = &_sidata, *d = &_sdata; d < &_edata; s++, d++) *d = *s;
    for (uint32_t *d = &_sbss; d < &_ebss; d++) *d = 0;
    system_init_stub();
    /* Entry point equivalente al registro :05 del HEX oficial. */
    extern int main(void);
    main();
}

int main(void)
{
#ifdef RT1000_HW_TEST
    /* Build de prueba de hardware: RT1000_HW_TEST=4, etc.
     * Flashear este build ANTES que la app completa. */
    return hw_test_main(RT1000_HW_TEST);
#else
    return app_main();
#endif
}
