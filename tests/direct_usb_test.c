/* SPDX-License-Identifier: GPL-3.0-only */
/* Controller USB contract: descriptor, source isolation, lossless bulk admission
 * and playback at the actual 10 kHz service opportunity cadence. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define HALF_FRAMES 128
#define FELUCCA_CONTROLLER 1
#define FELUCCA_CAFA 1
#define FELUCCA_UAC 1
#define FELUCCA_UAC_OUT 1
#define FELUCCA_CDC 0
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/src/usb.c"

static uint32_t pcm(int16_t l, int16_t r)
{ return (uint16_t)l | (uint32_t)(uint16_t)r << 16; }
static void descriptor(void)
{
    const uint8_t *d; uint16_t len; unsigned off, alt0=0, alt1=0, ep=0;
    assert(get_desc(0x0100, &d, &len) && len==18 && d[12]==0xE1 && d[13]==3);
    assert(get_desc(0x0200, &d, &len) && d[4]==5 && (d[2] | d[3]<<8)==len);
    for(off=0;off<len;off+=d[off]) {
        assert(d[off]>=2 && off+d[off]<=len);
        if(d[off+1]==4 && d[off+2]==DIRECT_IF) {
            assert(d[off+5]==0xff && d[off+6]==0x46 && d[off+7]==1);
            if(d[off+3]==0) { assert(d[off+4]==0); alt0++; }
            if(d[off+3]==1) { assert(d[off+4]==1); alt1++; }
        }
        if(d[off+1]==5 && d[off+2]==3) {
            assert(d[off+3]==2 && d[off+4]==64 && d[off+5]==0 && d[off+6]==0);
            ep++;
        }
    }
    assert(off==len && alt0==1 && alt1==1 && ep==1);
}
static void reset(void)
{
    memset(&uo,0,sizeof uo); memset(&direct,0,sizeof direct);
    uo_w=uo_r=0; usb.up=usb.config=1; usb.suspended=0; uo_stream(1);
    direct_session_publish(direct.epoch,1,0);
}
static void source_isolation(void)
{
    uint32_t managed[44], raw[16], i, before;
    int32_t out[2*HALF_FRAMES]={0};
    reset();
    for(i=0;i<44;i++) managed[i]=pcm(3000,3000);
    for(i=0;i<16;i++) raw[i]=pcm(1000,-1000);
    uo_packet(managed,sizeof managed);
    direct_stream(1);
    assert(!cafa_return_active() && !direct.owner && direct.alt==1);
    before=uo_w;
    uo_packet(managed,sizeof managed);
    assert(uo_w==before);                 /* EP4 is discarded before ring/key path */
    for(i=0;i<16;i++) assert(direct_packet(raw,64)==1);
    assert(!cafa_return_active());       /* pre-session PCM is admitted but silent */
    direct_session_publish(direct.epoch,2,0);
    assert(cafa_return_active());
    uac_play(out,HALF_FRAMES,4096);
    assert(out[0]==1000*256 && out[1]==-1000*256);
    for(i=0;i<HALF_FRAMES;i++) assert(out[2*i]==1000*256);
    uo_stream(0); uo_stream(1);         /* host UAC toggles cannot reset direct */
    assert(direct.alt && uo_w>uo_r);
    usb.suspended=1; assert(!cafa_return_active());
    usb.suspended=0; assert(cafa_return_active());
    before=direct.epoch;
    direct_stream(0);
    assert(!cafa_return_active() && !direct.owner);
    direct_session_publish(before,2,2); /* a late FIRST_STATE ACK cannot reopen it */
    assert(!cafa_return_active() && !cafa_keytime_return);
    before=uo_w;
    uo_packet(managed,sizeof managed);
    assert(uo_w==before);               /* old managed session does not resume */
    direct_session_publish(direct.epoch,1,0); /* fresh managed ownership */
    uo_stream(1);
    uo_packet(managed,sizeof managed);
    assert(uo_w==before+44);
    direct_session_publish(direct.epoch,0,0); /* PHONE session never selects USB audio */
    assert(!cafa_return_active());
    direct_stream(1); direct_packet(raw,64);
    direct_stream(0);
    assert(uo_w-uo.reset_w==0);          /* no raw tail after close */
}
static void backpressure(void)
{
    uint32_t raw[16], i, before;
    int32_t out[2*HALF_FRAMES]={0};
    reset(); direct_stream(1);
    for(i=0;i<16;i++) raw[i]=pcm(1000,-1000);
    for(i=0;i<DIRECT_MAXF/16;i++) assert(direct_packet(raw,64)==1);
    before=uo_w;
    assert(direct_packet(raw,64)==0 && uo_w==before);
    uac_play(out,HALF_FRAMES,4096);
    assert(out[0]==1000*256 && uo_w-uo_r<DIRECT_MAXF);
    assert(direct_packet(raw,64)==1 && uo_w==before+16);
    direct_stream(0); uac_play(out,HALF_FRAMES,4096);
    assert(!uo.go);
}
static void sustained(void)
{
    uint32_t raw[16], managed[44], i, tick, acc=0, remain=0, admitted=0, rendered=0;
    int32_t out[2*HALF_FRAMES];
    reset(); direct_stream(1);
    for(i=0;i<16;i++) raw[i]=pcm(1234,-2345);
    for(i=0;i<44;i++) managed[i]=pcm(3000,3000);
    /* First 128 frames may arrive before the session and pause until sync. */
    for(i=0;i<8;i++) assert(direct_packet(raw,64)==1);
    assert(!direct.owner && uo_w-uo.reset_w==128);
    direct_session_publish(direct.epoch,2,0);
    for(tick=0;tick<10000;tick++) {
        if(tick%10==0) { acc+=441; remain+=acc/10; acc%=10; uo_packet(managed,44*4); }
        if(remain) {
            uint32_t n=remain>16 ? 16:remain;
            if(direct_packet(raw,n*4)==1) { remain-=n; admitted+=n; }
        }
        if(tick%29==0) {
            memset(out,0,sizeof out); uac_play(out,HALF_FRAMES,4096);
            if(uo.go) { assert(out[0]==1234*256 && out[1]==-2345*256); rendered++; }
        }
        assert(uo_w-uo_r<=DIRECT_MAXF && remain<=64);
    }
    assert(admitted>44000 && rendered>300 && !uo.overruns && !uo.underruns);
}
int main(void)
{
    descriptor(); source_isolation(); backpressure(); sustained();
    puts("Direct USB controller descriptor, isolation, backpressure and 10 kHz cadence passed");
    return 0;
}
