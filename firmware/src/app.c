#include "app.h"
#include "hal.h"
#include "clock.h"
#include "display.h"
#include "ui.h"
#include "bk4829.h"
#include "si4732.h"
#include "keypad.h"
#include "nmea.h"
#include "aprs.h"
#include "vfo.h"
#include "tones.h"
#include "storage.h"
#include <string.h>
#include <stdio.h>

#define FW_VERSION "v1.0-open"
#define CHAN_SLOTS 16
#define ZONES 4
#define CHANS_PER_ZONE 4

/* Teclas (keypad.c): digitos 0-9, OK=10, ABC=11, Back=12,
 * V/M=13, #=14, *=15, Up=20, Down=21, Left=22, Right=23. */
#define K_OK 10
#define K_ABC 11
#define K_BACK 12
#define K_VM 13
#define K_HASH 14
#define K_STAR 15
#define K_UP 20
#define K_DOWN 21
#define K_LEFT 22
#define K_RIGHT 23

typedef enum {
    ST_SPLASH, ST_MAIN, ST_MENU, ST_GPS, ST_CHAN, ST_INFO, ST_ENTRY,
    ST_FM, ST_APRS, ST_EDIT, ST_ALARM, ST_MSG
} app_state_t;

/* Items del menu (orden fijo; el menu hace scroll de 10 filas). */
enum {
    M_VFO, M_STEP, M_POWER, M_MODE, M_SQL, M_GPS, M_APRS, M_BACKLIGHT, M_THEME,
    M_CALLSIGN, M_DEVNAME, M_SCAN, M_NOAA, M_FM, M_APRS_SSID, M_APRS_PATH,
    M_APRS_DETAIL, M_DTMF, M_MDC, M_VOX, M_WIDTH, M_BURST, M_TONE_RX, M_TONE_TX,
    M_SHIFT, M_OFFSET, M_TALK, M_REVERSE, M_MONITOR, M_GPSFMT, M_TNC, M_ZONE, M_TXPRIO, M_MULTISTBY,
    M_ALARM, M_BACKUP, M_RESET, M_CHAN, M_INFO,
    M_COUNT
};

static const uint32_t STEPS[] = {5000, 6250, 10000, 12500, 25000};
#define NSTEPS (sizeof STEPS / sizeof STEPS[0])
static const char *APRS_OPTS[] = {"OFF", "30s", "60s", "5min"};
static const uint32_t APRS_SECS[] = {0, 30, 60, 300};
static const char *APRS_PATHS[] = {
    "WIDE1-1,WIDE2-1", "WIDE1-1", "WIDE2-1", "WIDE1-1,WIDE2-2", "DIRECTO"};
static const char *APRS_PATHS_FRAME[] = {
    "WIDE1-1,WIDE2-1", "WIDE1-1", "WIDE2-1", "WIDE1-1,WIDE2-2", ""};
#define NAPRS_PATHS 5
static const uint32_t OFFSETS[] = {0, 600000, 1600000, 5000000};
static const char *OFFSET_NAMES[] = {"OFF", "600k", "1.6M", "5M"};
#define NOFFSETS 4
static const char *GPS_FMT_NAMES[] = {"GRADO", "GRADO.MIN", "GRADO.MIN.SEG"};
static const char *TXPRIO_NAMES[] = {"OFF", "A", "B"};

typedef struct {
    rt1000_vfo_t vfo_a, vfo_b;
    uint8_t active;      /* 0=A, 1=B */
    uint8_t mode, power, sql;
    int step_idx;
    bool gps_on;
    int aprs_idx;        /* 0 OFF, 1 30s, 2 60s, 3 5min */
    bool backlight;
    rt1000_gps_t gps;
    int16_t rssi_dbm;
    rt1000_chan_t chans[CHAN_SLOTS];
    bool chan_used[CHAN_SLOTS];
    rt1000_settings_t settings; /* persistentes (flash 25Q) */
    int zone;            /* 0..3 */
    bool scanning, alarm;
    bool reverse;        /* Reverse Freq (momentaneo por sesion) */
    int noaa_ch;         /* 0=off, 1..7 */
    bool si4732_ok;
    int batt_mv;
    uint32_t last_scan_ms;
    int edit_target;     /* 0 callsign, 1 nombre, 2 DTMF, 3 MDC */
    char edit_buf[17];
    int edit_cur, edit_len;
    char msg_line1[24], msg_line2[32];
    int msg_confirm;     /* 0=auto, 1=reset */
    uint32_t msg_t0;
} app_ctx_t;

