/* SPDX-License-Identifier: GPL-3.0-only
 * Open wire-6 at the real firmware mailbox, physical-input and LCD boundary. */
#include <assert.h>
#include "../firmware/src/cafa_protocol.h"
#define UI_TEST_NO_MAIN 1
#define FELUCCA_CAFA 1
#define FELUCCA_OTA 1
static cafa_link_t cafa;
static volatile uint8_t cafa_audio_on;
static int cafa_frame(const uint8_t *,uint32_t);
static int cafa_input(void),cafa_draw(void),cafa_leds(void);
#include "ui_test.c"
static uint32_t ota_now_ms(void) {return fm1_ms;}
static void ota_idle(void) {fm1_ms++;}
#include "../firmware/src/cafa.c"
static unsigned drain(uint8_t *f)
{
    unsigned n=0;
    while(so_r!=so_w) {
        uint32_t w=sx_out_q[so_r++%SXQ];unsigned cin=w&15u,k=cin==4u?3u:cin-4u,j;
        for(j=0;j<k;j++)f[n++]=(uint8_t)(w>>(8+8*j));
    }
    return n;
}
static void header(uint8_t *f,unsigned cmd,unsigned sid,unsigned rev)
{
    memset(f,0,147);f[0]=0x7d;f[1]=0x43;f[2]=0x41;f[3]=6;f[4]=(uint8_t)cmd;
    cafa_put14(f+5,sid);cafa_put14(f+7,rev);
}
static unsigned hello(uint8_t *f,unsigned sid,unsigned mode)
{
    header(f,WIRE6_HELLO,sid,0);f[9]=1;f[10]=0;f[11]=(uint8_t)mode;
    f[12]=3;memcpy(f+13,"APP",3);f[16]=1;
    f[17]=1;f[18]=1;f[19]=0;f[20]=1;
    return 21;
}
static unsigned hello_parts(uint8_t *f,unsigned sid,unsigned mode)
{
    unsigned n=hello(f,sid,mode);
    f[16]=2;f[n++]=3;f[n++]=1;f[n++]=0;f[n++]=0;
    return n;
}
static unsigned hello_native(uint8_t *f,unsigned sid)
{
    unsigned n=hello(f,sid,0);
    f[16]=3;f[n++]=9;f[n++]=1;f[n++]=0;f[n++]=1;
    f[n++]=11;f[n++]=1;f[n++]=0;f[n++]=0;
    return n;
}
static unsigned hello_delta(uint8_t *f,unsigned sid)
{
    unsigned n=hello_native(f,sid);
    f[16]=4;f[n++]=WIRE6_NATIVE_DELTA;f[n++]=1;f[n++]=0;f[n++]=1;
    return n;
}
static void native_view(uint8_t *v,unsigned context)
{
    unsigned at;
    memset(v,0,256);v[0]=CAFA_NATIVE_KIND;wire6_put28(v+2,context);
    v[6]=47;v[8]=6;cafa_put14(v+11,1);cafa_put14(v+15,1);
    cafa_put14(v+24,16383);memset(v+33,' ',79);
    memcpy(v+33,"HOME",4);
    for(at=112;at<256;at+=36)memset(v+at,' ',30);
    assert(cafa_native_view_valid(v,256));
}
static unsigned native_part(uint8_t *f,unsigned sid,unsigned rev,
                            const uint8_t *v,unsigned offset,unsigned count)
{
    header(f,WIRE6_VIEW_PART,sid,rev);cafa_put14(f+9,256);
    cafa_put14(f+11,offset);memcpy(f+13,v+offset,count);
    return 13u+count;
}
static void native_view(uint8_t *v,unsigned context);
static unsigned delta_one(uint8_t *f,unsigned sid,unsigned rev,unsigned base,
                          unsigned offset,const uint8_t *data,unsigned count)
{
    header(f,WIRE6_VIEW_DELTA,sid,rev);cafa_put14(f+9,base);
    cafa_put14(f+11,offset);f[13]=(uint8_t)count;
    memcpy(f+14,data,count);
    return 14u+count;
}
static void delta_contract(void)
{
    wire6_link_t link={0};wire6_reply_t r;
    uint8_t a[CAFA_NATIVE_MAX],b[CAFA_NATIVE_MAX],view[256],saved[256],f[165],oversize[166];
    unsigned n,last,rev;
    link.view=a;link.stage=b;
    n=hello(f,90,0);f[16]=2;f[n++]=WIRE6_NATIVE_DELTA;
    f[n++]=1;f[n++]=0;f[n++]=0;
    r=wire6_receive(&link,f,n,10);
    assert(r.result==WIRE6_OK && link.phase==1 && !wire6_has(&link,WIRE6_NATIVE_DELTA));
    memset(&link,0,sizeof link);link.view=a;link.stage=b;
    f[n-1]=1;r=wire6_receive(&link,f,n,10);
    assert(r.result==WIRE6_UNSUPPORTED && !link.phase);
    n=hello_delta(f,90);r=wire6_receive(&link,f,n,10);
    assert(r.result==WIRE6_OK && link.phase==1 && wire6_has(&link,WIRE6_NATIVE_DELTA) &&
           wire6_has(&link,WIRE6_NATIVE_UI) && wire6_has(&link,WIRE6_NATIVE_KEEPALIVE));
    native_view(view,17);
    n=native_part(f,90,16383,view,0,152);
    r=wire6_receive(&link,f,n,20);assert(!r.action && link.stage_len==152);
    n=native_part(f,90,16383,view,152,104);
    r=wire6_receive(&link,f,n,21);
    assert(r.action==WIRE6_FIRST_STATE && r.result==WIRE6_OK && link.phase==2 && link.revision==16383);
    memcpy(saved,link.view,256);last=link.last_ms;
    n=delta_one(f,90,0,16382,33,(const uint8_t *)"NEXT",4);
    r=wire6_receive(&link,f,n,100);
    assert(r.action==WIRE6_SEND_ACK && r.command==WIRE6_VIEW_DELTA && r.result==WIRE6_INVALID &&
           link.revision==16383 && link.last_ms==last && !memcmp(saved,link.view,256));
    cafa_put14(f+9,16383);f[13]=0;
    r=wire6_receive(&link,f,n,101);assert(r.action==WIRE6_SEND_ACK && r.result==WIRE6_INVALID);
    f[13]=4;f[14]=128;
    r=wire6_receive(&link,f,n,102);assert(r.action==WIRE6_SEND_ACK && r.result==WIRE6_INVALID);
    f[14]='N';cafa_put14(f+11,255);
    r=wire6_receive(&link,f,n,103);assert(r.action==WIRE6_SEND_ACK && r.result==WIRE6_INVALID);
    memset(oversize,0,sizeof oversize);memcpy(oversize,f,9);
    r=wire6_receive(&link,oversize,sizeof oversize,103);
    assert(r.action==WIRE6_SEND_ACK && r.result==WIRE6_INVALID);
    cafa_put14(f+11,33);f[14]=0;
    r=wire6_receive(&link,f,n,104);assert(r.action==WIRE6_SEND_ACK && r.result==WIRE6_INVALID);
    n=delta_one(f,90,0,16383,6,(const uint8_t *)".",1);
    r=wire6_receive(&link,f,n,105);assert(r.action==WIRE6_SEND_ACK && r.result==WIRE6_INVALID);
    n=delta_one(f,90,0,16383,33,(const uint8_t *)"NEXT",4);
    cafa_put14(f+n,32);f[n+2]=1;f[n+3]='X';n+=4;
    r=wire6_receive(&link,f,n,106);assert(r.action==WIRE6_SEND_ACK && r.result==WIRE6_INVALID);
    assert(link.last_ms==last && link.revision==16383 && !memcmp(saved,link.view,256));
    n=delta_one(f,91,0,16383,33,(const uint8_t *)"NEXT",4);
    r=wire6_receive(&link,f,n,107);assert(!r.action && link.last_ms==last);
    n=delta_one(f,90,0,16383,33,(const uint8_t *)"NEXT",4);
    r=wire6_receive(&link,f,n,108);
    assert(r.action==WIRE6_NEW_STATE && r.result==WIRE6_OK && r.renew && link.revision==0 &&
           link.last_ms==108 && !memcmp(link.view+33,"NEXT",4));
    rev=link.revision;n=delta_one(f,90,0,0,33,(const uint8_t *)"MORE",4);
    r=wire6_receive(&link,f,n,109);
    assert(r.action==WIRE6_SEND_ACK && r.result==WIRE6_INVALID && link.revision==rev && link.last_ms==108);
    native_view(view,18);n=native_part(f,90,1,view,0,152);
    r=wire6_receive(&link,f,n,110);assert(!r.action && link.stage_len==152);
    n=delta_one(f,90,2,0,33,(const uint8_t *)"MORE",4);
    r=wire6_receive(&link,f,n,111);
    assert(r.action==WIRE6_SEND_ACK && r.result==WIRE6_BUSY && link.revision==0 && link.last_ms==110);
    n=native_part(f,90,1,view,152,104);
    r=wire6_receive(&link,f,n,112);
    assert(r.action==WIRE6_NEW_STATE && r.result==WIRE6_OK && link.revision==1 &&
           wire6_u28(link.view+2)==18 && !link.stage_len);
    n=delta_one(f,90,2,0,33,(const uint8_t *)"MORE",4);
    r=wire6_receive(&link,f,n,113);
    assert(r.action==WIRE6_SEND_ACK && r.result==WIRE6_INVALID && link.revision==1 &&
           link.last_ms==112 && wire6_u28(link.view+2)==18);
    puts("Open wire-6 delta: negotiated dependencies, wrap, atomic validation, wrong base/session, busy and lease isolation passed");
}
static unsigned state(uint8_t *f,unsigned sid,unsigned rev,unsigned context)
{
    unsigned k;header(f,WIRE6_STATE,sid,rev);
    memset(f+9,' ',138);f[9]=1;f[10]=0;wire6_put28(f+11,context);
    memcpy(f+15,"OPEN INSTRUMENT ",16);memcpy(f+31,"SOUND BANK 1",12);
    cafa_put14(f+43,0);cafa_put14(f+45,1);cafa_put14(f+47,0);cafa_put14(f+49,1);
    for(k=0;k<4;k++) {
        uint8_t *q=f+51+24*k;
        memset(q,' ',20);q[20]=q[21]=q[22]=q[23]=0;
    }
    memcpy(f+51,"CUTOFF  ",8);memcpy(f+59,"1200 Hz     ",12);
    cafa_put14(f+71,8192);f[73]=1;f[74]=1;
    return 147;
}
static void overview_text_at(unsigned y,const aafont_t *font,const char *expected,uint16_t color)
{
    static uint16_t actual[25u*240u];
    unsigned row;
    for(row=0;row<25u;row++)memcpy(actual+row*240u,host_screen+(y+row)*240u,240u*sizeof(uint16_t));
    lcd_fill(0,0,240,240,T_BG);
    draw_text_box(8,y,224,font,expected,color,0);
    for(row=0;row<25u;row++)assert(!memcmp(actual+row*240u,host_screen+(y+row)*240u,240u*sizeof(uint16_t)));
}
static void overview(uint8_t *f,unsigned sid,unsigned rev,uint8_t flags,unsigned tempo)
{
    unsigned n=hello(f,sid,0);
    f[16]=2;f[21]=2;f[22]=1;f[23]=f[24]=0;
    assert(cafa_frame(f,n+4));
    {uint8_t out[128];assert(drain(out)==22 && wire6.accepted_count==2);}
    header(f,WIRE6_STATE,sid,rev);memset(f+9,' ',42);
    f[9]=2;f[10]=0;wire6_put28(f+11,rev);
    memcpy(f+15,"TRANSPORT       ",16);memcpy(f+31,"REAL OVERVIEW",12);
    f[43]=flags;cafa_put14(f+44,tempo);memset(f+46,0,5);
    assert(cafa_frame(f,51));
    {uint8_t out[128];assert(drain(out)==13 && wire6.phase==2);}
    assert(cafa_draw());
}
static unsigned mixer(uint8_t *f,unsigned sid,unsigned rev)
{
    unsigned k;header(f,WIRE6_STATE,sid,rev);
    memset(f+9,0,138);f[9]=4;wire6_put28(f+11,rev);
    memset(f+15,' ',28);memcpy(f+15,"BASS",4);memcpy(f+31,"9 / 10",6);
    cafa_put14(f+43,8);cafa_put14(f+45,10);cafa_put14(f+47,8);
    f[49]=2;f[50]=0;
    for(k=0;k<8;k++)memset(f+51+8*k,' ',5);
    memcpy(f+51,"BASS ",5);cafa_put14(f+56,10000);f[58]=60;
    memcpy(f+59,"LEAD ",5);cafa_put14(f+64,4000);f[66]=44;
    memset(f+115,' ',24);memcpy(f+115,"1.2 x",5);memcpy(f+127,"50%",3);
    cafa_put14(f+139,8192);f[141]=15;
    return 147;
}
/* A stock step LED can change during playback with no change to the display. */
static void panel_led_contract(void)
{
    uint8_t before[288],after[288],expected[FM1_NCOL]={0};
    native_view(before,71);
    memset(before+256,0,32);before[256]=CAFA_NATIVE_LEDS;before[257]=29;
    memcpy(after,before,sizeof before);
    after[259+2]=1;                   /* full green PLAY */
    after[259+7+3]=1;                /* dim first physical piano key */
    after[259+14+1]=1;               /* breathing button 7 */
    after[287]=1;
    assert(cafa_native_view_for_panel(after,sizeof after,1));
    assert(!cafa_native_view_for_panel(after,sizeof after,0));
    assert(!cafa_native_profile_regions(before,sizeof before,after,sizeof after));
    after[259+6]=64;assert(!cafa_native_view_valid(after,sizeof after));after[259+6]=0;
    wire6.view=after;wire6.view_len=sizeof after;wire6.phase=2;wire6_led_dirty=1;
    assert(wire6_leds() && host_dim_lo);
    assert(fm1_led[LED_PLAY_GREEN>>3]&(1u<<(LED_PLAY_GREEN&7u)));
    led_put(expected,LED_KEY0,1);assert(!memcmp(expected,(const void *)fm1_led_dim,FM1_NCOL));
    memset(expected,0,sizeof expected);led_put(expected,panel.btn[7],1);
    assert(!memcmp(expected,(const void *)fm1_led_breath,FM1_NCOL));
    wire6_leave();
}
int main(void)
{
    uint8_t f[147],out[128],saved[138];unsigned n,last;
    ui_power_on();usb.config=1;fm1_ms=0;
    trk_note_on(&trk[0],60,100);song.playing=1;
    header(f,WIRE6_PROBE,70,0);assert(cafa_frame(f,9));
    assert(drain(out)==17+4*sizeof wire6_capabilities && out[5]==WIRE6_OFFER && out[10]==1 && out[11]>=2 && out[16]==1 &&
           out[20]==2 && out[24]==3 && out[28]==9 && out[32]==11 && out[36]==12 && out[40]==13);
    header(f,WIRE6_PROBE,70,1);assert(cafa_frame(f,9));
    assert(drain(out)==21 && out[5]==WIRE6_OFFER && wire6_u14(out+8)==1u && out[15]==1u && out[16]==WIRE6_NATIVE_CHROME);
    assert(!wire6.phase && song.playing && voices_busy());
    /* Idle native keepalive is available only with the native display. */
    n=hello(f,71,0);f[16]=2;f[n]=11;f[n+1]=1;f[n+2]=0;f[n+3]=1;
    assert(cafa_frame(f,n+4));assert(drain(out)==13 && out[11]==WIRE6_UNSUPPORTED && !wire6.phase);
    f[n+3]=0;assert(cafa_frame(f,n+4));
    assert(drain(out)==18 && wire6.phase==1 && !wire6_has(&wire6,11));
    header(f,WIRE6_BYE,71,0);assert(cafa_frame(f,9));drain(out);
    n=hello(f,71,0);f[20]=0;assert(cafa_frame(f,n));
    assert(drain(out)==13 && out[11]==WIRE6_UNSUPPORTED && !wire6.phase && song.playing);
    f[20]=1;assert(cafa_frame(f,n));
    assert(drain(out)==18 && out[11]==WIRE6_OK && wire6.phase==1 && song.playing);
    fm1_ms=200;assert(cafa_frame(f,n));drain(out);assert(wire6.last_ms==0);
    header(f,CAFA_HELLO,71,0);f[3]=5;assert(cafa_frame(f,9) && !cafa.phase && wire6.phase==1);
    n=state(f,71,1,0x1234567u);assert(cafa_frame(f,n));
    assert(drain(out)==13 && wire6.phase==2 && cafa_audio_on && !cafa_usb_return_on && !song.playing);
    assert(wire6.drawn_context==0 && wire6.revision==1);
    fm1_in.buttons=fm1_in.notes=0;assert(cafa_input());
    assert(drain(out)==35 && out[5]==WIRE6_INPUT && wire6_u28(out+30)==0);
    assert(cafa_draw() && wire6.drawn_context==0x1234567u);
    host_enc[panel.enc[0]]=panel.dir[0];assert(cafa_input());
    assert(drain(out)==35 && wire6_u28(out+30)==0x1234567u && wire6_u14(out+16)==8193);
    memcpy(saved,wire6.view,138);last=wire6.last_ms;
    assert(cafa_frame(f,n));drain(out);
    assert(wire6.last_ms==last && !memcmp(saved,wire6.view,138));
    cafa_put14(f+7,2);f[51]=1;assert(cafa_frame(f,n));drain(out);
    assert(wire6.last_ms==last && !memcmp(saved,wire6.view,138));
    f[51]='C';f[9+1]=4;assert(cafa_frame(f,n));drain(out);
    assert(wire6.last_ms==last && !memcmp(saved,wire6.view,138));
    n=state(f,71,2,0x2345678u);assert(cafa_frame(f,n));drain(out);
    fm1_in.notes=(1u<<27)|1u;assert(cafa_input());
    assert(drain(out)==35 && wire6_u28(out+30)==0x1234567u && out[12]==1 && !(out[15]&8u));
    assert(cafa_draw() && wire6.drawn_context==0x2345678u);
    fm1_in.notes=0;assert(cafa_input());
    assert(drain(out)==35 && wire6_u28(out+30)==0x2345678u && out[12]==0);
    fm1_ms=1700;assert(cafa_input() && !wire6.phase && !cafa_audio_on && cafa_release_guard);
    fm1_in.buttons=fm1_in.notes=0;assert(cafa_input() && !cafa_release_guard);
    n=hello(f,72,0);assert(cafa_frame(f,n));drain(out);assert(wire6.phase==1);
    header(f,WIRE6_BYE,72,0);assert(cafa_frame(f,9) && !wire6.phase);
    header(f,CAFA_HELLO,73,0);f[3]=5;assert(cafa_frame(f,9) && cafa.phase==1);drain(out);
    n=hello(f,74,0);assert(cafa_frame(f,n));
    assert(drain(out)==13 && out[11]==WIRE6_BUSY && !wire6.phase && cafa.phase==1);
    f[20]=2;assert(cafa_frame(f,n));
    assert(drain(out)==13 && out[11]==WIRE6_INVALID && cafa.phase==1);
    header(f,CAFA_BYE,73,0);f[3]=5;assert(cafa_frame(f,9) && !cafa.phase);
    n=hello(f,75,0);assert(cafa_frame(f,n));drain(out);
    n=state(f,75,1,9);assert(cafa_frame(f,n));drain(out);
    so_w=so_r+SXQ;assert(cafa_input());
    assert(!wire6.phase && !cafa_audio_on && cafa_release_guard);
    so_r=so_w;
    overview(f,76,1,3,1234);
    overview_text_at(105,&AF_M,"RECORDING",T_THEME);
    /* A fresh view is needed after the screen comparison clears the host pixels. */
    f[43]=3;cafa_put14(f+44,1234);wire6_dirty=1;wire6_drawn_len=0;
    assert(cafa_draw());
    overview_text_at(141,&AF_S,"123.4 BPM",T_TEXT);
    header(f,WIRE6_BYE,76,0);assert(cafa_frame(f,9));drain(out);
    n=hello(f,77,0);assert(cafa_frame(f,n));drain(out);
    n=mixer(f,77,1);assert(cafa_frame(f,n));
    assert(drain(out)==13 && out[11]==WIRE6_INVALID && wire6.phase==1);
    header(f,WIRE6_BYE,77,0);assert(cafa_frame(f,9));drain(out);
    n=hello_parts(f,78,0);assert(cafa_frame(f,n));
    assert(drain(out)==22 && wire6_has(&wire6,3u) && !wire6_has(&wire6,2u));
    n=mixer(f,78,1);assert(cafa_frame(f,n));
    assert(drain(out)==13 && wire6.phase==2 && wire6.view[0]==4);
    assert(cafa_draw());
    assert(swap16(host_screen[73u*240u+8u])==T_SURF);
    assert(swap16(host_screen[84u*240u+100u])==T_THEME);
    /* Invalid Parts bodies cannot replace the accepted view. */
    {uint8_t valid[138];unsigned at;
     memcpy(valid,f+9,138);at=wire6.revision;
     f[49]=3;assert(cafa_frame(f,n));assert(drain(out)==13 && out[11]==WIRE6_INVALID && wire6.revision==at);
     memcpy(f+9,valid,138);f[141]=14;assert(cafa_frame(f,n));assert(drain(out)==13 && out[11]==WIRE6_INVALID);
     memcpy(f+9,valid,138);f[66]=1;assert(cafa_frame(f,n));assert(drain(out)==13 && out[11]==WIRE6_INVALID);
     memcpy(f+9,valid,138);f[107]='X';assert(cafa_frame(f,n));assert(drain(out)==13 && out[11]==WIRE6_INVALID);
     memcpy(f+9,valid,138);f[142]=1;assert(cafa_frame(f,n));assert(drain(out)==13 && out[11]==WIRE6_INVALID);
     assert(!memcmp(wire6.view,valid,138));
    }
    header(f,WIRE6_BYE,78,0);assert(cafa_frame(f,9));drain(out);
    {
        uint8_t nf[165],view[256];unsigned before;
        n=hello_native(nf,79);assert(cafa_frame(nf,n));
        assert(drain(out)==26 && wire6_has(&wire6,9u) && wire6_has(&wire6,11u));
        native_view(view,0x1234u);
        n=native_part(nf,79,1,view,0,152);before=so_w;
        assert(cafa_frame(nf,n) && so_w==before && wire6.phase==1 && wire6.view_len==0);
        n=native_part(nf,79,1,view,152,104);
        assert(cafa_frame(nf,n) && drain(out)==13 && wire6.phase==2 && wire6.view_len==256);
        assert(wire6.view[0]==CAFA_NATIVE_KIND && wire6.drawn_context==0);
        assert(cafa_draw() && wire6.drawn_context==0x1234u);
        native_view(view,0x5678u);
        n=native_part(nf,79,2,view,0,152);assert(cafa_frame(nf,n));
        assert(wire6.drawn_context==0x1234u && wire6_u28(wire6.view+2)==0x1234u);
        nf[13]=6;assert(cafa_frame(nf,n));
        assert(wire6.stage_len==152 && wire6_u28(wire6.view+2)==0x1234u);
        n=native_part(nf,79,2,view,152,104);
        assert(cafa_frame(nf,n) && drain(out)==13 && wire6_u28(wire6.view+2)==0x5678u);
        header(nf,WIRE6_BYE,79,0);assert(cafa_frame(nf,9));
    }
    {
        uint8_t nf[165],view[256];unsigned before;
        n=hello_native(nf,80);nf[10]=1;assert(cafa_frame(nf,n));
        assert(drain(out)==26 && wire6.core_minor==1u);
        native_view(view,0x7654u);
        n=native_part(nf,80,1,view,0,100);before=so_w;
        assert(cafa_frame(nf,n) && so_w==before && wire6.stage_len==100u);
        sx_ready=1;cafa_part_ready();assert(so_w==before); /* mailbox still owns this part */
        sx_ready=0;cafa_part_ready();
        assert(drain(out)==13u && out[5]==WIRE6_PART_READY &&
               wire6_u14(out+8)==1u && wire6_u14(out+10)==100u);
        n=native_part(nf,80,1,view,100,100);
        assert(cafa_frame(nf,n));cafa_part_ready();
        assert(drain(out)==13u && wire6_u14(out+10)==200u);
        before=so_w;
        n=native_part(nf,80,1,view,0,100);
        assert(cafa_frame(nf,n));cafa_part_ready();
        assert(so_w==before && wire6.stage_len==200u); /* older range cannot grant credit */
        n=native_part(nf,80,1,view,100,100);
        assert(cafa_frame(nf,n));cafa_part_ready();
        assert(drain(out)==13u && out[5]==WIRE6_PART_READY &&
               wire6_u14(out+10)==200u); /* lost credit can be recovered */
        n=native_part(nf,80,1,view,200,56);
        assert(cafa_frame(nf,n) && drain(out)==13u && wire6.phase==2u);
        header(nf,WIRE6_BYE,80,0);assert(cafa_frame(nf,9));
    }
    {
        uint8_t topology[281],bad[281];
        native_view(topology,0x88u);topology[6]=CAFA_SCREEN_OPERATOR;
        topology[7]=CAFA_NATIVE_VARIANT_FM_OPERATOR;topology[8]=4;
        topology[256]=CAFA_NATIVE_SERIES;cafa_put14(topology+257,10);
        topology[259]=1;topology[260]=0;topology[261]=3;topology[262]=1;
        cafa_put14(topology+263,4000);cafa_put14(topology+265,8000);
        cafa_put14(topology+267,9000);
        topology[269]=CAFA_NATIVE_EDGES;cafa_put14(topology+270,6);
        topology[272]=1;topology[273]=0;topology[274]=1;topology[275]=1;
        cafa_put14(topology+276,9000);
        assert(cafa_native_view_valid(topology,278));
        memcpy(bad,topology,278);bad[6]=CAFA_SCREEN_HOME;bad[7]=0;bad[8]=6;
        assert(!cafa_native_view_valid(bad,278));
        memcpy(bad,topology,278);cafa_put14(bad+257,13);bad[259]=2;
        bad[269]=bad[270]=bad[271]=0;bad[272]=CAFA_NATIVE_EDGES;
        cafa_put14(bad+273,6);memcpy(bad+275,topology+272,6);
        assert(!cafa_native_view_valid(bad,281));
    }
    delta_contract();
    panel_led_contract();
    puts("Open wire-6: discovery/rejections preserve standalone; shared owner, first-state takeover, full input drawn context, stale isolation, expiry and BYE passed");
    return 0;
}
