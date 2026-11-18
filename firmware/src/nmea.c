#include "nmea.h"
#include <string.h>
#include <stdlib.h>
uint8_t rt1000_nmea_checksum(const char *s){
    uint8_t c=0; if(*s=='$') s++;
    while(*s && *s!='*') c^=(uint8_t)*s++;
    return c;
}
static double ddmm_to_deg(const char *f, char hemi){
    double v=atof(f); int d=(int)(v/100); double m=v-d*100;
    double deg=d+m/60.0; if(hemi=='S'||hemi=='W') deg=-deg; return deg;
}
static int hexv(char c){ return (c>='0'&&c<='9')?c-'0':(c>='A'&&c<='F')?c-'A'+10:(c>='a'&&c<='f')?c-'a'+10:-1; }
bool rt1000_nmea_parse(const char *s, rt1000_gps_t *out){
    if(!s||s[0]!='$') return false;
    const char *star=strchr(s,'*'); if(!star||hexv(star[1])<0||hexv(star[2])<0) return false;
    if(rt1000_nmea_checksum(s)!=(uint8_t)(hexv(star[1])*16+hexv(star[2]))) return false;
    memset(out,0,sizeof *out);
    char buf[160]; size_t n=star-s; if(n>=sizeof buf) return false;
    memcpy(buf,s,n); buf[n]=0;
    char *f[16]; int nf=0; char *save=NULL;
    /* strtok_r no es portable en todos los ARM: split manual */
    char *p=buf; f[nf++]=p;
    for(char *q=p; *q && nf<16; q++) if(*q==','){ *q=0; f[nf++]=q+1; }
    (void)save;
    if(nf<2) return false;
    if(!strcmp(f[0],"$GPRMC") && nf>=10){
        if(strlen(f[1])>=6){ out->hour=(f[1][0]-'0')*10+(f[1][1]-'0'); out->min=(f[1][2]-'0')*10+(f[1][3]-'0'); out->sec=(f[1][4]-'0')*10+(f[1][5]-'0'); }
        if(f[2][0]!='A') return false;
        out->fix=true;
        out->lat=ddmm_to_deg(f[3],f[4][0]); out->lon=ddmm_to_deg(f[5],f[6][0]);
        out->speed_kmh=atof(f[7])*1.852; out->course=atof(f[8]);
        if(strlen(f[9])>=6){ out->day=(f[9][0]-'0')*10+(f[9][1]-'0'); out->month=(f[9][2]-'0')*10+(f[9][3]-'0'); out->year=2000+(f[9][4]-'0')*10+(f[9][5]-'0'); }
        out->valid=true; return true;
    }
    if(!strcmp(f[0],"$GPGGA") && nf>=10){
        if(strlen(f[1])>=6){ out->hour=(f[1][0]-'0')*10+(f[1][1]-'0'); out->min=(f[1][2]-'0')*10+(f[1][3]-'0'); out->sec=(f[1][4]-'0')*10+(f[1][5]-'0'); }
        if(f[6][0]=='0') return false;
        out->fix=true;
        out->lat=ddmm_to_deg(f[2],f[3][0]); out->lon=ddmm_to_deg(f[4],f[5][0]);
        out->alt_m=atof(f[9]); out->valid=true; return true;
    }
    return false;
}
