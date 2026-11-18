#include "storage.h"
#include "hal.h"
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

/* ---- Flash SPI externa 25Q ---- */
static uint32_t flash_size;
static bool flash_known;
static void spi_cmd(const uint8_t *tx, uint8_t *rx, size_t len){
    hal_spi_transfer(tx, rx, len);
}
bool flash25q_detect(uint32_t *size_bytes){
    uint8_t tx[4] = {0x9F, 0xFF, 0xFF, 0xFF}, rx[4] = {0};
    spi_cmd(tx, rx, sizeof tx);
    if(rx[1] != 0xEF && rx[1] != 0xC8) return false; /* Winbond / GigaDevice */
    /* byte de capacidad: 0x14=1MB ... 0x19=32MB, 0x20=64MB */
    static const uint32_t SIZES[13] = {
        0x100000u, 0x200000u, 0x400000u, 0x800000u, 0x1000000u, 0x2000000u,
        0, 0, 0, 0, 0, 0, 0x4000000u};
    uint8_t cap = rx[3];
    uint32_t sz = 0;
    if(cap >= 0x14 && cap <= 0x20) sz = SIZES[cap - 0x14];
    if(sz == 0) return false;
    flash_size = sz; flash_known = true;
    if(size_bytes) *size_bytes = sz;
    return true;
}
uint32_t flash25q_user_base(void){
    if(!flash_known && !flash25q_detect(NULL)) return 0;
    return flash_size - FLASH25Q_SECTOR;
}
bool flash25q_read(uint32_t addr, uint8_t *buf, size_t len){
    if(!buf) return false;
    uint8_t tx[4 + 512]; uint8_t rx[4 + 512];
    while(len){
        size_t chunk = len > 512 ? 512 : len;
        tx[0] = 0x03; tx[1] = (uint8_t)(addr >> 16); tx[2] = (uint8_t)(addr >> 8); tx[3] = (uint8_t)addr;
        memset(tx + 4, 0xFF, chunk);
        spi_cmd(tx, rx, 4 + chunk);
        memcpy(buf, rx + 4, chunk);
        buf += chunk; addr += (uint32_t)chunk; len -= chunk;
    }
    return true;
}
static bool flash25q_write_enable(void){
    uint8_t tx[1] = {0x06};
    spi_cmd(tx, NULL, 1);
    return true;
}
bool flash25q_program(uint32_t addr, const uint8_t *buf, size_t len){
    while(len){
        size_t page_off = addr % FLASH25Q_PAGE;
        size_t chunk = FLASH25Q_PAGE - page_off;
        if(chunk > len) chunk = len;
        uint8_t tx[4 + FLASH25Q_PAGE]; uint8_t rx[4 + FLASH25Q_PAGE];
        flash25q_write_enable();
        tx[0] = 0x02; tx[1] = (uint8_t)(addr >> 16); tx[2] = (uint8_t)(addr >> 8); tx[3] = (uint8_t)addr;
        memcpy(tx + 4, buf, chunk);
        spi_cmd(tx, rx, 4 + chunk);
        hal_delay_ms(5); /* tPP tipico */
        buf += chunk; addr += (uint32_t)chunk; len -= chunk;
    }
    return true;
}
bool flash25q_erase_sector(uint32_t addr){
    uint8_t tx[4]; uint8_t rx[4];
    flash25q_write_enable();
    tx[0] = 0x20; tx[1] = (uint8_t)(addr >> 16); tx[2] = (uint8_t)(addr >> 8); tx[3] = (uint8_t)addr;
    spi_cmd(tx, rx, 4);
    hal_delay_ms(50); /* tSE tipico */
    return true;
}

/* ---- Ajustes persistentes ---- */
void settings_defaults(rt1000_settings_t *s){
    memset(s, 0, sizeof *s);
    s->magic = SETTINGS_MAGIC; s->version = 1;
    memcpy(s->callsign, "XE1ARS", 6);
    memcpy(s->device_name, "RT-1000Pro", 10);
    s->theme = 1; s->aprs_ssid = 0; s->aprs_path_idx = 0;
    memcpy(s->dtmf_code, "1234", 4);
    memcpy(s->mdc_id, "0001", 4);
    s->gps_fmt = 0; s->tx_priority = 0; s->multi_standby = 0;
    s->tnc = 0; s->vox = 0; s->wide = 0; s->tone_burst = 0;
    s->shift_dir = 0; s->offset_hz = 600000; s->talkaround = 0; s->monitor = 0;
    s->scan_interval_ms = 300; s->fm_freq_khz = 87900;
    memcpy(s->zone_names[0], "ZONA 1", 6);
    memcpy(s->zone_names[1], "ZONA 2", 6);
    memcpy(s->zone_names[2], "ZONA 3", 6);
    memcpy(s->zone_names[3], "ZONA 4", 6);
}
static uint8_t settings_checksum(const uint8_t *b, size_t n){
    uint8_t c = 0; for(size_t i = 0; i < n; i++) c ^= b[i]; return c;
}
bool persist_save(const rt1000_settings_t *s, const rt1000_chan_t *chans, const bool *used, int count){
    uint32_t base = flash25q_user_base();
    if(!base) return false;
    if(count > 16) count = 16;
    uint8_t sector[FLASH25Q_SECTOR];
    memset(sector, 0xFF, sizeof sector);
    rt1000_settings_t tmp = *s;
    tmp.magic = SETTINGS_MAGIC; tmp.version = 1;
    memcpy(sector, &tmp, sizeof tmp);
    uint16_t mask = 0;
    for(int i = 0; i < count; i++){
        uint8_t enc[CHAN_SIZE];
        chan_encode(&chans[i], enc);
        memcpy(sector + 256 + i * CHAN_SIZE, enc, CHAN_SIZE);
        if(used[i]) mask |= (uint16_t)(1u << i);
    }
    sector[768] = (uint8_t)(mask & 0xFF); sector[769] = (uint8_t)(mask >> 8);
    sector[770] = settings_checksum(sector, 770);
    if(!flash25q_erase_sector(base)) return false;
    for(size_t off = 0; off < sizeof sector; off += FLASH25Q_PAGE){
        if(!flash25q_program(base + (uint32_t)off, sector + off, FLASH25Q_PAGE)) return false;
    }
    return true;
}
bool persist_load(rt1000_settings_t *s, rt1000_chan_t *chans, bool *used, int count){
    uint32_t base = flash25q_user_base();
    if(!base) return false;
    if(count > 16) count = 16;
    uint8_t sector[FLASH25Q_SECTOR];
    if(!flash25q_read(base, sector, sizeof sector)) return false;
    if(settings_checksum(sector, 770) != sector[770]) return false;
    rt1000_settings_t tmp;
    memcpy(&tmp, sector, sizeof tmp);
    if(tmp.magic != SETTINGS_MAGIC || tmp.version != 1) return false;
    *s = tmp;
    uint16_t mask = (uint16_t)(sector[768] | (sector[769] << 8));
    for(int i = 0; i < count; i++){
        chan_decode(sector + 256 + i * CHAN_SIZE, &chans[i]);
        used[i] = (mask & (1u << i)) != 0;
    }
    return true;
}
