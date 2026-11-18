#include "bootloader_rx.h"
#include "hal.h"
void rt1000_rx_init(rt1000_rx_t *rx){
    rx->state=RT1000_STATE_WAIT_CONNECT; rx->connects=0; rx->offset=0;
    rx->packets=0; rx->pkt_len=0; rx->error=0;
}
static int ack(void){ uint8_t a=RT1000_ACK; return hal_uart_write(&a,1); }
static int nak(rt1000_rx_t *rx){ uint8_t a=RT1000_NAK; rx->state=RT1000_STATE_ERROR; rx->error=-1; return hal_uart_write(&a,1); }
int rt1000_rx_feed(rt1000_rx_t *rx, const uint8_t *buf, size_t len){
    for(size_t i=0;i<len;i++){
        uint8_t b=buf[i];
        if(rx->state==RT1000_STATE_WAIT_CONNECT){
            /* CONNECT es de 5 bytes: 39 33 05 10 28 */
            rx->pkt[rx->pkt_len++]=b;
            if(rx->pkt_len==5){
                if(rx->pkt[0]==0x39&&rx->pkt[1]==0x33&&rx->pkt[2]==0x05&&rx->pkt[3]==0x10&&rx->pkt[4]==0x28){
                    rx->connects++; ack(); rx->pkt_len=0;
                    if(rx->connects>=4) rx->state=RT1000_STATE_WAIT_UPDATE;
                } else { rx->pkt_len=0; return nak(rx); }
            }
        } else if(rx->state==RT1000_STATE_WAIT_UPDATE){
            rx->pkt[rx->pkt_len++]=b;
            if(rx->pkt_len==5){
                if(rx->pkt[0]==0x39&&rx->pkt[1]==0x33&&rx->pkt[2]==0x05&&rx->pkt[3]==0x55&&rx->pkt[4]==0x6D){
                    ack(); rx->state=RT1000_STATE_RECEIVING; rx->pkt_len=0; rx->offset=0;
                } else { rx->pkt_len=0; return nak(rx); }
            }
        } else if(rx->state==RT1000_STATE_RECEIVING){
            rx->pkt[rx->pkt_len++]=b;
            if(rx->pkt_len==RT1000_PACKET_SIZE){
                if(rt1000_validate_packet(rx->pkt, rx->offset)!=0){ rx->pkt_len=0; return nak(rx); }
                if(hal_flash_write(RT1000_APP_BASE+rx->offset, rx->pkt+3, RT1000_PACKET_DATA)!=0){ rx->pkt_len=0; return nak(rx); }
                rx->offset+=RT1000_PACKET_DATA; rx->packets++; rx->pkt_len=0; ack();
                if(rx->offset>=RT1000_IMAGE_SIZE) rx->state=RT1000_STATE_DONE;
            }
        }
    }
    return 0;
}
