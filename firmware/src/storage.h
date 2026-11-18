#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Config en flash SPI externa (candidato RT-950 Pro, confirmado
 * byte a byte por jaymart1983 en ese radio; en el 1000Pro la
 * flash existe (lista 25Q en V1.16) pero el layout NO esta
 * verificado). Canal: 32 bytes. Frecuencia: BCD empaquetado
 * little-endian en unidades de 10 Hz (462.5625 MHz =
 * 50 62 25 46). Nombre: 12 bytes ASCII, relleno 0xFF.
 * Zonas: 10 slots de 16 bytes en 0xC000 (NO 0xA200, ese era
 * del RT-900 y rompia los nombres). */
#define CHAN_SIZE 32
#define CHAN_COUNT 999
typedef struct {
    uint32_t rx_hz, tx_hz; /* 0xFFFFFFFF tx = solo RX */
    uint16_t rx_tone, tx_tone;
    uint8_t sig_code, ptt_id, power_scramble, flags;
    char name[12];
} rt1000_chan_t;
bool chan_encode(const rt1000_chan_t *c, uint8_t out[CHAN_SIZE]);
bool chan_decode(const uint8_t in[CHAN_SIZE], rt1000_chan_t *c);
uint32_t bcd_pack_10hz(uint32_t hz);
uint32_t bcd_unpack_10hz(const uint8_t b[4]);

/* Flash SPI externa 25Q (W25Q/GD25Q, lista en V1.16).
 * Nuestros datos van en el ULTIMO sector de 4 KiB de la flash
 * detectada, para no tocar el layout del firmware oficial.
 * CS: el bus SPI2 ya se usa asi en TEST 6; si la flash no
 * responde, flash25q_detect() devuelve false. */
#define FLASH25Q_SECTOR 4096
#define FLASH25Q_PAGE 256
bool flash25q_detect(uint32_t *size_bytes); /* JEDEC 0x9F */
bool flash25q_read(uint32_t addr, uint8_t *buf, size_t len);
bool flash25q_program(uint32_t addr, const uint8_t *buf, size_t len);
bool flash25q_erase_sector(uint32_t addr); /* borra 4 KiB */
uint32_t flash25q_user_base(void); /* ultimo sector, 0 si no detectada */

/* Ajustes persistentes (canales + config). Un solo sector:
 * settings (offset 0), canales 16x32 (offset 256),
 * mascara de usados u16 (offset 768), checksum XOR (offset 770).
 * Sin magic/checksum valido, persist_load() devuelve false
 * y la app usa defaults + canales vacios. */
#define SETTINGS_MAGIC 0x31543152u /* "RT1K" */
typedef struct {
    uint32_t magic; uint8_t version;
    char callsign[7]; char device_name[13];
    uint8_t theme; uint8_t aprs_ssid; uint8_t aprs_path_idx;
    char dtmf_code[17]; char mdc_id[5];
    uint8_t gps_fmt; uint8_t tx_priority; uint8_t multi_standby;
    uint8_t tnc; uint8_t vox; uint8_t wide; uint8_t tone_burst;
    int8_t shift_dir; uint32_t offset_hz; uint8_t talkaround; uint8_t monitor;
    uint16_t scan_interval_ms; uint32_t fm_freq_khz;
    uint16_t tone_rx, tone_tx; /* codificacion rt1000_tone_*: 0=OFF */
    uint8_t modem; /* 0=OFF, 1=DMR, 2=NXDN, 3=P25 (nodo MMDVM, ver docs/DMR-CONVERSION.md) */
    uint8_t apo_idx; /* APO: 0=OFF, 1=15, 2=30, 3=60, 4=120 min (ver apo.h) */
    char zone_names[4][13];
} rt1000_settings_t;
void settings_defaults(rt1000_settings_t *s);
bool persist_save(const rt1000_settings_t *s, const rt1000_chan_t *chans, const bool *used, int count);
bool persist_load(rt1000_settings_t *s, rt1000_chan_t *chans, bool *used, int count);
