/* SPDX-License-Identifier: GPL-3.0-only
 * Validated, bounded Windjammer native presentation. No standalone project reads. */
#ifndef CAFA_NATIVE_VIEW_H
#define CAFA_NATIVE_VIEW_H
#include <stdint.h>
#define CAFA_NATIVE_KIND 7u
#define CAFA_NATIVE_HEADER 256u
#define CAFA_NATIVE_MAX 3072u
#define CAFA_NATIVE_SCREEN 6u
#define CAFA_NATIVE_VARIANT 7u
#define CAFA_NATIVE_FAMILY 8u
#define CAFA_NATIVE_TITLE 33u
#define CAFA_NATIVE_SUBTITLE 57u
#define CAFA_NATIVE_FOOTER 81u
#define CAFA_NATIVE_CONTROLS 112u
#define CAFA_NATIVE_ROWS 1u
#define CAFA_NATIVE_NOTES 2u
#define CAFA_NATIVE_CURVE 3u
#define CAFA_NATIVE_MARKERS 4u
#define CAFA_NATIVE_CELLS 5u
#define CAFA_NATIVE_KEYBOARD 6u
#define CAFA_NATIVE_TEXT 7u
#define CAFA_NATIVE_SERIES 8u
#define CAFA_NATIVE_EDGES 9u
#define CAFA_NATIVE_LEDS 10u
#define CAFA_NATIVE_PREFS 11u
#define CAFA_NATIVE_ARP_GRID 12u
#define CAFA_NATIVE_TRANSIENT_BOX 13u
#define CAFA_NATIVE_CHROME 14u
#define CAFA_NATIVE_ICON_NONE 0u
#define CAFA_NATIVE_ICON_NOTE 1u
#define CAFA_NATIVE_ICON_STEP 2u
#define CAFA_NATIVE_ICON_WAVE 3u
#define CAFA_NATIVE_ICON_ENV 4u
#define CAFA_NATIVE_ICON_LFO 5u
#define CAFA_NATIVE_ICON_FX 6u
#define CAFA_NATIVE_ICON_MIX 7u
#define CAFA_NATIVE_ICON_SAVE 8u
#define CAFA_NATIVE_ICON_PLAY 9u
#define CAFA_NATIVE_ICON_REC 10u
#define CAFA_NATIVE_ICON_LOCK 11u
#define CAFA_NATIVE_ICON_MENU 12u
#define CAFA_NATIVE_VARIANT_NORMAL 0u
#define CAFA_NATIVE_VARIANT_DRUM 1u
#define CAFA_NATIVE_VARIANT_ANALOG 2u
#define CAFA_NATIVE_VARIANT_FM_OPERATOR 3u
#define CAFA_NATIVE_VARIANT_DRUM_SOUND 4u
#define CAFA_NATIVE_VARIANT_SAMPLE_WAVE 5u
#define CAFA_NATIVE_VARIANT_DRUM_LANES 6u
/* Append-only native presentation IDs, independent of PAGES[] indices. */
enum {
    CAFA_SCREEN_ENV = 0u,
    CAFA_SCREEN_ENV2 = 1u,
    CAFA_SCREEN_LFO = 2u,
    CAFA_SCREEN_LFO2 = 3u,
    CAFA_SCREEN_MOD = 4u,
    CAFA_SCREEN_FX = 5u,
    CAFA_SCREEN_FX_DELAY = 6u,
    CAFA_SCREEN_FX_REVERB = 7u,
    CAFA_SCREEN_DRIVE = 8u,
    CAFA_SCREEN_INSERT = 9u,
    CAFA_SCREEN_SLICER = 10u,
    CAFA_SCREEN_DUCK = 11u,
    CAFA_SCREEN_SCALE = 12u,
    CAFA_SCREEN_CHORD = 13u,
    CAFA_SCREEN_FOLLOW = 14u,
    CAFA_SCREEN_EDIT1 = 15u,
    CAFA_SCREEN_EDIT2 = 16u,
    CAFA_SCREEN_OPERATOR = 17u,
    CAFA_SCREEN_ALL_OPS = 18u,
    CAFA_SCREEN_LANES = 19u,
    CAFA_SCREEN_SOUND = 20u,
    CAFA_SCREEN_LANE_MIX = 21u,
    CAFA_SCREEN_SLICES = 22u,
    CAFA_SCREEN_SAMPLES = 23u,
    CAFA_SCREEN_OP1_ENV = 24u,
    CAFA_SCREEN_OP2_ENV = 25u,
    CAFA_SCREEN_OP3_ENV = 26u,
    CAFA_SCREEN_OP4_ENV = 27u,
    CAFA_SCREEN_OP_LEVEL = 28u,
    CAFA_SCREEN_VOICE = 29u,
    CAFA_SCREEN_VOICE2 = 30u,
    CAFA_SCREEN_MIXER = 31u,
    CAFA_SCREEN_DELAY = 32u,
    CAFA_SCREEN_REVERB = 33u,
    CAFA_SCREEN_CHORUS = 34u,
    CAFA_SCREEN_GLOBAL = 35u,
    CAFA_SCREEN_PRESETS = 36u,
    CAFA_SCREEN_USER = 37u,
    CAFA_SCREEN_PATTERNS = 38u,
    CAFA_SCREEN_PROJECT = 39u,
    CAFA_SCREEN_ARP = 40u,
    CAFA_SCREEN_ARP2 = 41u,
    CAFA_SCREEN_NOTES = 42u,
    CAFA_SCREEN_FEEL = 43u,
    CAFA_SCREEN_PATTERN = 44u,
    CAFA_SCREEN_AUTOMATION = 45u,
    CAFA_SCREEN_SONG = 46u,
    CAFA_SCREEN_HOME = 47u,
    CAFA_SCREEN_SCOPE = 48u,
    CAFA_SCREEN_FX_LAYER = 49u,
    CAFA_SCREEN_GLO_LAYER = 50u,
    CAFA_SCREEN_SEL_LAYER = 51u,
    CAFA_SCREEN_EDIT_LAYER = 52u,
    CAFA_SCREEN_SEQ_LAYER = 53u,
    CAFA_SCREEN_REC_LAYER = 54u,
    CAFA_SCREEN_ARP_LAYER = 55u,
    CAFA_SCREEN_SHIFT = 56u,
    CAFA_SCREEN_MENU_DISPLAY = 57u,
    CAFA_SCREEN_MENU_CONTROL = 58u,
    CAFA_SCREEN_MENU_AUDIO = 59u,
    CAFA_SCREEN_MENU_SYSTEM = 60u,
    CAFA_SCREEN_MENU_MIDI = 61u,
    CAFA_SCREEN_ABOUT = 62u,
    CAFA_SCREEN_CONFIRM = 63u,
    CAFA_SCREEN_NAME = 64u,
    CAFA_SCREEN_UNDO = 65u,
    CAFA_SCREEN_TAKE = 66u,
    CAFA_SCREEN_TAKE_PICKER = 67u,
    CAFA_SCREEN_COUNT_IN = 68u,
    CAFA_SCREEN_UPDATE = 69u,
    CAFA_SCREEN_CALIBRATION = 70u,
    CAFA_SCREEN_RESET_SETTINGS = 71u,
    CAFA_SCREEN_APP_MENU = 72u
};
static uint32_t cafa_native_wire_u14(const uint8_t *p) { return p[0] | (uint32_t)p[1]<<7; }
static int cafa_native_wire_ascii(const uint8_t *p,uint32_t n) {
    uint32_t i;for(i=0;i<n;i++)if(p[i]<32u || p[i]>126u)return 0;return 1;
}
static int cafa_native_blank(const uint8_t *p,uint32_t n) {
    uint32_t i;for(i=0;i<n;i++)if(p[i]!=' ')return 0;return 1;
}
/* -1 means an overlay may keep its originating physical family. */
static int cafa_native_family(uint32_t s) {
    if(s==CAFA_SCREEN_APP_MENU)return 6;
    if(s<=1u)return 2;
    if(s<=4u)return 3;
    if(s<=11u)return 0;
    if(s<=14u)return 1;
    if(s<=30u)return 4;
    if(s<=35u)return 5;
    if(s<=39u)return 7;
    if(s<=41u)return 8;
    if(s<=46u)return 9;
    if(s<=48u)return 6;
    if(s==49u)return 0;
    if(s==50u)return 5;
    if(s==51u)return 1;
    if(s==52u)return 4;
    if(s==53u)return 9;
    if(s==54u)return 11;
    if(s==55u)return 8;
    if(s>=57u && s<=62u || s==70u || s==71u)return 6;
    if(s==56u || s>=63u && s<=69u)return -1;
    return -2;
}
static uint32_t cafa_native_tags(uint32_t s) {
    if(s==CAFA_SCREEN_APP_MENU)return 1u<<CAFA_NATIVE_ROWS;
    if(s>=6u && s<=9u || s>=32u && s<=34u)
        return (1u<<CAFA_NATIVE_ROWS)|(1u<<CAFA_NATIVE_CURVE)|(1u<<CAFA_NATIVE_MARKERS)|
               (1u<<CAFA_NATIVE_CELLS)|(1u<<CAFA_NATIVE_TEXT)|(1u<<CAFA_NATIVE_SERIES);
    if(s>=15u && s<=18u || s>=24u && s<=28u)
        return (1u<<CAFA_NATIVE_ROWS)|(1u<<CAFA_NATIVE_CURVE)|
               (1u<<CAFA_NATIVE_MARKERS)|(1u<<CAFA_NATIVE_TEXT)|
               (1u<<CAFA_NATIVE_SERIES)|(1u<<CAFA_NATIVE_EDGES);
    if(s==42u || s==43u)return (1u<<CAFA_NATIVE_NOTES)|(1u<<CAFA_NATIVE_CELLS)|
                              (1u<<CAFA_NATIVE_ROWS)|(1u<<CAFA_NATIVE_KEYBOARD);
    if(s==10u || s==44u || s==53u)return (1u<<CAFA_NATIVE_CELLS)|(1u<<CAFA_NATIVE_ROWS);
    if(s>=12u && s<=14u)return (1u<<CAFA_NATIVE_KEYBOARD)|(1u<<CAFA_NATIVE_ROWS);
    if(s<=1u || s>=24u && s<=27u || s==2u || s==3u)return (1u<<CAFA_NATIVE_CURVE)|(1u<<CAFA_NATIVE_MARKERS);
    if(s==48u)return (1u<<CAFA_NATIVE_CURVE)|(1u<<CAFA_NATIVE_MARKERS)|(1u<<CAFA_NATIVE_ROWS);
    if(s==22u)return (1u<<CAFA_NATIVE_MARKERS)|(1u<<CAFA_NATIVE_ROWS)|(1u<<CAFA_NATIVE_CURVE);
    if(s==45u || s==4u)return (1u<<CAFA_NATIVE_ROWS)|(1u<<CAFA_NATIVE_CURVE);
    if(s==49u)return (1u<<CAFA_NATIVE_ROWS)|(1u<<CAFA_NATIVE_CELLS)|(1u<<CAFA_NATIVE_TEXT);
    if(s==50u || s==55u)return (1u<<CAFA_NATIVE_ROWS)|(1u<<CAFA_NATIVE_CELLS);
    if(s==51u)return (1u<<CAFA_NATIVE_ROWS)|(1u<<CAFA_NATIVE_KEYBOARD)|(1u<<CAFA_NATIVE_TEXT);
    if(s==56u)return 1u<<CAFA_NATIVE_ROWS;
    if(s==54u)return (1u<<CAFA_NATIVE_ROWS)|(1u<<CAFA_NATIVE_TEXT);
    if(s==68u)return (1u<<CAFA_NATIVE_CELLS)|(1u<<CAFA_NATIVE_TEXT);
    if(s==52u)return (1u<<CAFA_NATIVE_ROWS)|(1u<<CAFA_NATIVE_CELLS)|(1u<<CAFA_NATIVE_TEXT);
    if(s==64u || s==65u || s==66u)return (1u<<CAFA_NATIVE_ROWS)|(1u<<CAFA_NATIVE_TEXT);
    if(s==67u)return (1u<<CAFA_NATIVE_NOTES)|(1u<<CAFA_NATIVE_CURVE)|(1u<<CAFA_NATIVE_CELLS)|(1u<<CAFA_NATIVE_TEXT);
    if(s==62u || s==63u || s==69u)return 1u<<CAFA_NATIVE_TEXT;
    return (1u<<CAFA_NATIVE_ROWS)|(1u<<CAFA_NATIVE_CURVE)|(1u<<CAFA_NATIVE_MARKERS)|
           (1u<<CAFA_NATIVE_CELLS)|(1u<<CAFA_NATIVE_TEXT);
}
static int cafa_native_view_for_caps(const uint8_t *b,uint32_t n,int panel,int chrome) {
    uint32_t i,s,part,parts,first,rows,row,seen=0,at;
    int family;
    if(!b || n<CAFA_NATIVE_HEADER || n>CAFA_NATIVE_MAX || b[0]!=CAFA_NATIVE_KIND)return 0;
    for(i=0;i<n;i++)if(b[i]&128u)return 0;
    s=b[CAFA_NATIVE_SCREEN];family=cafa_native_family(s);
    part=cafa_native_wire_u14(b+13);parts=cafa_native_wire_u14(b+15);
    first=cafa_native_wire_u14(b+26);rows=cafa_native_wire_u14(b+28);row=cafa_native_wire_u14(b+24);
    if(s==CAFA_SCREEN_APP_MENU && (!panel || b[1]>=8u || cafa_native_wire_u14(b+11)!=5u))return 0;
    if(b[1]>15u || (s>=15u && s<=30u ? b[7]>6u : b[7]>1u) || family==-2 ||
       (family==-1 ? b[8]>11u : b[8]!=(uint32_t)family) ||
       !cafa_native_wire_u14(b+11) || cafa_native_wire_u14(b+9)>=cafa_native_wire_u14(b+11) ||
       !parts || part>=parts || b[19]>16u ||
       (b[19] && cafa_native_wire_u14(b+17)+b[19]>16383u) ||
       (!rows ? first || row!=16383u : first>=rows || (row!=16383u && row>=rows)) ||
       b[32]>7u || !cafa_native_wire_ascii(b+33,79u))return 0;
    if((s>=57u && s<=62u || s==70u || s==71u) && (!(b[1]&8u) || n!=CAFA_NATIVE_HEADER))return 0;
    for(at=CAFA_NATIVE_CONTROLS;at<CAFA_NATIVE_HEADER;at+=36u) {
        const uint8_t *q=b+at;
        if(!cafa_native_wire_ascii(q,30u) || q[32]>4u || q[33]>15u ||
           q[34]>CAFA_NATIVE_ICON_MENU || q[35])return 0;
        if(!q[32] && (!cafa_native_blank(q,30u) || cafa_native_wire_u14(q+30) || q[33] || q[34]))return 0;
    }
    for(at=CAFA_NATIVE_HEADER;at<n;) {
        uint32_t tag,len,p,count,base,stride,limit,j;
        if(at+3u>n)return 0;
        tag=b[at];len=cafa_native_wire_u14(b+at+1);p=at+3u;
        if(tag<1u || tag>CAFA_NATIVE_CHROME || (seen&(1u<<tag)) || (tag<CAFA_NATIVE_LEDS && !(cafa_native_tags(s)&(1u<<tag))) || p+len>n)return 0;
        seen|=1u<<tag;
        if(tag==CAFA_NATIVE_CHROME) {
            if(!chrome || !panel || s>CAFA_SCREEN_SCOPE || len!=56u || b[p+2]>2u ||
               !cafa_native_wire_ascii(b+p+3,9u) || b[p+12]>1u ||
               !cafa_native_wire_ascii(b+p+13,40u) ||
               ((b[p+53]|b[p+54])&~27u) || b[p+55])return 0;
            at=p+len;continue;
        }
        if(tag==CAFA_NATIVE_TRANSIENT_BOX) {
            if(!panel || s>CAFA_SCREEN_SCOPE || s==CAFA_SCREEN_MIXER || len!=67u ||
               b[p]>=parts || b[p+1]>1u || b[p+66]>3u ||
               !cafa_native_wire_ascii(b+p+2u,64u))return 0;
            at=p+len;continue;
        }
        if(tag==CAFA_NATIVE_ARP_GRID) {
            if(!panel || (s!=CAFA_SCREEN_ARP && s!=CAFA_SCREEN_ARP2) || len<9u ||
               b[p]>14u || !b[p+1] || b[p+1]>4u || b[p+3]>100u || b[p+5]>1u ||
               !b[p+6] || b[p+6]>64u || !b[p+7] || b[p+7]>65u || len!=8u+b[p+7])return 0;
            at=p+len;continue;
        }
        if(tag==CAFA_NATIVE_PREFS) {
            if(!panel || len<2u || b[p]>5u || b[p+1]>15u ||
               len!=((b[p+1]&4u)?35u:2u))return 0;
            for(j=4;j<len;j+=3)if(b[p+j]>3u)return 0;
            at=p+len;continue;
        }
        if(tag==CAFA_NATIVE_LEDS) {
            if(!panel || p+len!=n || len!=29u || b[p+28]>1u)return 0;
            for(j=0;j<28;j+=7)if(b[p+j+2]>1u || b[p+j+6]>63u)return 0;
            at=p+len;continue;
        }
        if(tag==CAFA_NATIVE_SERIES) {
            uint32_t count,used=0,at2;
            if(!len)return 0;
            count=b[p];if(count>6u)return 0;
            at2=p+1u;
            for(j=0;j<count;j++) {
                uint32_t id,points;
                if(at2+3u>p+len)return 0;
                id=b[at2];points=b[at2+2u];
                uint32_t fm=(s>=17u && s<=18u || s>=24u && s<=28u ||
                             (s==15u || s==16u) && b[7]==CAFA_NATIVE_VARIANT_FM_OPERATOR);
                if(id>5u || (used&(1u<<id)) || b[at2+1u]>7u || (fm && points>12u) ||
                   at2+3u+6u*points>p+len)return 0;
                used|=1u<<id;at2+=3u+6u*points;
            }
            if(at2!=p+len)return 0;
            at=p+len;continue;
        }
        if(tag==CAFA_NATIVE_EDGES) {
            uint32_t count;
            if(!len)return 0;
            count=b[p];if(count>36u || len!=1u+5u*count)return 0;
            for(j=0;j<count;j++) {
                const uint8_t *q=b+p+1u+5u*j;
                if(q[0]>5u || q[1]>5u || q[2]>1u)return 0;
            }
            at=p+len;continue;
        }
        if(tag==CAFA_NATIVE_KEYBOARD) {
            if(len!=20u || (b[p+18]&0x7cu))return 0;
            at=p+len;continue;
        }
        base=(tag==CAFA_NATIVE_ROWS || tag==CAFA_NATIVE_MARKERS || tag==CAFA_NATIVE_TEXT)?1u:2u;
        if(len<base)return 0;
        count=base==1u?b[p]:cafa_native_wire_u14(b+p);
        stride=tag==CAFA_NATIVE_ROWS?48u:tag==CAFA_NATIVE_NOTES?7u:
               tag==CAFA_NATIVE_CURVE?4u:tag==CAFA_NATIVE_MARKERS?13u:
               tag==CAFA_NATIVE_CELLS?7u:24u;
        limit=tag==CAFA_NATIVE_ROWS || tag==CAFA_NATIVE_TEXT?16u:tag==CAFA_NATIVE_MARKERS?32u:256u;
        if(count>limit || len!=base+count*stride)return 0;
        if(s==CAFA_SCREEN_APP_MENU && tag==CAFA_NATIVE_ROWS &&
           (!count || count>6u || first+count>rows || row<first || row>=first+count))return 0;
        for(j=0;j<count;j++) {
            const uint8_t *q=b+p+base+j*stride;
            if(tag==CAFA_NATIVE_ROWS && (!cafa_native_wire_ascii(q,40u) || q[47]))return 0;
            if(tag==CAFA_NATIVE_NOTES && (!cafa_native_wire_u14(q+3) || q[5]>63u))return 0;
            if(tag==CAFA_NATIVE_MARKERS && !cafa_native_wire_ascii(q+5,8u))return 0;
            if(tag==CAFA_NATIVE_CELLS && !cafa_native_wire_ascii(q+3,4u))return 0;
            if(tag==CAFA_NATIVE_TEXT && !cafa_native_wire_ascii(q,24u))return 0;
        }
        at=p+len;
    }
    return s!=CAFA_SCREEN_APP_MENU || (seen&(1u<<CAFA_NATIVE_ROWS));
}
/* Use only after cafa_native_view_valid; returns payload, not the TLV prefix. */
static int cafa_native_view_for_panel(const uint8_t *b,uint32_t n,int panel) {
    return cafa_native_view_for_caps(b,n,panel,0);
}
static int cafa_native_view_valid(const uint8_t *b,uint32_t n) { return cafa_native_view_for_caps(b,n,1,1); }
static const uint8_t *cafa_native_section(const uint8_t *b,uint32_t n,uint32_t tag,uint32_t *length) {
    uint32_t at=CAFA_NATIVE_HEADER;
    if(length)*length=0;
    if(!b || n<CAFA_NATIVE_HEADER || n>CAFA_NATIVE_MAX || tag<1u || tag>CAFA_NATIVE_CHROME)return 0;
    while(at+3u<=n) {
        uint32_t len=cafa_native_wire_u14(b+at+1),p=at+3u;
        if(p+len>n)return 0;
        if(b[at]==tag) {if(length)*length=len;return b+p;}
        at=p+len;
    }
    return 0;
}
#endif