static app_ctx_t ctx = {
    .vfo_a = {146520000u, 0, 2}, .vfo_b = {435000000u, 0, 2},
    .active = 0, .mode = 0, .power = 2, .sql = 2, .step_idx = 3,
    .gps_on = true, .aprs_idx = 0, .backlight = true,
};
static app_state_t state = ST_SPLASH;
static uint32_t splash_t0;
static int menu_sel;
static int chan_sel;
static char entry_buf[16];
static uint32_t last_rssi_ms, last_beacon_ms;

static uint32_t active_freq(void){
    return ctx.active == 0 ? ctx.vfo_a.freq_hz : ctx.vfo_b.freq_hz;
}
static void set_active_freq(uint32_t hz){
    hz = rt1000_freq_clamp_rx(hz);
    if(ctx.active == 0) ctx.vfo_a.freq_hz = hz; else ctx.vfo_b.freq_hz = hz;
    bk4829_set_freq(0, hz);
}
/* Frecuencia resultante de TX con shift/offset/talkaround/reverse.
 * 0 = Prohibit TX (fuera de banda de aficionado). */
static uint32_t tx_result_freq(void){
    int dir = ctx.settings.shift_dir;
    if(ctx.reverse) dir = -dir;
    return rt1000_tx_freq(active_freq(), dir, ctx.settings.offset_hz,
                          ctx.settings.talkaround);
}
static bool persist_now(void){
    return persist_save(&ctx.settings, ctx.chans, ctx.chan_used, CHAN_SLOTS);
}

static void ui_state_fill(ui_state_t *s){
    memset(s, 0, sizeof *s);
    s->vfo_a_hz = ctx.vfo_a.freq_hz;
    s->vfo_b_hz = ctx.vfo_b.freq_hz;
    s->active = ctx.active;
    s->mode = ctx.mode;
    s->power = ctx.power;
    s->sql = ctx.sql;
    s->step_hz = STEPS[ctx.step_idx];
    s->rssi_dbm = ctx.rssi_dbm;
    s->tx = false;
    s->gps_fix = ctx.gps.fix;
    s->gps_h = ctx.gps.hour; s->gps_m = ctx.gps.min; s->gps_s = ctx.gps.sec;
    s->gps_lat = ctx.gps.lat; s->gps_lon = ctx.gps.lon;
    s->gps_alt_m = ctx.gps.alt_m;
    s->gps_sats = ctx.gps.sats;
    s->speed_kmh = ctx.gps.speed_kmh;
    s->course = ctx.gps.course;
    s->gps_fmt = ctx.settings.gps_fmt;
    s->callsign = ctx.settings.callsign;
    s->device_name = ctx.settings.device_name;
    s->aprs_state = APRS_OPTS[ctx.aprs_idx];
    s->monitor = ctx.settings.monitor;
    s->scan = ctx.scanning;
    s->batt_mv = ctx.batt_mv;
}

/* "146.5325" -> Hz, sin floats. '*' es el punto decimal. */
static bool entry_parse(const char *b, uint32_t *hz_out){
    uint32_t mhz = 0, frac = 0;
    int frac_digits = 0, seen_dot = 0;
    if(!*b) return false;
    for(const char *p = b; *p; p++){
        if(*p == '.'){
            if(seen_dot) return false;
            seen_dot = 1;
        } else if(*p >= '0' && *p <= '9'){
            if(!seen_dot){ mhz = mhz * 10 + (uint32_t)(*p - '0'); if(mhz > 999) return false; }
            else if(frac_digits < 4){ frac = frac * 10 + (uint32_t)(*p - '0'); frac_digits++; }
            else return false;
        } else return false;
    }
    while(frac_digits < 4){ frac *= 10; frac_digits++; }
    *hz_out = mhz * 1000000u + frac * 100u;
    return *hz_out >= 150000 && *hz_out <= 480000000u;
}

/* Construye el frame APRS de posicion (con path y FCS) y lo
 * devuelve en `frame`; el TX por AFSK sigue pendiente de audio. */
