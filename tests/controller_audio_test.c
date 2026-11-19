/* SPDX-License-Identifier: GPL-3.0-only */
/* The controller's real DMA ISR, with a simulated codec and USB return. */
#include <stdint.h>
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main
#define FELUCCA_CAFA 1
#define FELUCCA_CONTROLLER 1
#define FELUCCA_UAC 1
#define FELUCCA_UAC_OUT 1
#define FM1_AUDIO_HALF 0x80u
#define FM1_TICKS_PER_US 1u
static uint32_t host_time, host_acks, host_returns, host_gain;
static uint8_t host_pending = FM1_AUDIO_HALF;
static uint32_t fm1_ticks(void) { return ++host_time; }
static uint8_t fm1_audio_pending(void) { return host_pending; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return 0; }
static void fm1_audio_ack_half(void) { host_acks++; }
static void fm1_audio_init(int32_t *b, uint32_t n, void (*isr)(void), uint32_t p)
{ (void)b; (void)n; (void)isr; (void)p; }
void isr_alnk0(void) {}
static void uac_render_start(void) {}
static void uac_play(int32_t *out, uint32_t n, uint32_t gain)
{
    uint32_t i;
    host_returns++; host_gain=gain;
    for (i=0;i<2u*n;i++) { assert(out[i]==0); out[i]=17; }
}
#include "../firmware/src/audio.c"
int main(void)
{
    uint32_t i;
    song.master_q12=4096;
    for (i=0;i<2u*HALF_WORDS;i++) abuf[i]=99;
    cafa_usb_return_on=0; fm1_alnk0_irq();
    assert(host_acks==1 && host_returns==0);
    for (i=0;i<HALF_WORDS;i++) assert(abuf[i]==0);
    cafa_usb_return_on=1; fm1_alnk0_irq();
    assert(host_acks==2 && host_returns==1 && host_gain==2048);
    for (i=0;i<HALF_WORDS;i++) assert(abuf[i]==17);
    cafa_usb_return_on=0; fm1_alnk0_irq();
    assert(host_acks==3 && host_returns==1);
    for (i=0;i<HALF_WORDS;i++) assert(abuf[i]==0);
    cafa_host_master=1;cafa_usb_return_on=1;song.master_q12=321;
    fm1_alnk0_irq();assert(host_gain==4096 && host_returns==2);
    host_pending=0;fm1_alnk0_irq();assert(host_acks==4);
    puts("Controller audio ISR: cold idle, USB return, disconnect and auxiliary interrupt passed");
    return 0;
}
