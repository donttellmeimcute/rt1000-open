#include "keypad.h"
#include "mcu.h"
/* Matriz corregida (pinmap RT-950 Pro, HW_CONFIRMED):
 * filas PD4..PD7, columnas PC0..PC3 (+ col implicita all-HIGH).
 * PD7 (top): 1 2 3 OK Up | PD6: 4 5 6 ABC Down |
 * PD5: 7 8 9 Back Left | PD4 (bottom): * 0 # V/M Right.
 * Codigos: 0-9 digitos, 10=OK, 11=ABC, 12=Back, 13=V/M,
 * 14=#, 15=*, 20=Up, 21=Down, 22=Left, 23=Right. */
static const int key_table[4][4]={
    {13,14,0,23},  /* PC0: V/M # 0 Right */
    {12,9,8,22},   /* PC1: Back 9 8 Left */
    {11,6,5,21},   /* PC2: ABC 6 5 Down */
    {10,3,2,20},   /* PC3: OK 3 2 Up */
};
uint16_t keypad_scan_raw(void){
    uint16_t raw=0;
#ifdef RT1000_TARGET_ARM
    for(int col=0; col<4; col++){
        /* Poner la columna en bajo, resto en alto. */
        for(int c=0;c<4;c++){ if(c==col) gpio_clr(GPIOC,(uint32_t)c); else gpio_set(GPIOC,(uint32_t)c); }
        /* Settle: pull-up 40k necesita tiempo, no un loop de 10. */
        for(volatile int i=0;i<2000;i++){ }
        for(int row=0; row<4; row++)
            if(!gpio_read(GPIOD, (uint32_t)(4+row))) raw |= (uint16_t)(1u<<(col*4+row));
    }
    for(int c=0;c<4;c++) gpio_set(GPIOC,(uint32_t)c);
#else
    (void)raw;
#endif
    return raw;
}
int keypad_get_key(void){
    uint16_t raw=keypad_scan_raw();
    if(!raw) return KEY_NONE;
    /* raw bit col*4+row: row 0=PD4, 1=PD5, 2=PD6, 3=PD7.
     * La tabla esta indexada [col][row] con row 3=PD7 top. */
    for(int col=0; col<4; col++) for(int row=0; row<4; row++)
        if(raw & (1u<<(col*4+row))) return key_table[col][3-row];
    return KEY_NONE;
}