static size_t aprs_build_frame(uint8_t *frame, size_t cap){
    char comment[32];
    snprintf(comment, sizeof comment, "RT-1000Pro open /A=%06.0f",
             ctx.gps.alt_m * 3.28084);
    size_t n = rt1000_aprs_position_path(
        frame, cap, ctx.settings.callsign, ctx.settings.aprs_ssid, "APRS",
        APRS_PATHS_FRAME[ctx.settings.aprs_path_idx],
        ctx.gps.lat, ctx.gps.lon, ctx.gps.alt_m, comment);
    if(n + 2 > cap) return n;
    uint16_t fcs = rt1000_ax25_fcs(frame, n);
    frame[n++] = (uint8_t)(fcs & 0xFF);
    frame[n++] = (uint8_t)(fcs >> 8);
    return n;
}

static void aprs_beacon_maybe(uint32_t now_ms){
    if(ctx.aprs_idx == 0 || !ctx.gps.fix) return;
    uint32_t iv = APRS_SECS[ctx.aprs_idx] * 1000u;
    if(now_ms - last_beacon_ms < iv) return;
    last_beacon_ms = now_ms;
    uint8_t frame[160];
    size_t n = aprs_build_frame(frame, sizeof frame);
    if(ctx.settings.tnc && n > 0){
        /* TNC Stream: el frame sale por UART para un TNC externo. */
        hal_uart_write(frame, n);
        hal_uart_write((const uint8_t *)"\r\n", 2);
    }
}

static void gps_feed_byte(uint8_t b){
    static char line[128];
    static size_t n;
    if(!ctx.gps_on) return;
    if(b == '\n'){
        line[n] = 0; n = 0;
        if(line[0] == '$') rt1000_nmea_parse(line, &ctx.gps);
    } else if(b != '\r' && n < sizeof line - 1){
        line[n++] = (char)b;
    } else if(n >= sizeof line - 1){
        n = 0;
    }
}

static const char *edit_charset(int target){
    switch(target){
    case 0: return "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    case 1: return " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
    case 2: return "0123456789*#ABCD";
    default: return "0123456789ABCDEF";
    }
}
static int edit_len_for(int target){
    switch(target){ case 0: return 6; case 1: return 12; case 2: return 16; default: return 4; }
}
static const char *edit_title(int target){
    switch(target){
    case 0: return "CALLSIGN"; case 1: return "NOMBRE"; case 2: return "DTMF"; default: return "MDC ID";
    }
}
static void edit_start(int target){
    ctx.edit_target = target;
    ctx.edit_len = edit_len_for(target);
    ctx.edit_cur = 0;
    const char *cs = edit_charset(target);
    const char *src = NULL;
    switch(target){
    case 0: src = ctx.settings.callsign; break;
    case 1: src = ctx.settings.device_name; break;
    case 2: src = ctx.settings.dtmf_code; break;
    default: src = ctx.settings.mdc_id; break;
    }
    for(int i = 0; i < ctx.edit_len; i++)
        ctx.edit_buf[i] = src[i] ? src[i] : cs[0];
    ctx.edit_buf[ctx.edit_len] = 0;
    state = ST_EDIT;
}
static void edit_save(void){
    switch(ctx.edit_target){
    case 0: memcpy(ctx.settings.callsign, ctx.edit_buf, 6); ctx.settings.callsign[6] = 0; break;
    case 1: memcpy(ctx.settings.device_name, ctx.edit_buf, 12); ctx.settings.device_name[12] = 0; break;
    case 2: memcpy(ctx.settings.dtmf_code, ctx.edit_buf, 16); ctx.settings.dtmf_code[16] = 0; break;
    default: memcpy(ctx.settings.mdc_id, ctx.edit_buf, 4); ctx.settings.mdc_id[4] = 0; break;
    }
    persist_now();
    state = ST_MENU;
}
static void edit_key(int key){
    const char *cs = edit_charset(ctx.edit_target);
    int ncs = (int)strlen(cs);
    char *cur = &ctx.edit_buf[ctx.edit_cur];
    const char *pos = strchr(cs, *cur);
    int idx = pos ? (int)(pos - cs) : 0;
    if(key == K_UP) *cur = cs[(idx + 1) % ncs];
    else if(key == K_DOWN) *cur = cs[(idx + ncs - 1) % ncs];
    else if(key == K_LEFT) ctx.edit_cur = (ctx.edit_cur + ctx.edit_len - 1) % ctx.edit_len;
    else if(key == K_RIGHT) ctx.edit_cur = (ctx.edit_cur + 1) % ctx.edit_len;
    else if(key == K_OK) edit_save();
    else if(key == K_BACK) state = ST_MENU;
}

