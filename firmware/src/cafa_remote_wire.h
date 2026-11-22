/* SPDX-License-Identifier: GPL-3.0-only
 * REMOTE_UI payload validation, independent of the LCD and USB drivers. */
#ifndef CAFA_REMOTE_WIRE_H
#define CAFA_REMOTE_WIRE_H
#include <stdint.h>

enum { REMOTE_RESET=0, REMOTE_TILE=1, REMOTE_BLIT=2, REMOTE_PRESENT=3, REMOTE_RECOVER=4,
       REMOTE_PACKED_TILE=5, REMOTE_BLITS=6 };
#define REMOTE_TILES 128u
#define REMOTE_GRID 15u
#define REMOTE_PIXELS 256u
#define REMOTE_LED_BYTES 29u
#define REMOTE_MAX_BLITS 51u

static uint32_t remote_u14(const uint8_t *p) {return p[0]|(uint32_t)p[1]<<7;}
static uint32_t remote_u28(const uint8_t *p) {
    return p[0]|(uint32_t)p[1]<<7|(uint32_t)p[2]<<14|(uint32_t)p[3]<<21;
}
static uint32_t remote_record_count(uint8_t tag) {return (tag&63u)+1u;}
static uint16_t remote_lcd_pixel(const uint8_t *p)
{
    uint16_t rgb=(uint16_t)(p[0]|(uint16_t)p[1]<<7|(uint16_t)p[2]<<14);
    return (uint16_t)((rgb>>8)|(rgb<<8));
}
static int remote_blit_valid(const uint8_t *p)
{
    return p[0]<REMOTE_TILES && p[1]<REMOTE_GRID && p[2]<REMOTE_GRID;
}
/* Validate the complete fragment before any staging or cache state changes. */
static uint32_t remote_packed_count(const uint8_t *p,uint32_t n)
{
    uint32_t i=4u,count=0,off=remote_u14(p+2);
    while(i<n) {
        uint8_t tag=p[i++];
        uint32_t pixels=remote_record_count(tag),colors=tag<64u ? pixels:1u;
        count+=pixels;
        if(off+count>REMOTE_PIXELS || n-i<3u*colors)return 0;
        for(;colors;colors--,i+=3u)if(p[i+2u]>3u)return 0;
    }
    return count;
}
static int remote_payload_valid(const uint8_t *p,uint32_t n,uint32_t *pixels)
{
    uint32_t i,count;
    if(!n)return 0;
    for(i=0;i<n;i++)if(p[i]&128u)return 0;
    switch(p[0]) {
    case REMOTE_RESET: case REMOTE_RECOVER:return n==1u;
    case REMOTE_TILE:
        if(n<7u || p[1]>=REMOTE_TILES)return 0;
        count=(n-4u)/3u;
        if((n-4u)%3u || !count || count>50u ||
           remote_u14(p+2)>=REMOTE_PIXELS || remote_u14(p+2)+count>REMOTE_PIXELS)return 0;
        for(i=0;i<count;i++)if(p[6u+3u*i]>3u)return 0;
        *pixels=count;
        return 1;
    case REMOTE_PACKED_TILE:
        if(n<8u || p[1]>=REMOTE_TILES)return 0;
        *pixels=remote_packed_count(p,n);
        return *pixels!=0;
    case REMOTE_BLIT:return n==4u && remote_blit_valid(p+1);
    case REMOTE_BLITS:
        if(n<4u || (n-1u)%3u || (n-1u)/3u>REMOTE_MAX_BLITS)return 0;
        for(i=1u;i<n;i+=3u)if(!remote_blit_valid(p+i))return 0;
        return 1;
    case REMOTE_PRESENT:
        if(n!=5u+REMOTE_LED_BYTES || p[33]>1u)return 0;
        for(i=0;i<4u;i++)if(p[7u+7u*i]>1u || p[11u+7u*i]>63u)return 0;
        return 1;
    default:return 0;
    }
}
#endif
