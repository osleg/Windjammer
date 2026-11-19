/* SPDX-License-Identifier: GPL-3.0-only
 * Private physical key-to-DAC diagnostic at the hosted controller boundary. */
#include <assert.h>
#include "../firmware/src/cafa_protocol.h"
#define UI_TEST_NO_MAIN 1
#define FELUCCA_CAFA 1
#define FELUCCA_CONTROLLER 1
#define FELUCCA_OTA 1
#define FELUCCA_DIAG 1
#define FELUCCA_UAC 1
#define FELUCCA_UAC_OUT 1
#define FELUCCA_CDC 0
static void cafa_diag_pcm(uint32_t,uint32_t);
static void cafa_diag_dac(uint32_t,uint32_t);
static void fm1_irq_off(void),fm1_irq_on(void);
static int cafa_diag_pcm_needed(void),cafa_diag_dac_needed(void);
static uint32_t cafa_diag_audio_t0;
static cafa_link_t cafa;
static volatile uint8_t cafa_audio_on;
static int cafa_frame(const uint8_t *,uint32_t);
static int cafa_input(void),cafa_draw(void),cafa_leds(void);
#include "ui_test.c"
static uint32_t ota_now_ms(void) {return fm1_ms;}
static void ota_idle(void) {fm1_ms++;}
static volatile uint8_t sx_diag_transport,sx_next_ready,sx_diag_ready;
#include "../firmware/src/cafa.c"

static void feed_return(void)
{
    uint32_t packet[UO_MAXF];
    unsigned at=0;
    while(at<UO_TGT) {
        unsigned n=UO_TGT-at<UO_MAXF?UO_TGT-at:UO_MAXF,i;
        for(i=0;i<n;i++) {
            unsigned frame=at+i;
            int16_t cue=frame>=44u && frame<76u ?
                (((frame-44u)/8u)&1u ? -24576 : 24576) : 0;
            packet[i]=1234u | (uint32_t)(uint16_t)cue<<16;
        }
        uo_packet(packet,n*4u);
        at+=n;
    }
}
static void feed_direct_return(void)
{
    uint32_t packet[16];
    unsigned at=0;
    while(at<UO_TGT) {
        unsigned i;
        for(i=0;i<16u;i++) {
            unsigned frame=at+i;
            int16_t cue=frame>=44u && frame<76u ?
                (((frame-44u)/8u)&1u ? -24576 : 24576) : 0;
            packet[i]=1234u | (uint32_t)(uint16_t)cue<<16;
        }
        assert(direct_packet(packet,sizeof packet)==1);
        at+=16u;
    }
}