static void draw_all(void){
    ui_state_t s;
    ui_state_fill(&s);
    switch(state){
    case ST_SPLASH: ui_splash(ctx.settings.callsign, FW_VERSION); break;
    case ST_MAIN: ui_main(&s); break;
    case ST_ENTRY: ui_entry(&s, entry_buf); break;
    case ST_GPS: ui_gps_screen(&s); break;
    case ST_CHAN:
        ui_chan_screen(ctx.chans + ctx.zone * CHANS_PER_ZONE,
                       ctx.chan_used + ctx.zone * CHANS_PER_ZONE,
                       CHANS_PER_ZONE, chan_sel,
                       ctx.settings.zone_names[ctx.zone]);
        break;
    case ST_INFO: ui_info_screen(&s, FW_VERSION); break;
    case ST_FM: ui_fm_screen(ctx.settings.fm_freq_khz, ctx.si4732_ok); break;
    case ST_APRS: {
        uint8_t frame[160];
        size_t n = aprs_build_frame(frame, sizeof frame);
        ui_aprs_screen(ctx.settings.callsign, ctx.settings.aprs_ssid,
                       APRS_PATHS[ctx.settings.aprs_path_idx],
                       n > 0 && rt1000_ax25_fcs_ok(frame, n) ? "Success" : "Fail");
        break;
    }
    case ST_EDIT: ui_editor_screen(edit_title(ctx.edit_target), ctx.edit_buf, ctx.edit_cur); break;
    case ST_ALARM: ui_alarm_screen(ctx.settings.callsign); break;
    case ST_MSG: ui_msg_screen(ctx.msg_line1, ctx.msg_line2); break;
    case ST_MENU: {
        static const char *labels[M_COUNT] = {
            "VFO activo", "Paso", "Potencia", "Modo", "Squelch",
            "GPS", "APRS beacon", "Luz pantalla", "Tema",
            "Callsign", "Nombre", "Scan", "NOAA CH", "Radio FM",
            "APRS SSID", "APRS Path", "APRS detalle",
            "DTMF", "MDC ID", "VOX", "Ancho banda", "Tone Burst",
            "CTCSS RX", "CTCSS TX",
            "Shift", "Offset", "Talkaround", "Reverse",
            "Monitor", "GPS formato", "TNC", "Zona",
            "Prioridad TX", "Multi standby",
            "Alarma", "Backup", "Reset", "Canales", "Info"};
        char v[M_COUNT][16];
        snprintf(v[M_VFO], 16, "%s", ctx.active == 0 ? "A" : "B");
        snprintf(v[M_STEP], 16, "%lu.%01luk",
                 (unsigned long)(STEPS[ctx.step_idx] / 1000),
                 (unsigned long)((STEPS[ctx.step_idx] % 1000) / 100));
        snprintf(v[M_POWER], 16, "%s", UI_POWER_NAMES[ctx.power]);
        snprintf(v[M_MODE], 16, "%s", UI_MODE_NAMES[ctx.mode]);
        snprintf(v[M_SQL], 16, "%d", ctx.sql);
        snprintf(v[M_GPS], 16, "%s", ctx.gps_on ? "ON" : "OFF");
        snprintf(v[M_APRS], 16, "%s", APRS_OPTS[ctx.aprs_idx]);
        snprintf(v[M_BACKLIGHT], 16, "%s", ctx.backlight ? "ON" : "OFF");
        snprintf(v[M_THEME], 16, "%s", ui_theme_name());
        snprintf(v[M_CALLSIGN], 16, "%s", ctx.settings.callsign);
        snprintf(v[M_DEVNAME], 16, "%s", ctx.settings.device_name);
        snprintf(v[M_SCAN], 16, "%s", ctx.scanning ? "ON" : ">");
        snprintf(v[M_NOAA], 16, "%s", ctx.noaa_ch == 0 ? "OFF" : rt1000_noaa_name(ctx.noaa_ch));
        snprintf(v[M_FM], 16, ">");
        snprintf(v[M_APRS_SSID], 16, "%d", ctx.settings.aprs_ssid);
        snprintf(v[M_APRS_PATH], 16, "%s", APRS_PATHS[ctx.settings.aprs_path_idx]);
        snprintf(v[M_APRS_DETAIL], 16, ">");
        snprintf(v[M_DTMF], 16, "%s", ctx.settings.dtmf_code);
        snprintf(v[M_MDC], 16, "%s", ctx.settings.mdc_id);
        snprintf(v[M_VOX], 16, "%s", ctx.settings.vox == 0 ? "OFF" : "NIV");
        if(ctx.settings.vox) snprintf(v[M_VOX], 16, "%d", ctx.settings.vox);
        snprintf(v[M_WIDTH], 16, "%s", ctx.settings.wide ? "WIDE" : "NARROW");
        snprintf(v[M_BURST], 16, "%s", ctx.settings.tone_burst ? "1750Hz" : "OFF");
        rt1000_tone_label(ctx.settings.tone_rx, v[M_TONE_RX], 16);
        rt1000_tone_label(ctx.settings.tone_tx, v[M_TONE_TX], 16);
        {
            uint32_t txf = tx_result_freq();
            if(ctx.settings.shift_dir == 0) snprintf(v[M_SHIFT], 16, "0");
            else if(txf == 0) snprintf(v[M_SHIFT], 16, "PROHIB TX");
            else snprintf(v[M_SHIFT], 16, "%c%lu.%04lu",
                          ctx.settings.shift_dir > 0 ? '+' : '-',
                          (unsigned long)(txf / 1000000),
                          (unsigned long)((txf % 1000000) / 100));
        }
        {
            int oi = 0;
            for(int i = 0; i < NOFFSETS; i++) if(OFFSETS[i] == ctx.settings.offset_hz) oi = i;
            snprintf(v[M_OFFSET], 16, "%s", OFFSET_NAMES[oi]);
        }
        snprintf(v[M_TALK], 16, "%s", ctx.settings.talkaround ? "ON" : "OFF");
        snprintf(v[M_REVERSE], 16, "%s", ctx.reverse ? "ON" : "OFF");
        snprintf(v[M_MONITOR], 16, "%s", ctx.settings.monitor ? "ON" : "OFF");
        snprintf(v[M_GPSFMT], 16, "%s", GPS_FMT_NAMES[ctx.settings.gps_fmt]);
        snprintf(v[M_TNC], 16, "%s", ctx.settings.tnc ? "ON" : "OFF");
        snprintf(v[M_ZONE], 16, "%s", ctx.settings.zone_names[ctx.zone]);
        snprintf(v[M_TXPRIO], 16, "%s", TXPRIO_NAMES[ctx.settings.tx_priority]);
        snprintf(v[M_MULTISTBY], 16, "%s", ctx.settings.multi_standby ? "ON" : "OFF");
        snprintf(v[M_ALARM], 16, ">");
        snprintf(v[M_BACKUP], 16, ">");
        snprintf(v[M_RESET], 16, ">");
        snprintf(v[M_CHAN], 16, ">");
        snprintf(v[M_INFO], 16, ">");
        const char *values[M_COUNT];
        for(int i = 0; i < M_COUNT; i++) values[i] = v[i];
        ui_menu(labels, values, M_COUNT, menu_sel);
        break;
    }
    }
}

