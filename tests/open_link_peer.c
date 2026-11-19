/* SPDX-License-Identifier: GPL-3.0-only */
/* Subprocess boundary for the public Java session. Uses production USB MIDI
 * assembly/mailbox, ownership, display and input; only the HAL is simulated. */
#include <assert.h>
#include "../firmware/src/cafa_protocol.h"
#define UI_TEST_NO_MAIN 1
#define FELUCCA_CAFA 1
#define FELUCCA_OTA 1
#define FELUCCA_UAC 1
#define FELUCCA_UAC_OUT 1
#ifdef HOST_DIAG_PEER
#define FELUCCA_CONTROLLER 1
#define FELUCCA_DIAG 1
static void cafa_diag_pcm(uint32_t,uint32_t);
static void cafa_diag_dac(uint32_t,uint32_t);
static int cafa_diag_pcm_needed(void),cafa_diag_dac_needed(void);
static uint32_t cafa_diag_audio_t0;
#endif
static cafa_link_t cafa;
static volatile uint8_t cafa_audio_on;
static int cafa_frame(const uint8_t *,uint32_t);
static int cafa_input(void),cafa_draw(void),cafa_leds(void);
#include "ui_test.c"
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { fm1_ms++; }
#include "../firmware/src/cafa.c"

static uint32_t read32(void)
{
    uint32_t value=0;
    unsigned k;
    for (k=0;k<4;k++) { int b=getchar(); if (b==EOF) exit(0); value=(value<<8)|(uint8_t)b; }
    return value;
}
static void write32(uint32_t value)
{
    int k;
    for (k=3;k>=0;k--) putchar((value>>(k*8))&255u);
}
int main(int argc,char **argv)
{
    uint8_t frame[167],reply[SXQ*3];
    ui_power_on(); usb.config=1;
    if(argc>1 && argv[1][0]=='m')song.master_q12=4096; /* paired host check's physical MASTER */
    for (;;) {
        unsigned service,frames,j,n=0;
        fm1_ms=read32(); service=read32(); fm1_in.notes=read32();
#ifdef HOST_DIAG_PEER
        host_ticks=fm1_ms*1000u;
#endif
        {uint32_t buttons=read32(),k;fm1_in.buttons=0;
         for(k=0;k<NB;k++)if(buttons&(1u<<k))fm1_in.buttons|=1u<<panel.btn[k];}
        host_enc[panel.enc[EN_SELECT]]=(int32_t)read32()*panel.dir[EN_SELECT];
        frames=read32();
        assert(frames<100);
        for (j=0;j<frames;j++) {
            unsigned size=read32(),i=0;
            assert(size<=sizeof frame && fread(frame,1,size,stdin)==size);
            while (i<size) {
                unsigned k=size-i>=3u?3u:size-i;
                unsigned cin=k==3u && i+3u<size?4u:k==3u?7u:4u+k;
                midi_in_event(cin|(uint32_t)frame[i]<<8|
                    (k>1?(uint32_t)frame[i+1]<<16:0)|(k>2?(uint32_t)frame[i+2]<<24:0));
                i+=k;
            }
        }
        if (service) {
            const uint8_t *p;
            uint32_t size;
#ifdef HOST_DIAG_PEER
            while (ota_frame_get(&p,&size)) { cafa_frame(p,size); ota_frame_done(); cafa_part_ready(); }
#else
            if (ota_frame_get(&p,&size)) { cafa_frame(p,size); ota_frame_done(); cafa_part_ready(); }
#endif
            cafa_input();
            cafa_draw();
        }
        while (so_r!=so_w) {
            uint32_t event=sx_out_q[so_r++%SXQ];
            unsigned cin=event&15u,k=cin==4?3u:cin-4u,i;
            assert(k<=3 && n+k<=sizeof reply);
            for (i=0;i<k;i++) reply[n++]=(event>>(8+8*i))&255;
        }
        write32(cafa.phase); write32(wire6.phase); write32(wire6.revision);
        write32(cafa_audio_on); write32(n);
        fwrite(reply,1,n,stdout);
        if(service&2u) {
            /* Test readback from the receiving side after its normal draw. */
            write32(wire6.drawn_context);
            write32(wire6.view_len);
            if(wire6.view_len)fwrite(wire6.view,1,wire6.view_len,stdout);
            write32(fnv(2166136261u,(const uint8_t *)host_screen,sizeof host_screen));
            uint32_t names=2166136261u;
            for(j=Y_GRAPH;j<Y_GRAPH+H_GRAPH;j++)
                names=fnv(names,host_screen+j*240u+28u,68u*sizeof *host_screen);
            write32(names);
        }
        fflush(stdout);
    }
}
