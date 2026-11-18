#include "vfo.h"
bool rt1000_freq_in_rx_range(uint32_t hz){
    /* LW/MW/SW + VHF/UHF RX, segun specs del producto (RX, no TX) */
    if(hz>=150000 && hz<=30000000u) return true;   /* LW/MW/SW */
    if(hz>=108000000u && hz<=136000000u) return true; /* airband RX */
    if(hz>=136000000u && hz<=174000000u) return true;
    if(hz>=400000000u && hz<=480000000u) return true;
    return false;
}
bool rt1000_freq_tx_allowed(uint32_t hz){
    /* TX segun specs: 27-30 MHz, VHF/UHF aficionado. Airband TX NO. */
    if(hz>=27000000u && hz<=30000000u) return true;
    if(hz>=144000000u && hz<=148000000u) return true;
    if(hz>=430000000u && hz<=440000000u) return true;
    return false;
}
uint32_t rt1000_freq_clamp_rx(uint32_t hz){
    if(hz<150000) return 150000;
    if(hz>480000000u) return 480000000u;
    return hz;
}