/* Aplica los tonos CTCSS/DCS seleccionados al BK4829.
 * Los valores se guardan aunque el registro del chip no este
 * confirmado (ver bk4829.h). */
static void tones_apply(void){
    bk4829_set_tones(0,
        rt1000_tone_is_dcs(ctx.settings.tone_rx) ? 0 : rt1000_tone_value(ctx.settings.tone_rx),
        rt1000_tone_is_dcs(ctx.settings.tone_tx) ? 0 : rt1000_tone_value(ctx.settings.tone_tx),
        rt1000_tone_is_dcs(ctx.settings.tone_rx) ? rt1000_tone_value(ctx.settings.tone_rx) : 0,
        rt1000_tone_is_dcs(ctx.settings.tone_tx) ? rt1000_tone_value(ctx.settings.tone_tx) : 0);
}

/* Cambia el valor del item de menu seleccionado con Left/Right. */
static void menu_change(int dir){
    switch(menu_sel){
    case M_VFO: ctx.active ^= 1; break;
    case M_STEP:
        ctx.step_idx = (ctx.step_idx + dir + (int)NSTEPS) % (int)NSTEPS;
        break;
    case M_POWER:
        ctx.power = (uint8_t)((ctx.power + dir + 3) % 3);
        bk4829_set_tx_power(0, ctx.power);
        break;
    case M_MODE: ctx.mode = (uint8_t)((ctx.mode + dir + 5) % 5); break;
    case M_SQL: ctx.sql = (uint8_t)((ctx.sql + dir + 10) % 10); break;
    case M_GPS: ctx.gps_on = !ctx.gps_on; break;
    case M_APRS: ctx.aprs_idx = (ctx.aprs_idx + dir + 4) % 4; break;
    case M_BACKLIGHT: ctx.backlight = !ctx.backlight; break;
    case M_THEME:
        ui_set_theme(!ui_get_theme());
        ctx.settings.theme = (uint8_t)ui_get_theme();
        break;
    case M_NOAA: {
        ctx.noaa_ch = (ctx.noaa_ch + dir + 8) % 8;
        if(ctx.noaa_ch) set_active_freq(rt1000_noaa_freq(ctx.noaa_ch));
        break;
    }
    case M_APRS_SSID:
        ctx.settings.aprs_ssid = (uint8_t)((ctx.settings.aprs_ssid + dir + 16) % 16);
        break;
    case M_APRS_PATH:
        ctx.settings.aprs_path_idx = (uint8_t)((ctx.settings.aprs_path_idx + dir + NAPRS_PATHS) % NAPRS_PATHS);
        break;
    case M_VOX:
        ctx.settings.vox = (uint8_t)((ctx.settings.vox + dir + 10) % 10);
        bk4829_set_vox(0, ctx.settings.vox);
        break;
    case M_WIDTH:
        ctx.settings.wide = !ctx.settings.wide;
        bk4829_set_bandwidth(0, ctx.settings.wide);
        break;
    case M_BURST:
        ctx.settings.tone_burst = !ctx.settings.tone_burst;
        bk4829_set_tone_burst(0, ctx.settings.tone_burst);
        break;
    case M_TONE_RX: {
        int c = (int)ctx.settings.tone_rx + dir;
        if(c < 0) c = CTCSS_COUNT + DCS_COUNT;
        if(c > CTCSS_COUNT + DCS_COUNT) c = 0;
        ctx.settings.tone_rx = (uint16_t)c;
        tones_apply();
        break;
    }
    case M_TONE_TX: {
        int c = (int)ctx.settings.tone_tx + dir;
        if(c < 0) c = CTCSS_COUNT + DCS_COUNT;
        if(c > CTCSS_COUNT + DCS_COUNT) c = 0;
        ctx.settings.tone_tx = (uint16_t)c;
        tones_apply();
        break;
    }
    case M_SHIFT: {
        int d = (int)ctx.settings.shift_dir + dir;
        if(d > 1) d = -1;
        if(d < -1) d = 1;
        ctx.settings.shift_dir = (int8_t)d;
        break;
    }
    case M_OFFSET: {
        int oi = 0;
        for(int i = 0; i < NOFFSETS; i++) if(OFFSETS[i] == ctx.settings.offset_hz) oi = i;
        oi = (oi + dir + NOFFSETS) % NOFFSETS;
        ctx.settings.offset_hz = OFFSETS[oi];
        break;
    }
    case M_TALK: ctx.settings.talkaround = !ctx.settings.talkaround; break;
    case M_REVERSE: ctx.reverse = !ctx.reverse; break;
    case M_MONITOR: ctx.settings.monitor = !ctx.settings.monitor; break;
    case M_GPSFMT:
        ctx.settings.gps_fmt = (uint8_t)((ctx.settings.gps_fmt + dir + 3) % 3);
        break;
    case M_TNC: ctx.settings.tnc = !ctx.settings.tnc; break;
    case M_ZONE:
        ctx.zone = (ctx.zone + dir + ZONES) % ZONES;
        chan_sel = 0;
        break;
    case M_TXPRIO:
        ctx.settings.tx_priority = (uint8_t)((ctx.settings.tx_priority + dir + 3) % 3);
        break;
    case M_MULTISTBY: ctx.settings.multi_standby = !ctx.settings.multi_standby; break;
    default: break;
    }
}

