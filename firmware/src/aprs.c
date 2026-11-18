#include "aprs.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
size_t rt1000_ax25_addr(uint8_t *out, const char *callsign, uint8_t ssid, int last){
    size_t n=strlen(callsign); if(n>6) n=6;
    for(size_t i=0;i<6;i++){ char c=i<n?callsign[i]:' '; out[i]=(uint8_t)(c<<1); }
    out[6]=(uint8_t)(((ssid&0xF)<<1)|(last?1:0)|0x60);
    return 7;
}
static void fmt_lat(char *b, double lat){
    char ns=lat>=0?'N':'S'; if(lat<0) lat=-lat;
    int d=(int)lat; double m=(lat-d)*60;
    snprintf(b,16,"%02d%05.2f%c",d,m,ns);
}
static void fmt_lon(char *b, double lon){
    char ew=lon>=0?'E':'W'; if(lon<0) lon=-lon;
    int d=(int)lon; double m=(lon-d)*60;
    snprintf(b,16,"%03d%05.2f%c",d,m,ew);
}
size_t rt1000_aprs_position(uint8_t *out, size_t cap, const char *src, uint8_t ssid,
    const char *dst, double lat, double lon, double alt_m, const char *comment){
    return rt1000_aprs_position_path(out, cap, src, ssid, dst, "", lat, lon, alt_m, comment);
}
size_t rt1000_aprs_position_path(uint8_t *out, size_t cap, const char *src, uint8_t ssid,
    const char *dst, const char *path, double lat, double lon, double alt_m, const char *comment){
    /* Frame: dst(7) src(7) [path...] 0x03 0xF0 + "!lat/long..." */
    uint8_t *p=out;
    if(cap<64) return 0;
    bool has_path = path && *path;
    p+=rt1000_ax25_addr(p,dst,0,0);
    p+=rt1000_ax25_addr(p,src,ssid,has_path?0:1);
    if(has_path){
        char tmp[48];
        size_t n = strlen(path); if(n >= sizeof tmp) n = sizeof tmp - 1;
        memcpy(tmp, path, n); tmp[n] = 0;
        /* Cuenta entradas para marcar la ultima. */
        int total = 1;
        for(char *q = tmp; *q; q++) if(*q == ',') total++;
        int idx = 0;
        char *save = NULL; (void)save;
        char *tok = tmp;
        for(char *q = tmp; ; q++){
            if(*q == ',' || *q == 0){
                char saved = *q; *q = 0;
                char *dash = strchr(tok, '-');
                int ps = dash ? atoi(dash + 1) : 0;
                if(dash) *dash = 0;
                p += rt1000_ax25_addr(p, tok, (uint8_t)ps, idx == total - 1 ? 1 : 0);
                idx++;
                if(saved == 0) break;
                tok = q + 1;
            }
        }
    }
    *p++=0x03; *p++=0xF0;
    char la[16], lo[16];
    fmt_lat(la,lat); fmt_lon(lo,lon);
    int n=snprintf((char*)p, cap-(size_t)(p-out), "!%s/%s-%s", la, lo, comment?comment:"");
    if(n<0) return 0;
    p+=n; (void)alt_m; /* altitud: extension /A= en el comentario, no en el cuerpo base */
    return (size_t)(p-out);
}
uint16_t rt1000_ax25_fcs(const uint8_t *data, size_t len){
    uint16_t fcs = 0xFFFF;
    for(size_t i = 0; i < len; i++){
        fcs ^= data[i];
        for(int b = 0; b < 8; b++)
            fcs = (fcs & 1) ? (uint16_t)((fcs >> 1) ^ 0x8408) : (uint16_t)(fcs >> 1);
    }
    return (uint16_t)~fcs;
}
bool rt1000_ax25_fcs_ok(const uint8_t *frame, size_t len){
    if(len < 2) return false;
    uint16_t want = rt1000_ax25_fcs(frame, len - 2);
    uint16_t got = (uint16_t)(frame[len - 2] | (frame[len - 1] << 8));
    return want == got;
}
