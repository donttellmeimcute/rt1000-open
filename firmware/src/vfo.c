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
uint32_t rt1000_tx_freq(uint32_t rx_hz, int shift_dir, uint32_t offset_hz, bool talkaround){
    if(talkaround || shift_dir==0) return rx_hz;
    int64_t tx = (int64_t)rx_hz + (int64_t)shift_dir * (int64_t)offset_hz;
    if(tx < 0 || tx > 480000000) return 0;
    if(!rt1000_freq_tx_allowed((uint32_t)tx)) return 0;
    return (uint32_t)tx;
}
/* NOAA Weather Radio: 162.400, 162.425, 162.450, 162.475,
 * 162.500, 162.525, 162.550 MHz. */
static const uint32_t NOAA_FREQS[NOAA_CH_COUNT] = {
    162400000u, 162425000u, 162450000u, 162475000u,
    162500000u, 162525000u, 162550000u};
uint32_t rt1000_noaa_freq(int ch){
    if(ch < 1 || ch > NOAA_CH_COUNT) return 0;
    return NOAA_FREQS[ch - 1];
}
const char *rt1000_noaa_name(int ch){
    static const char *names[NOAA_CH_COUNT] = {
        "WX1", "WX2", "WX3", "WX4", "WX5", "WX6", "WX7"};
    if(ch < 1 || ch > NOAA_CH_COUNT) return "?";
    return names[ch - 1];
}
uint32_t rt1000_scan_next(uint32_t from, uint32_t step_hz){
    uint32_t lo, hi;
    if(from >= 400000000u){ lo = 400000000u; hi = 480000000u; }
    else if(from >= 136000000u){ lo = 136000000u; hi = 174000000u; }
    else if(from >= 27000000u){ lo = 27000000u; hi = 30000000u; }
    else { lo = 150000u; hi = 30000000u; }
    uint32_t next = from + step_hz;
    if(next > hi || next < from) next = lo;
    return next;
}