static void menu_ok(void){
    switch(menu_sel){
    case M_CALLSIGN: edit_start(0); break;
    case M_DEVNAME: edit_start(1); break;
    case M_SCAN:
        ctx.scanning = true;
        ctx.last_scan_ms = hal_millis();
        state = ST_MAIN;
        break;
    case M_FM: state = ST_FM; break;
    case M_APRS_DETAIL: state = ST_APRS; break;
    case M_DTMF: edit_start(2); break;
    case M_MDC: edit_start(3); break;
    case M_ALARM:
        ctx.alarm = true;
        state = ST_ALARM;
        break;
    case M_BACKUP:
        snprintf(ctx.msg_line1, sizeof ctx.msg_line1, "Guardando...");
        snprintf(ctx.msg_line2, sizeof ctx.msg_line2,
                 persist_now() ? "Backup OK en flash" : "Sin flash: no guardado");
        ctx.msg_confirm = 0; ctx.msg_t0 = hal_millis();
        state = ST_MSG;
        break;
    case M_RESET:
        snprintf(ctx.msg_line1, sizeof ctx.msg_line1, "Reset de fabrica?");
        snprintf(ctx.msg_line2, sizeof ctx.msg_line2, "OK borra todo");
        ctx.msg_confirm = 1; ctx.msg_t0 = hal_millis();
        state = ST_MSG;
        break;
    case M_CHAN: chan_sel = 0; state = ST_CHAN; break;
    case M_INFO: state = ST_INFO; break;
    default: break;
    }
}

