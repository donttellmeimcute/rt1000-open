#include "aprs.h"
#include <string.h>
#include <stdio.h>
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
    /* Frame: dst(7) src(7) [path] 0x03 0xF0 + "!lat/long..." */
    uint8_t *p=out;
    if(cap<64) return 0;
    p+=rt1000_ax25_addr(p,dst,0,0);
    p+=rt1000_ax25_addr(p,src,ssid,1);
    *p++=0x03; *p++=0xF0;
    char la[16], lo[16];
    fmt_lat(la,lat); fmt_lon(lo,lon);
    int n=snprintf((char*)p, cap-(size_t)(p-out), "!%s/%s-%s", la, lo, comment?comment:"");
    if(n<0) return 0;
    p+=n; (void)alt_m; /* altitud: extension /A= en el comentario, no en el cuerpo base */
    return (size_t)(p-out);
}
