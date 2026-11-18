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
#include "storage.h"
#include <string.h>
#include <stdio.h>

#define FW_VERSION "v1.0-open"
#define CHAN_SLOTS 16

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

typedef enum { ST_SPLASH, ST_MAIN, ST_MENU, ST_GPS, ST_CHAN, ST_INFO, ST_ENTRY } app_state_t;

static const uint32_t STEPS[] = {5000, 6250, 10000, 12500, 25000};
#define NSTEPS (sizeof STEPS / sizeof STEPS[0])
static const char *APRS_OPTS[] = {"OFF", "30s", "60s", "5min"};
static const uint32_t APRS_SECS[] = {0, 30, 60, 300};

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
} app_ctx_t;

static app_ctx_t ctx = {
    .vfo_a = {146520000u, 0, 2}, .vfo_b = {435000000u, 0, 2},
    .active = 0, .mode = 0, .power = 2, .sql = 2, .step_idx = 3,
    .gps_on = true, .aprs_idx = 0, .backlight = true,
};
static char callsign[7] = "XE1ARS"; /* configurable, no quemado en flash */
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
    s->gps_sats = 0; /* GGA con sats aun no se extrae en nmea.c */
    s->callsign = callsign;
    s->aprs_state = APRS_OPTS[ctx.aprs_idx];
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

static void aprs_beacon_maybe(uint32_t now_ms){
    if(ctx.aprs_idx == 0 || !ctx.gps.fix) return;
    uint32_t iv = APRS_SECS[ctx.aprs_idx] * 1000u;
    if(now_ms - last_beacon_ms < iv) return;
    last_beacon_ms = now_ms;
    uint8_t frame[128];
    /* El frame se envia por AFSK/BK4829 cuando el driver de
     * audio este confirmado; hoy solo se construye. */
    (void)rt1000_aprs_position(frame, sizeof frame, callsign, 7, "APRS",
                               ctx.gps.lat, ctx.gps.lon, ctx.gps.alt_m,
                               "RT-1000Pro open");
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

static void draw_all(void){
    ui_state_t s;
    ui_state_fill(&s);
    switch(state){
    case ST_SPLASH: ui_splash(callsign, FW_VERSION); break;
    case ST_MAIN: ui_main(&s); break;
    case ST_ENTRY: ui_entry(&s, entry_buf); break;
    case ST_GPS: ui_gps_screen(&s); break;
    case ST_CHAN: ui_chan_screen(ctx.chans, ctx.chan_used, CHAN_SLOTS, chan_sel); break;
    case ST_INFO: ui_info_screen(callsign, FW_VERSION); break;
    case ST_MENU: {
        static const char *labels[] = {
            "VFO activo", "Paso", "Potencia", "Modo", "Squelch",
            "GPS", "APRS beacon", "Luz pantalla", "Canales", "Info"};
        char v0[8], v1[12], v2[8], v3[8], v4[8], v5[8], v6[8], v7[8];
        snprintf(v0, sizeof v0, "%s", ctx.active == 0 ? "A" : "B");
        snprintf(v1, sizeof v1, "%lu.%01luk",
                 (unsigned long)(STEPS[ctx.step_idx] / 1000),
                 (unsigned long)((STEPS[ctx.step_idx] % 1000) / 100));
        snprintf(v2, sizeof v2, "%s", UI_POWER_NAMES[ctx.power]);
        snprintf(v3, sizeof v3, "%s", UI_MODE_NAMES[ctx.mode]);
        snprintf(v4, sizeof v4, "%d", ctx.sql);
        snprintf(v5, sizeof v5, "%s", ctx.gps_on ? "ON" : "OFF");
        snprintf(v6, sizeof v6, "%s", APRS_OPTS[ctx.aprs_idx]);
        snprintf(v7, sizeof v7, "%s", ctx.backlight ? "ON" : "OFF");
        const char *values[] = {v0, v1, v2, v3, v4, v5, v6, v7, ">", ">"};
        ui_menu(labels, values, 10, menu_sel);
        break;
    }
    }
}

/* Cambia el valor del item de menu seleccionado con Left/Right. */
static void menu_change(int dir){
    switch(menu_sel){
    case 0: ctx.active ^= 1; break;
    case 1:
        ctx.step_idx = (ctx.step_idx + dir + (int)NSTEPS) % (int)NSTEPS;
        break;
    case 2:
        ctx.power = (uint8_t)((ctx.power + dir + 3) % 3);
        bk4829_set_tx_power(0, ctx.power);
        break;
    case 3: ctx.mode = (uint8_t)((ctx.mode + dir + 5) % 5); break;
    case 4: ctx.sql = (uint8_t)((ctx.sql + dir + 10) % 10); break;
    case 5: ctx.gps_on = !ctx.gps_on; break;
    case 6: ctx.aprs_idx = (ctx.aprs_idx + dir + 4) % 4; break;
    case 7: ctx.backlight = !ctx.backlight; break;
    default: break;
    }
}

static void handle_key(int key){
    uint32_t now = hal_millis();
    (void)now;
    switch(state){
    case ST_SPLASH:
        state = ST_MAIN;
        break;
    case ST_MAIN:
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
        if(key == K_UP) menu_sel = (menu_sel + 9) % 10;
        else if(key == K_DOWN) menu_sel = (menu_sel + 1) % 10;
        else if(key == K_LEFT) menu_change(-1);
        else if(key == K_RIGHT) menu_change(1);
        else if(key == K_OK){
            if(menu_sel == 8){ chan_sel = 0; state = ST_CHAN; }
            else if(menu_sel == 9){ state = ST_INFO; }
        }
        else if(key == K_BACK) state = ST_MAIN;
        break;
    case ST_GPS:
        if(key == K_BACK || key == K_HASH) state = ST_MAIN;
        break;
    case ST_CHAN:
        if(key == K_UP) chan_sel = (chan_sel + CHAN_SLOTS - 1) % CHAN_SLOTS;
        else if(key == K_DOWN) chan_sel = (chan_sel + 1) % CHAN_SLOTS;
        else if(key == K_OK){
            if(ctx.chan_used[chan_sel]) set_active_freq(ctx.chans[chan_sel].rx_hz);
            state = ST_MAIN;
        } else if(key == K_ABC){
            rt1000_chan_t *c = &ctx.chans[chan_sel];
            memset(c, 0, sizeof *c);
            c->rx_hz = active_freq();
            c->tx_hz = 0xFFFFFFFFu; /* solo RX por defecto */
            snprintf(c->name, sizeof c->name, "CH-%03d", chan_sel + 1);
            ctx.chan_used[chan_sel] = true;
            state = ST_MAIN;
        } else if(key == K_BACK) state = ST_MAIN;
        break;
    case ST_INFO:
        if(key == K_BACK) state = ST_MENU;
        break;
    }
}

int app_main(void){
    rt1000_system_init_documented();
    hal_uart_init_115200();
    hal_spi_init();
    lcd_init();
    bk4829_init();
    bk4829_set_freq(0, ctx.vfo_a.freq_hz);
    bk4829_set_tx_power(0, ctx.power);
    splash_t0 = hal_millis();
    last_rssi_ms = splash_t0;
    last_beacon_ms = splash_t0;
    draw_all();
    for(;;){
        uint32_t now = hal_millis();
        if(state == ST_SPLASH && now - splash_t0 > 1500){
            state = ST_MAIN;
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
            if(state == ST_MAIN) draw_all();
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