static void handle_key(int key){
    switch(state){
    case ST_SPLASH:
        state = ST_MAIN;
        break;
    case ST_MAIN:
        if(ctx.scanning){
            /* Cualquier tecla para el scan. */
            ctx.scanning = false;
            break;
        }
        if(key == K_UP || key == K_DOWN){
            int32_t f = (int32_t)active_freq() +
                        (key == K_UP ? (int32_t)STEPS[ctx.step_idx]
                                     : -(int32_t)STEPS[ctx.step_idx]);
            set_active_freq((uint32_t)(f < 0 ? 0 : f));
        } else if(key == K_LEFT || key == K_RIGHT){
            ctx.step_idx = (ctx.step_idx + (key == K_RIGHT ? 1 : (int)NSTEPS - 1)) % (int)NSTEPS;
        } else if(key == K_VM){
            ctx.active ^= 1;
        } else if(key == K_OK){
            menu_sel = 0; state = ST_MENU;
        } else if(key == K_HASH){
            state = ST_GPS;
        } else if(key == K_ABC){
            chan_sel = 0; state = ST_CHAN;
        } else if((key >= 0 && key <= 9) || key == K_STAR){
            entry_buf[0] = (char)(key == K_STAR ? '.' : '0' + key);
            entry_buf[1] = 0;
            state = ST_ENTRY;
        }
        break;
    case ST_ENTRY:
        if(key >= 0 && key <= 9){
            size_t n = strlen(entry_buf);
            if(n < sizeof entry_buf - 1){
                entry_buf[n] = (char)('0' + key); entry_buf[n + 1] = 0;
            }
        } else if(key == K_STAR){
            size_t n = strlen(entry_buf);
            if(n < sizeof entry_buf - 1 && !strchr(entry_buf, '.')){
                entry_buf[n] = '.'; entry_buf[n + 1] = 0;
            }
        } else if(key == K_OK){
            uint32_t hz;
            if(entry_parse(entry_buf, &hz)) set_active_freq(hz);
            state = ST_MAIN;
        } else if(key == K_BACK){
            state = ST_MAIN;
        }
        break;
    case ST_MENU:
        if(key == K_UP) menu_sel = (menu_sel + M_COUNT - 1) % M_COUNT;
        else if(key == K_DOWN) menu_sel = (menu_sel + 1) % M_COUNT;
        else if(key == K_LEFT) menu_change(-1);
        else if(key == K_RIGHT) menu_change(1);
        else if(key == K_OK) menu_ok();
        else if(key == K_BACK){
            persist_now(); /* guarda ajustes al salir del menu */
            state = ST_MAIN;
        }
        break;
    case ST_GPS:
        if(key == K_BACK || key == K_HASH) state = ST_MAIN;
        break;
    case ST_CHAN: {
        int slot = ctx.zone * CHANS_PER_ZONE + chan_sel;
        if(key == K_UP) chan_sel = (chan_sel + CHANS_PER_ZONE - 1) % CHANS_PER_ZONE;
        else if(key == K_DOWN) chan_sel = (chan_sel + 1) % CHANS_PER_ZONE;
        else if(key == K_LEFT || key == K_RIGHT){
            ctx.zone = (ctx.zone + (key == K_RIGHT ? 1 : ZONES - 1)) % ZONES;
            chan_sel = 0;
        }
        else if(key == K_OK){
            if(ctx.chan_used[slot]) set_active_freq(ctx.chans[slot].rx_hz);
            state = ST_MAIN;
        } else if(key == K_ABC){
            rt1000_chan_t *c = &ctx.chans[slot];
            memset(c, 0, sizeof *c);
            c->rx_hz = active_freq();
            c->tx_hz = 0xFFFFFFFFu; /* solo RX por defecto */
            c->rx_tone = ctx.settings.tone_rx;
            c->tx_tone = ctx.settings.tone_tx;
            snprintf(c->name, sizeof c->name, "CH-%03d", slot + 1);
            ctx.chan_used[slot] = true;
            persist_now();
            state = ST_MAIN;
        } else if(key == K_BACK) state = ST_MAIN;
        break;
    }
    case ST_INFO:
        if(key == K_BACK) state = ST_MENU;
        break;
    case ST_FM:
        if(key == K_UP || key == K_DOWN){
            int32_t f = (int32_t)ctx.settings.fm_freq_khz +
                        (key == K_UP ? 100 : -100);
            if(f < 87500) f = 87500;
            if(f > 108000) f = 108000;
            ctx.settings.fm_freq_khz = (uint32_t)f;
            if(ctx.si4732_ok) si4732_tune_fm(ctx.settings.fm_freq_khz);
        } else if(key == K_BACK){
            persist_now();
            state = ST_MAIN;
        }
        break;
    case ST_APRS:
        if(key == K_BACK) state = ST_MENU;
        break;
    case ST_EDIT:
        edit_key(key);
        break;
    case ST_ALARM:
        ctx.alarm = false;
        state = ST_MENU;
        break;
    case ST_MSG:
        if(ctx.msg_confirm == 1 && key == K_OK){
            settings_defaults(&ctx.settings);
            memset(ctx.chans, 0, sizeof ctx.chans);
            memset(ctx.chan_used, 0, sizeof ctx.chan_used);
            persist_now();
            ui_set_theme(ctx.settings.theme);
            state = ST_MENU;
        } else if(key == K_BACK || key == K_OK){
            state = ST_MENU;
        }
        break;
    }
}

