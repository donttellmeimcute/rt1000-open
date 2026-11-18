#include "storage.h"
#include <string.h>
uint32_t bcd_pack_10hz(uint32_t hz){
    uint32_t v=hz/10, out=0;
    for(int i=0;i<8;i++){ out |= (v%10)<<(i*4); v/=10; }
    return out;
}
uint32_t bcd_unpack_10hz(const uint8_t b[4]){
    uint32_t v=0, m=1;
    for(int i=0;i<4;i++) for(int n=0;n<2;n++){
        uint32_t d=(b[i]>>(n*4))&0xF; v += d*m; m*=10;
    }
    return v*10;
}
bool chan_encode(const rt1000_chan_t *c, uint8_t out[CHAN_SIZE]){
    memset(out,0xFF,CHAN_SIZE);
    uint32_t rx=bcd_pack_10hz(c->rx_hz), tx=c->tx_hz==0xFFFFFFFFu?0xFFFFFFFFu:bcd_pack_10hz(c->tx_hz);
    memcpy(out,&rx,4); memcpy(out+4,&tx,4);
    out[8]=(uint8_t)c->rx_tone; out[9]=(uint8_t)(c->rx_tone>>8);
    out[10]=(uint8_t)c->tx_tone; out[11]=(uint8_t)(c->tx_tone>>8);
    out[12]=c->sig_code; out[13]=c->ptt_id; out[14]=c->power_scramble; out[15]=c->flags;
    memcpy(out+20,c->name,12);
    return true;
}
bool chan_decode(const uint8_t in[CHAN_SIZE], rt1000_chan_t *c){
    c->rx_hz=bcd_unpack_10hz(in);
    uint32_t tx; memcpy(&tx,in+4,4);
    c->tx_hz=tx==0xFFFFFFFFu?0xFFFFFFFFu:bcd_unpack_10hz(in+4);
    c->rx_tone=(uint16_t)(in[8]|(in[9]<<8)); c->tx_tone=(uint16_t)(in[10]|(in[11]<<8));
    c->sig_code=in[12]; c->ptt_id=in[13]; c->power_scramble=in[14]; c->flags=in[15];
    memcpy(c->name,in+20,12); c->name[11]=0;
    return true;
}
