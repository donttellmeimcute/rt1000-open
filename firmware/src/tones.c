#include "tones.h"
#include <stdio.h>
/* CTCSS EIA estandar, en decimas de Hz (670 = 67.0 Hz). */
static const uint16_t CTCSS[CTCSS_COUNT] = {
    670, 693, 719, 744, 770, 797, 825, 854, 885, 915,
    948, 974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,
    1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
    1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 2035, 2065,
    2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541};
uint16_t rt1000_ctcss_tone(int idx){
    if(idx < 0 || idx >= CTCSS_COUNT) return 0;
    return CTCSS[idx];
}
/* DCS estandar (sin polaridad invertida; el bit de polaridad
 * vive en el registro del chip, sin confirmar). */
static const uint16_t DCS[DCS_COUNT] = {
    23, 25, 26, 31, 32, 36, 43, 47, 51, 53, 54, 65, 71, 72, 73, 74,
    114, 115, 116, 122, 125, 131, 132, 134, 143, 145, 152, 155, 156, 162,
    165, 172, 174, 205, 212, 223, 225, 226, 243, 244, 245, 246, 251, 252,
    255, 261, 263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343,
    346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432, 445, 446,
    452, 454, 455, 462, 464, 465, 466, 503, 506, 516, 523, 526, 532, 546,
    565, 606, 612, 624, 627, 631, 632, 654, 662, 664, 703, 712, 723, 731,
    732, 734, 743, 754};
uint16_t rt1000_dcs_code(int idx){
    if(idx < 0 || idx >= DCS_COUNT) return 0;
    return DCS[idx];
}
/* DTMF: filas 697/770/852/941 Hz, columnas 1209/1336/1477/1633 Hz. */
bool rt1000_dtmf_freqs(char key, uint16_t *f_row, uint16_t *f_col){
    static const uint16_t ROWS[4] = {697, 770, 852, 941};
    static const uint16_t COLS[4] = {1209, 1336, 1477, 1633};
    static const char KEYS[4][4] = {
        {'1', '2', '3', 'A'}, {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'}, {'*', '0', '#', 'D'}};
    for(int r = 0; r < 4; r++)
        for(int c = 0; c < 4; c++)
            if(KEYS[r][c] == key){ *f_row = ROWS[r]; *f_col = COLS[c]; return true; }
    return false;
}
uint16_t rt1000_tone_encode_ctcss(int idx){
    if(idx < 0 || idx >= CTCSS_COUNT) return 0;
    return (uint16_t)(idx + 1);
}
uint16_t rt1000_tone_encode_dcs(int idx){
    if(idx < 0 || idx >= DCS_COUNT) return 0;
    return (uint16_t)(CTCSS_COUNT + 1 + idx);
}
bool rt1000_tone_is_dcs(uint16_t code){
    return code > CTCSS_COUNT;
}
uint16_t rt1000_tone_value(uint16_t code){
    if(code == 0) return 0;
    if(code <= CTCSS_COUNT) return rt1000_ctcss_tone((int)code - 1);
    if(code <= CTCSS_COUNT + DCS_COUNT) return rt1000_dcs_code((int)code - CTCSS_COUNT - 1);
    return 0;
}
void rt1000_tone_label(uint16_t code, char *buf, size_t n){
    if(!buf || n == 0) return;
    if(code == 0){ snprintf(buf, n, "OFF"); return; }
    if(code <= CTCSS_COUNT){
        uint16_t t = rt1000_ctcss_tone((int)code - 1);
        snprintf(buf, n, "%u.%u", t / 10, t % 10);
    } else if(code <= CTCSS_COUNT + DCS_COUNT){
        snprintf(buf, n, "DCS%03u", rt1000_dcs_code((int)code - CTCSS_COUNT - 1));
    } else {
        snprintf(buf, n, "?");
    }
}
