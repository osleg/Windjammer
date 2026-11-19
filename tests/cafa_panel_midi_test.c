/* SPDX-License-Identifier: GPL-3.0-only
 * Controller TRS MIDI -> phone USB packet bridge at the real wire-6 owner. */
#include <assert.h>
#include "../firmware/src/cafa_protocol.h"
#define UI_TEST_NO_MAIN 1
#define FELUCCA_CAFA 1
#define FELUCCA_CONTROLLER 1
#define FELUCCA_UART 1
#define FELUCCA_OTA 1
static cafa_link_t cafa;
static volatile uint8_t cafa_audio_on;
static int cafa_frame(const uint8_t *,uint32_t);
static int cafa_input(void),cafa_draw(void),cafa_leds(void);
#include "ui_test.c"
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { fm1_ms++; }
#include "../firmware/src/cafa.c"

static void header(uint8_t *f,unsigned cmd,unsigned serial)
{
    memset(f,0,165);f[0]=0x7d;f[1]=0x43;f[2]=0x41;f[3]=6;f[4]=(uint8_t)cmd;
    cafa_put14(f+5,73);cafa_put14(f+7,serial);
}
static void view(uint8_t *v)
{
    unsigned at;
    memset(v,0,256);v[0]=CAFA_NATIVE_KIND;wire6_put28(v+2,1);
    v[6]=47;v[8]=6;cafa_put14(v+11,1);cafa_put14(v+15,1);
    cafa_put14(v+24,16383);memset(v+33,' ',79);
    memcpy(v+33,"HOME",4);
    for(at=112;at<256;at+=36)memset(v+at,' ',30);
    v[256]=CAFA_NATIVE_PREFS;v[257]=2;v[258]=v[259]=0;v[260]=8;
    assert(cafa_native_view_valid(v,261));
}
static void part(uint8_t *f,const uint8_t *v,unsigned offset,unsigned count)
{
    header(f,WIRE6_VIEW_PART,1);cafa_put14(f+9,261);cafa_put14(f+11,offset);
    memcpy(f+13,v+offset,count);assert(cafa_frame(f,13u+count));
}
int main(void)
{
    uint8_t f[165],v[261],raw[MQO*4];
    uint32_t n;
    usb.config=1;
    um_byte(0x90);um_byte(60);um_byte(100);
    assert(!mo_w && !mi_w);              /* idle controller cannot forward */
    header(f,WIRE6_HELLO,0);
    f[9]=1;f[10]=0;f[11]=0;f[12]=3;memcpy(f+13,"APP",3);
    f[16]=4;f[17]=1;f[18]=1;f[19]=0;f[20]=1;
    f[21]=9;f[22]=1;f[23]=0;f[24]=1;
    f[25]=WIRE6_PANEL_MIDI;f[26]=1;f[27]=0;f[28]=1;
    f[29]=WIRE6_NATIVE_PANEL;f[30]=1;f[31]=0;f[32]=1;
    assert(cafa_frame(f,33) && wire6.phase==1 && !cafa_panel_midi_active);
    so_r=so_w;
    um_byte(0x90);um_byte(61);um_byte(100);
    assert(!mo_w && !mi_w);              /* HELLO alone cannot forward */
    view(v);
    part(f,v,0,152);part(f,v,152,sizeof v-152);
    assert(wire6.phase==2 && cafa_panel_midi_active && so_w!=so_r);
    assert(cafa_host_master);
    so_r=so_w;song.master_q12=1234;
    assert(cafa_input());
    /* Reassemble the 26-byte initial scan through the USB MIDI transmitter. */
    unsigned pos=0;
    while(so_r!=so_w) {
        uint32_t word=sx_out_q[so_r++%SXQ],cin=word&15u;
        for(unsigned i=0;i<(cin==4?3u:cin-4u);i++)f[pos++]=(uint8_t)(word>>(8u+8u*i));
    }
    assert(pos==37 && f[5]==WIRE6_INPUT && wire6_u14(f+10+24)==1234);
    fm1_ms+=34;song.master_q12=0;
    assert(cafa_input() && wire6_last_master==0 && so_r!=so_w);
    um_byte(0x90);um_byte(62);um_byte(101);
    um_byte(0xF8);um_byte(0xF2);um_byte(4);um_byte(0);
    assert(!mi_w && mo_w==3 && sx_out_q[so_r%SXQ]);
    n=mo_take(raw,sizeof raw,mo.now);
    assert(n==12 && raw[0]==9 && raw[1]==0x90 && raw[2]==62 && raw[3]==101 &&
           raw[4]==15 && raw[5]==0xF8 && raw[8]==3 && raw[9]==0xF2);
    so_r=so_w;
    mo_w=mo_r+MQO;so_w=so_r+SXQ;
    um_byte(0x90);um_byte(63);um_byte(100);
    assert(cafa_panel_midi_overflow && mo_w==mo_r+MQO);
    assert(cafa_input() && !wire6.phase && !cafa_panel_midi_active && !cafa_host_master && mo_r==mo_w);
    assert(so_w!=so_r);                  /* BYE, before the host lease times out */
    um_byte(0x90);um_byte(64);um_byte(100);
    assert(mo_r==mo_w);                  /* no stale note after disconnect */
    puts("Controller panel MIDI: cap gate, TRS packets, SysEx queue, overflow BYE, disconnect passed");
    return 0;
}