static unsigned drain(uint8_t *out)
{
    unsigned at=0;
    while(so_r!=so_w) {
        uint32_t q=sx_out_q[so_r++%SXQ],cin=q&15u,n=cin==4u||cin==7u?3u:cin-4u,j;
        for(j=0;j<n;j++)out[at++]=(uint8_t)(q>>(8u+8u*j));
    }
    return at;
}
static unsigned hello(uint8_t *f,const char *name,unsigned length)
{
    unsigned count=13u+length;
    memset(f,0,56);f[0]=0x7d;f[1]=0x43;f[2]=0x41;f[3]=6;f[4]=WIRE6_HELLO;
    f[5]=7;f[9]=1;f[12]=(uint8_t)length;memcpy(f+13,name,length);
    f[count++]=3;
    f[count++]=1;f[count++]=1;f[count++]=0;f[count++]=1;
    f[count++]=WIRE6_NATIVE_UI;f[count++]=1;f[count++]=0;f[count++]=1;
    f[count++]=WIRE6_NATIVE_PANEL;f[count++]=1;f[count++]=0;f[count++]=1;
    return count;
}
int main(void)
{
    uint8_t f[56],out[64];wire6_link_t link={0};wire6_reply_t reply;
    unsigned n,i;
    uint32_t expected_us=(12000u+(HALF_FRAMES+45u)*DAC_TICKS)/FM1_TICKS_PER_US;
    int32_t audio[2u*HALF_FRAMES]={0};
    n=hello(f,"org.fm1.skiff.diag",18);
    reply=wire6_receive(&link,f,n,0);
    assert(reply.result==WIRE6_OK && !link.diagnostic);
    link.phase=2;link.audio_mode=1;f[4]=WIRE6_DIAG;f[9]=1;
    reply=wire6_receive(&link,f,10,1);
    assert(reply.result==WIRE6_UNSUPPORTED);
    n=hello(f,"org.fm1.skiff.diag2",19);memset(&link,0,sizeof link);
    reply=wire6_receive(&link,f,n,0);
    assert(reply.result==WIRE6_OK && !link.diagnostic);
    n=hello(f,"org.fm1.skiff.keytime",21);memset(&link,0,sizeof link);
    reply=wire6_receive(&link,f,n,0);
    assert(reply.result==WIRE6_OK && link.diagnostic);
    link.phase=2;link.audio_mode=1;f[4]=WIRE6_DIAG;f[9]=1;
    reply=wire6_receive(&link,f,10,1);
    assert(reply.result==WIRE6_OK);
    f[9]=4;reply=wire6_receive(&link,f,10,1);
    assert(reply.result==WIRE6_INVALID);
    usb.up=usb.config=1;usb.suspended=0;uo_stream(1);
    direct_session_publish(direct.epoch,1,1);
    wire6.phase=2;wire6.audio_mode=1;wire6.diagnostic=1;
    fm1_ms=100;cafa_diag_start();
    host_ticks=100000u;cafa_diag_key_downscan(host_ticks);
    cafa_diag_input(5,1);
    assert(cafa_diag.key_reports==1 && cafa_diag.key_stage==2);
    feed_return();
    assert(uo.frames==UO_TGT && uo.overruns==0u);
    assert(cafa_diag.key_stage==3 && cafa_diag.marker_index==44u);
    cafa_diag_audio_t0=112000u;
    uac_play(audio,HALF_FRAMES,4096u);
    for(i=0;i<HALF_FRAMES;i++) {
        assert(audio[2u*i]==1234*256);
        assert(audio[2u*i+1u]==0);
    }
    assert(cafa_diag.key_samples==1u && cafa_diag.key_last_us==expected_us &&
           cafa_diag.key_last_serial==5u && cafa_diag.detected_markers==1u);
    cafa_diag_reply(2,99);
    assert(drain(out)==49u && out[5]==WIRE6_DIAG && out[10]==2u && out[11]==3u);
    assert(wire6_u28(out+12)==0u && wire6_u28(out+16)==1u &&
           wire6_u28(out+20)==expected_us && wire6_u28(out+24)==5u &&
           wire6_u28(out+28)==0u && wire6_u28(out+32)==1u &&
           wire6_u28(out+36)==1u && wire6_u28(out+40)==1u &&
           wire6_u28(out+44)==0u);
    fm1_ms=200;cafa_diag_end();uo.overruns++;fm1_ms=300;
    cafa_diag_reply(3,100);
    assert(drain(out)==49u && wire6_u28(out+12)==100u &&
           wire6_u28(out+16)==1u && wire6_u28(out+44)==0u);
    fm1_ms=400;cafa_keys=0;cafa_diag_start();
    cafa_diag_key_downscan(100u);cafa_diag_input(9,1u);
    cafa_diag_key_downscan(200u);
    assert(cafa_diag.key_stage==0u && cafa_diag.key_unmatched==2u && cafa_diag.key_quiet);
    cafa_diag_input(10,1u);
    assert(cafa_diag.key_reports==2u && cafa_diag.key_stage==0u);
    cafa_diag_key_downscan(200u+2u*1000u*1000u*FM1_TICKS_PER_US-1u);
    assert(cafa_diag.key_stage==0u && cafa_diag.key_unmatched==3u);
    cafa_diag_key_downscan(cafa_diag.key_quiet_tick+2u*1000u*1000u*FM1_TICKS_PER_US);
    assert(cafa_diag.key_stage==1u);
    n=hello(f,"org.fm1.skiff.keytime2",22);memset(&link,0,sizeof link);
    reply=wire6_receive(&link,f,n,0);
    assert(reply.result==WIRE6_OK && link.diagnostic==2u);
    n=hello(f,"org.fm1.skiff.keytime3",22);memset(&link,0,sizeof link);
    reply=wire6_receive(&link,f,n,0);
    assert(reply.result==WIRE6_OK && !link.diagnostic);
    wire6.diagnostic=2u;fm1_ms=500;cafa_keys=0;
    direct_stream(1);cafa_diag_start();
    cafa_diag_key_downscan(99999u);
    assert(cafa_diag.key_downscans==0u); /* direct interface alone has no session owner */
    direct_session_publish(direct.epoch,2,1);cafa_diag_start();
    host_ticks=100000u;cafa_diag_key_downscan(host_ticks);
    host_ticks=101000u;cafa_diag_input(12,1u);
    host_ticks=105000u;feed_direct_return();
    assert(cafa_diag.key_stage==3u);
    cafa_diag_audio_t0=112000u;uac_play(audio,HALF_FRAMES,4096u);
    assert(cafa_diag.key_samples==1u && cafa_diag.key_last_serial==12u);
    cafa_diag_reply(2,101);
    assert(drain(out)==57u && out[10]==2u && out[11]==4u);
    assert(wire6_u28(out+16)==1u && wire6_u28(out+24)==12u);
    assert(wire6_u28(out+48)==1000u/FM1_TICKS_PER_US);
    assert(wire6_u28(out+52)==5000u/FM1_TICKS_PER_US);
    direct_stream(0);direct_stream(1);direct_session_publish(direct.epoch,2,1);
    cafa_keys=0;fm1_ms=600;cafa_diag_start();
    host_ticks=200000u;cafa_diag_key_downscan(host_ticks);
    host_ticks=205000u;cafa_diag_input(13,1u);
    host_ticks=203000u;feed_direct_return(); /* impossible event order: unavailable stages */
    cafa_diag_audio_t0=212000u;uac_play(audio,HALF_FRAMES,4096u);
    assert(cafa_diag.key_samples==1u);
    cafa_diag_reply(2,102);
    assert(drain(out)==57u && wire6_u28(out+48)==0u && wire6_u28(out+52)==0u);
    puts("Keytime: namespace, matched DAC, frozen reply and overlapping-press rejection passed");
    return 0;
}