int app_main(void){
    rt1000_system_init_documented();
    hal_uart_init_115200();
    hal_spi_init();
    lcd_init();
    /* Ajustes persistentes: si no hay flash valida, defaults. */
    if(!persist_load(&ctx.settings, ctx.chans, ctx.chan_used, CHAN_SLOTS))
        settings_defaults(&ctx.settings);
    ui_set_theme(ctx.settings.theme);
    bk4829_init();
    bk4829_set_freq(0, ctx.vfo_a.freq_hz);
    bk4829_set_tx_power(0, ctx.power);
    bk4829_set_vox(0, ctx.settings.vox);
    bk4829_set_bandwidth(0, ctx.settings.wide);
    bk4829_set_tone_burst(0, ctx.settings.tone_burst);
    tones_apply();
    if(si4732_power_up()){
        uint8_t rev[9];
        ctx.si4732_ok = si4732_get_rev(rev);
        if(ctx.si4732_ok) si4732_tune_fm(ctx.settings.fm_freq_khz);
    }
    splash_t0 = hal_millis();
    last_rssi_ms = splash_t0;
    last_beacon_ms = splash_t0;
    ctx.last_scan_ms = splash_t0;
    draw_all();
    for(;;){
        uint32_t now = hal_millis();
        if(state == ST_SPLASH && now - splash_t0 > 1500){
            state = ST_MAIN;
            draw_all();
        }
        if(state == ST_MSG && ctx.msg_confirm == 0 && now - ctx.msg_t0 > 1200){
            state = ST_MENU;
            draw_all();
        }
        int key = keypad_get_key();
        if(key != KEY_NONE){
            handle_key(key);
            draw_all();
        }
        if(now - last_rssi_ms > 500){
            last_rssi_ms = now;
            ctx.rssi_dbm = bk4829_rssi_dbm(0);
            ctx.batt_mv = hal_adc_batt_mv();
            if(state == ST_MAIN || state == ST_INFO) draw_all();
        }
        if(ctx.scanning && state == ST_MAIN &&
           now - ctx.last_scan_ms >= ctx.settings.scan_interval_ms){
            ctx.last_scan_ms = now;
            set_active_freq(rt1000_scan_next(active_freq(), STEPS[ctx.step_idx]));
            /* Para con senal: umbral segun squelch. */
            int16_t thr = (int16_t)(-120 + ctx.sql * 4);
            if(ctx.rssi_dbm > thr) ctx.scanning = false;
            draw_all();
        }
        if(hal_uart_rx_ready()){
            uint8_t b;
            if(hal_uart_read_byte(&b) == 0) gps_feed_byte(b);
        }
        aprs_beacon_maybe(now);
        hal_delay_ms(50);
    }
    return 0;
}
