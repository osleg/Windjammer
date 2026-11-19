/* SPDX-License-Identifier: GPL-3.0-only
 * Windjammer controller: phone-authoritative surface, no writes to the standalone project. */
static uint32_t cafa_buttons, cafa_keys, cafa_tx_serial;
static uint8_t cafa_dirty, cafa_first, cafa_release_guard;
#if FELUCCA_CONTROLLER
static uint8_t cafa_idle_menu, cafa_idle_dirty;
static uint32_t cafa_idle_home_t;
#endif
static uint8_t cafa_native_shown __attribute__((section(".bss.cafa_ui")));
static uint8_t cafa_drawn[CAFA_DYNAMIC_BYTES] __attribute__((section(".bss.cafa_ui"))); /* last pixels shown, not merely last state received */
#if FELUCCA_UAC_OUT
#define WIRE6_AUDIO_MODES 3u
#else
#define WIRE6_AUDIO_MODES 1u
#endif
#include "cafa_wire6.h"
#if FELUCCA_DIAG
#include "windjammer_marker_detector.h"
#endif
static wire6_link_t wire6 __attribute__((section(".bss.cafa_ui")));
/* Appended after retained settings/projects; never persistent session state.
 * Explicitly cleared before use because this RAM is not zeroed at startup. */
static uint8_t wire6_buffer_a[CAFA_NATIVE_MAX] __attribute__((section(".noinit.cafa_ui")));
static uint8_t wire6_buffer_b[CAFA_NATIVE_MAX] __attribute__((section(".pool")));
static uint8_t wire6_dirty;
static uint16_t wire6_drawn_len;
static uint8_t wire6_drawn_view[WIRE6_MAX_BODY] __attribute__((section(".bss.cafa_ui")));
static uint8_t wire6_device_owned;
static uint16_t wire6_last_master;
static uint8_t wire6_master_decided;
static uint32_t wire6_master_ms;
static uint32_t wire6_drawn_palette;
static uint8_t wire6_drawn_prefs, wire6_drawn_screen, wire6_drawn_variant;
static uint8_t wire6_backdrop_dirty;
static uint8_t wire6_region_dirty, wire6_led_dirty;
static uint8_t wire6_part_ready_pending;
static uint16_t wire6_part_ready_session,wire6_part_ready_revision,wire6_part_ready_offset;
static int wire6_input(void),wire6_draw(void),wire6_leds(void);
#ifndef FELUCCA_DIAG
#define FELUCCA_DIAG 0
#endif
#if FELUCCA_DIAG
/* Exact opt-in physical key to DAC measurement; all ticks use device TIMER4. */
static struct {
    uint8_t running, started, key_stage, key_quiet;
    uint16_t key_serial;
    uint32_t start_ms, stop_ms, key_tick, key_quiet_tick, marker_index;
    uint32_t marker_under, marker_over;
    uint32_t key_samples, key_last_us, key_last_serial, key_unmatched;
    uint32_t key_enqueue_tick, key_recognized_tick, key_scan_enqueue_us, key_scan_recognized_us;
    uint32_t key_last_enqueue_us, key_last_recognized_us, key_publish;
    uint32_t key_downscans, key_reports, detected_markers, dac_misses;
} volatile cafa_diag;
static windjammer_marker_detector_t cafa_marker;
static int cafa_diag_return_active(void)
{
#if FELUCCA_DIRECT_PCM
    return cafa_return_active();
#else
    return cafa_usb_return_on;
#endif
}
static int cafa_diag_live(void)
{
    return cafa_diag.running && fm1_ms-cafa_diag.start_ms<90000u &&
           wire6.phase==2 && wire6.diagnostic && cafa_diag_return_active();
}
static void cafa_diag_miss_key(void)
{
    if(cafa_diag.key_stage) {
        cafa_diag.key_unmatched++;
        if(cafa_diag.key_stage==3u)cafa_diag.dac_misses++;
    }
    cafa_diag.key_stage=0;
}
static void cafa_diag_key_downscan(uint32_t tick)
{
    if(!cafa_diag_live())return;
    cafa_diag.key_downscans++;
    /* Identical audio cues cannot identify which of two overlapping physical
     * presses produced a marker. Discard both and wait one trial bound after
     * the last overlap before admitting another isolated key. */
    if(cafa_diag.key_stage ||
       (cafa_diag.key_quiet && tick-cafa_diag.key_quiet_tick<2u*1000u*1000u*FM1_TICKS_PER_US)) {
        cafa_diag_miss_key();cafa_diag.key_unmatched++;
        cafa_diag.key_quiet=1;cafa_diag.key_quiet_tick=tick;
        return;
    }
    cafa_diag.key_quiet=0;cafa_diag.key_stage=1;cafa_diag.key_tick=tick;
}
static int cafa_diag_pcm_needed(void) {return cafa_diag_live() && cafa_diag.key_stage==2u;}
static int cafa_diag_dac_needed(void) {return cafa_diag_live() && cafa_diag.key_stage==3u;}
static void cafa_diag_pcm(uint32_t index,uint32_t word)
{
    uint32_t start;
    if(!cafa_diag_live() || cafa_diag.key_stage!=2u)return;
    if(windjammer_marker_feed(&cafa_marker,index,(int16_t)(word>>16),&start)) {
        if(wire6.diagnostic==2u)cafa_diag.key_recognized_tick=fm1_ticks();
        cafa_diag.marker_index=start;
        cafa_diag.marker_under=uo.underruns;cafa_diag.marker_over=uo.overruns;
        cafa_diag.detected_markers++;
        cafa_diag.key_stage=3u;
    }
}
/* idx is the source frame selected before interpolation; tick is the start
 * of its DAC frame in the next DMA half. */
static void cafa_diag_dac(uint32_t idx,uint32_t tick)
{
    if(!cafa_diag_live() || cafa_diag.key_stage!=3u)return;
    if(idx<cafa_diag.marker_index)return;
    if(idx-cafa_diag.marker_index>2u ||
       uo.underruns!=cafa_diag.marker_under || uo.overruns!=cafa_diag.marker_over ||
       tick-cafa_diag.key_tick>=2u*1000u*1000u*FM1_TICKS_PER_US) {
        cafa_diag_miss_key();return;
    }
    if(wire6.diagnostic==2u) {
        uint32_t total=tick-cafa_diag.key_tick;
        uint32_t enqueue=cafa_diag.key_enqueue_tick-cafa_diag.key_tick;
        uint32_t recognized=cafa_diag.key_recognized_tick-cafa_diag.key_tick;
        uint32_t limit=2u*1000u*1000u*FM1_TICKS_PER_US;
        cafa_diag.key_scan_enqueue_us=enqueue<=recognized && recognized<=total && total<limit ?
            enqueue/FM1_TICKS_PER_US : 0u;
        cafa_diag.key_scan_recognized_us=enqueue<=recognized && recognized<=total && total<limit ?
            recognized/FM1_TICKS_PER_US : 0u;
    }
    cafa_diag.key_publish++;
    cafa_diag.key_last_us=(tick-cafa_diag.key_tick)/FM1_TICKS_PER_US;
    cafa_diag.key_last_serial=cafa_diag.key_serial;
    cafa_diag.key_last_enqueue_us=cafa_diag.key_scan_enqueue_us;
    cafa_diag.key_last_recognized_us=cafa_diag.key_scan_recognized_us;
    cafa_diag.key_samples++;cafa_diag.key_publish++;cafa_diag.key_stage=0;
}
static void cafa_diag_start(void)
{
    memset((void *)&cafa_diag,0,sizeof cafa_diag);
    windjammer_marker_reset(&cafa_marker);
    cafa_diag.running=cafa_diag.started=1;cafa_diag.start_ms=fm1_ms;
}
static void cafa_diag_end(void)
{
    if(!cafa_diag.running)return;
    cafa_diag_miss_key();
    cafa_diag.stop_ms=fm1_ms;cafa_diag.running=0;
}
static void cafa_diag_timeout(void)
{
    if(cafa_diag.running && fm1_ms-cafa_diag.start_ms>=90000u)cafa_diag_end();
}
static void cafa_diag_input(uint16_t serial,uint32_t keys)
{
    if(!cafa_diag_live())return;
    if((keys&1u) && !(cafa_keys&1u)) {
        cafa_diag.key_reports++;
        if(cafa_diag.key_stage!=1u)return;
        if(wire6.diagnostic==2u)cafa_diag.key_enqueue_tick=fm1_ticks();
        cafa_diag.key_serial=serial;cafa_diag.key_stage=2;
        windjammer_marker_reset(&cafa_marker);
    }
}
#endif
/* Entire frames queued atomically. Never block input/the lease watchdog on a MIDI host. */
static int cafa_send_to(uint32_t version,uint32_t session,uint32_t cmd,uint32_t serial,const uint8_t *p,uint32_t n)
{
    uint8_t f[82u];
    uint32_t i=0, size=n+11u;
    if (!usb.config || size>sizeof f || SXQ-(so_w-so_r)<(size+2u)/3u) return 0;
    f[0]=0xf0; f[1]=0x7d; f[2]=0x43; f[3]=0x41; f[4]=(uint8_t)version; f[5]=(uint8_t)cmd;
    cafa_put14(f+6,session); cafa_put14(f+8,serial);
    if (n) memcpy(f+10,p,n);
    f[size-1]=0xf7;
    sx_busy=1;
    while (i<size) {
        uint32_t k=size-i>=3u ? 3u : size-i;
        uint32_t cin=k==3u && i+3u<size ? 4u : k==3u ? 7u : 4u+k;
        sx_out_q[so_w%SXQ]=cin | (uint32_t)f[i]<<8 | (k>1 ? (uint32_t)f[i+1]<<16 : 0) |
                          (k>2 ? (uint32_t)f[i+2]<<24 : 0);
        RING_PUBLISH(); so_w++; i+=k;
    }
    sx_busy=0;
    return 1;
}
static int cafa_send(uint32_t cmd,uint32_t serial,const uint8_t *p,uint32_t n)
{ return cafa_send_to(cafa.version,cafa.session,cmd,serial,p,n); }
#if FELUCCA_DIAG
static void cafa_diag_reply(uint8_t op,uint16_t serial)
{
    uint8_t out[2u+11u*4u];uint32_t v[11],i,seq,elapsed=fm1_ms-cafa_diag.start_ms;
    if(cafa_diag.key_stage && fm1_ticks()-cafa_diag.key_tick>=2u*1000u*1000u*FM1_TICKS_PER_US)
        cafa_diag_miss_key();
    if(!cafa_diag.running)elapsed=cafa_diag.stop_ms-cafa_diag.start_ms;
    if(elapsed>90000u)elapsed=90000u;
    do {
        seq=cafa_diag.key_publish;
        if(seq&1u)continue;
        v[1]=cafa_diag.key_samples;v[2]=cafa_diag.key_last_us;
        v[3]=cafa_diag.key_last_serial;
        v[9]=cafa_diag.key_last_enqueue_us;
        v[10]=cafa_diag.key_last_recognized_us;
    } while(seq!=cafa_diag.key_publish || (seq&1u));
    v[0]=elapsed;v[4]=cafa_diag.key_unmatched;
    v[5]=cafa_diag.key_downscans;v[6]=cafa_diag.key_reports;
    v[7]=cafa_diag.detected_markers;v[8]=cafa_diag.dac_misses;
    out[0]=op;out[1]=wire6.diagnostic==2u ? 4u : 3u;
    for(i=0;i<(wire6.diagnostic==2u ? 11u : 9u);i++)wire6_put28(out+2u+4u*i,v[i]);
    (void)cafa_send_to(6,wire6.session,WIRE6_DIAG,serial,out,
                       wire6.diagnostic==2u ? sizeof out : 2u+9u*4u);
}
#endif
static void cafa_silence_standalone(void)
{
#if FELUCCA_CONTROLLER
    /* The controller never starts voices or a sequencer. Drain the USB return on
     * ownership changes so old host samples cannot play in a new session. */
#if FELUCCA_UAC_OUT
#if FELUCCA_DIRECT_PCM
    if(!direct.alt)
#endif
    uac_play_flush();
#endif
#else
    uint32_t k;
    /* The audio ISR owns voices. Hold it while changing mode, and empty pending input. */
    fm1_irq_off();
    seq_stop(); transport_req=panic_req=0;
    /* Mark EVERY outgoing note owed before a bounded flush can run out of queue room. */
    memcpy(mn.owe,mn.on,sizeof mn.on); mn.lost=1;
    mo_flush((1u<<NTRK)-1u);
    for (k=0;k<NTRK;k++) {
        memset(trk[k].v,0,sizeof trk[k].v);
        trk[k].nheld=0; trk[k].arp_phys=0; arp_forget(&trk[k]); midi_forget_track(k);
    }
    memset(kb_ped,0,sizeof kb_ped); kb_ped_trk=0;
    memset(kb_note,KB_SILENT,sizeof kb_note); kb_prev=fm1_in.notes;
    mi_r=mi_w; midi_in_overflow=0;
    kb_boot_hold=1;
#if FELUCCA_UAC_OUT
    uac_play_flush();
#endif
    fm1_irq_on();
    /* Audio is bypassed during this reset, so large effect buffers can clear with IRQs on. */
    memset(dly_buf,0,sizeof dly_buf); memset(cho_buf,0,sizeof cho_buf); rev_clear();
    /* Scalar stores let the target compiler keep hot mixer globals split. */
    fx.dly_w=fx.cho_w=fx.cho_ph=0; fx.dly_lp=0;
    for (k=0;k<4;k++) { fx.comb_i[k]=0; fx.comb_lp[k]=0; fx.ph_s[k]=0; }
    for (k=0;k<2;k++) fx.ap_i[k]=0;
    fx.rtype=0; fx.sp_w=0; fx.sp_lp=fx.sp_hp=fx.sp_he=fx.sp_size=0; fx.sp_ph=0;
    fx.cho_y=0; fx.dmode=fx.dclr=0; fx.dly_lpr=fx.dpl=fx.dpr=fx.dmp=fx.dsp=0; memset(&pf,0,sizeof pf); memset(ins,0,sizeof ins);
    memset(&clk,0,sizeof clk); click_req=0;
    perf_held=perf_latched=perf_act=0;
#endif
}
static void cafa_leave(void)
{
    /* Negotiation alone never owns or interrupts the standalone instrument. */
    if (cafa.phase!=2
#if !FELUCCA_CONTROLLER
        && !cafa_audio_on
#endif
    ) { cafa.phase=0; return; }
    cafa_audio_on=1;
#if FELUCCA_DIRECT_PCM
    direct_session_stop();
#endif
    cafa_silence_standalone();
    cafa.phase=0; cafa_release_guard=1;
#if FELUCCA_CONTROLLER
    cafa_usb_return_on=0;
#else
    cafa_audio_on=0;
#endif
    ui.force=1; ui.home_t0=ui.save_t0=0;
    ui_message("CAFA DISCONNECTED");
}
static void wire6_leave(void)
{
    int active=wire6.phase==2;
#if FELUCCA_DIRECT_PCM
    direct_session_stop();
#endif
#if FELUCCA_DIAG
    cafa_diag.running=0;cafa_diag.key_stage=0;
    sx_diag_transport=0;
    sx_next_ready=sx_diag_ready=0;
#endif
    cafa_host_master=wire6_master_decided=0;
    wire6_part_ready_pending=0;
    if(active && wire6_has(&wire6,WIRE6_NATIVE_PANEL))fm1_led_dim_level(settings_leds==LEDS_DIM_LO);
#if FELUCCA_CONTROLLER
    cafa_panel_midi_active=0;cafa_panel_midi_overflow=0;
    fm1_irq_off();
    mo_r=mo_w;                       /* no old TRS note may reach a new session */
    fm1_irq_on();
#endif
    if(active) {
        cafa_audio_on=1;cafa_silence_standalone();
        cafa_release_guard=1;ui.force=1;ui.home_t0=ui.save_t0=0;
        ui_message("COMPANION DISCONNECTED");
    }
    cafa_usb_return_on=0;
#if FELUCCA_DIAG && FELUCCA_UAC_OUT
    cafa_keytime_return=0;
#endif
#if !FELUCCA_CONTROLLER
    cafa_audio_on=0;cafa_usb_return_on=1;
#endif
    memset(&wire6,0,sizeof wire6);
    memset(wire6_buffer_a,0,sizeof wire6_buffer_a);
    wire6.view=wire6_buffer_a;wire6.stage=wire6_buffer_b;
    wire6_dirty=0;wire6_drawn_len=0;wire6_device_owned=0;wire6_backdrop_dirty=0;
    wire6_region_dirty=0;
    cafa_native_profile_reset();
#if FELUCCA_CONTROLLER
    if(cafa_idle_menu)cafa_native_device_open_local();
#endif
}
static int wire6_frame(const uint8_t *p,uint32_t n)
{
    wire6_reply_t r;uint8_t payload[6u+4u*8u],i,count;
    uint16_t old_len=wire6.view_len;
#if FELUCCA_DIRECT_PCM
    uint32_t source_epoch=0;
    uint8_t source=0;
#endif
    if(wire6_expired(&wire6,fm1_ms))wire6_leave();
    if(!wire6.view) {
        memset(wire6_buffer_a,0,sizeof wire6_buffer_a);
        wire6.view=wire6_buffer_a;wire6.stage=wire6_buffer_b;
    }
    r=wire6_receive(&wire6,p,n,fm1_ms);
#if FELUCCA_DIAG
    if(r.command==WIRE6_HELLO && r.result==WIRE6_OK && wire6.diagnostic)
        sx_diag_transport=1;
#endif
    if(!r.action)return 1;
    if(r.action==WIRE6_CLOSE) {wire6_leave();return 1;}
    if(r.action==WIRE6_SEND_OFFER) {
        payload[0]=1;payload[1]=2;payload[2]=WIRE6_AUDIO_MODES;cafa_put14(payload+3,156);
        /* Page zero retains the original eight-record maximum for older apps. */
        payload[5]=r.serial ? 1u : sizeof wire6_capabilities;count=6;
        for(i=0;i<payload[5];i++) {
            payload[count++]=r.serial ? WIRE6_NATIVE_CHROME : wire6_capabilities[i];payload[count++]=1;
            payload[count++]=0;payload[count++]=0;
        }
        if(!cafa_send_to(6,r.session,WIRE6_OFFER,r.serial,payload,count) && wire6.phase)wire6_leave();
        return 1;
    }
    if(r.action==WIRE6_SEND_PART_READY) {
        wire6_part_ready_session=r.session;
        wire6_part_ready_revision=r.serial;
        wire6_part_ready_offset=wire6.stage_len;
        wire6_part_ready_pending=1;
        return 1;
    }
#if FELUCCA_DIAG
    if(r.command==WIRE6_DIAG && r.result==WIRE6_OK) {
        uint8_t op=p[9];
        if(!cafa_diag_return_active() || (op!=1u && !cafa_diag.started))
            r.result=WIRE6_BUSY;
        else {
            if(op==1u && !cafa_diag.running)cafa_diag_start();
            if(op==3u || fm1_ms-cafa_diag.start_ms>=90000u)cafa_diag_end();
            cafa_diag_reply(op,r.serial);return 1;
        }
    }
#endif
    if((r.action==WIRE6_FIRST_STATE || r.action==WIRE6_NEW_STATE) && !wire6_master_decided &&
       wire6.view[0]==CAFA_NATIVE_KIND && !(wire6.view[1]&8u)) {
        const uint8_t *prefs=cafa_native_section(wire6.view,wire6.view_len,CAFA_NATIVE_PREFS,0);
        cafa_host_master=(uint8_t)(prefs && (prefs[1]&8u));wire6_master_decided=1;
    }
    if(r.action==WIRE6_FIRST_STATE) {
        /* A valid first image is the sole physical takeover point. */
#if FELUCCA_DIRECT_PCM
        source_epoch=direct.epoch;
        source=wire6.audio_mode==1 ? (direct.alt ? 2u : 1u) : 0u;
        direct_session_stop();
#endif
#if FELUCCA_CONTROLLER
        cafa_idle_menu=cafa_idle_dirty=0;
        cafa_native_device_reset();
#endif
        cafa_usb_return_on=0;cafa_audio_on=1;
        cafa_silence_standalone();
        cafa_first=1;cafa_tx_serial=0;cafa_buttons=cafa_keys=0;
        wire6_led_dirty=1;wire6_dirty=2;wire6_region_dirty=CAFA_PAINT_FULL;scr_wake_now();
    } else if(r.action==WIRE6_NEW_STATE) {
        const uint8_t *old=wire6.stage,*now=wire6.view;
        wire6_led_dirty=1;
        if(now[0]==CAFA_NATIVE_KIND) {
            uint8_t regions=cafa_native_profile_regions(old,old_len,now,wire6.view_len);
            wire6_region_dirty|=regions;
        }
        if(now[0]==CAFA_NATIVE_KIND && old_len==wire6.view_len && old[0]==CAFA_NATIVE_KIND &&
           !memcmp(old,now,2u) && !memcmp(old+6,now+6,wire6.view_len-6u)) {
            uint32_t context=wire6_u28(now+2);
            int menu=(now[6]>=57u && now[6]<=62u) || now[6]==70u || now[6]==71u;
            if(menu && wire6_u28(old+2)!=context) {
                if(!wire6_dirty)wire6_dirty=1;
            } else if(!wire6_dirty && wire6_drawn_len==wire6.view_len &&
                      wire6_drawn_view[0]==CAFA_NATIVE_KIND &&
                      wire6_drawn_palette==ux.gen && wire6_drawn_prefs==ui_prefs)
                wire6.drawn_context=context;
        } else {
            if(now[0]==CAFA_NATIVE_KIND && old_len>=CAFA_NATIVE_HEADER && old[0]==CAFA_NATIVE_KIND &&
               memcmp(old+CAFA_NATIVE_CONTROLS,now+CAFA_NATIVE_CONTROLS,
                      CAFA_NATIVE_HEADER-CAFA_NATIVE_CONTROLS))wire6_backdrop_dirty=1;
            if(!wire6_dirty)wire6_dirty=1;
        }
    }
    count=2;
    payload[0]=r.command;payload[1]=r.result;
    if(r.command==WIRE6_HELLO && r.result==WIRE6_OK) {
        payload[count++]=wire6.accepted_count;
        for(i=0;i<wire6.accepted_count;i++) {
            payload[count++]=wire6.accepted[i];payload[count++]=1;
            payload[count++]=0;payload[count++]=0;
        }
    }
    if(!cafa_send_to(6,r.session,WIRE6_ACK,r.serial,payload,count)) {
        if(wire6.phase)wire6_leave();
    } else if(r.action==WIRE6_FIRST_STATE) {
#if FELUCCA_DIRECT_PCM
        direct_session_publish(source_epoch,source,wire6.diagnostic);
#else
        if(wire6.audio_mode==1) {
#if FELUCCA_DIAG && FELUCCA_UAC_OUT
            cafa_keytime_return=wire6.diagnostic;
#endif
            cafa_usb_return_on=1;
        }
#endif
#if FELUCCA_CONTROLLER
        cafa_panel_midi_active=(uint8_t)wire6_has(&wire6,WIRE6_PANEL_MIDI);
        cafa_panel_midi_overflow=0;
#endif
    }
    return 1;
}
/* A PART_READY is credit for one more fragment. Publish it after the USB
 * mailbox has been released, so a fast host cannot overwrite an unread view. */
static void cafa_part_ready(void)
{
    uint8_t offset[2];
    if(!wire6_part_ready_pending || wire6.session!=wire6_part_ready_session ||
       !wire6.phase || wire6.stage_revision!=wire6_part_ready_revision ||
       wire6.stage_len!=wire6_part_ready_offset) {
        wire6_part_ready_pending=0;return;
    }
#if FELUCCA_DIAG
    if(sx_ready && (!sx_diag_transport || sx_next_ready))return;
#else
    if(sx_ready)return;
#endif
    cafa_put14(offset,wire6_part_ready_offset);
    if(cafa_send_to(6,wire6_part_ready_session,WIRE6_PART_READY,
                    wire6_part_ready_revision,offset,sizeof offset))
        wire6_part_ready_pending=0;
}
static int cafa_frame(const uint8_t *p, uint32_t n)
{
    int action, changed;
    uint8_t ack=1;
    if (n<3 || p[0]!=0x7d || p[1]!=0x43 || p[2]!=0x41) return 0;
    if(cafa_expired(&cafa,fm1_ms))cafa_leave();
    if(n>=4 && p[3]==6u) {
        if(n>=9u && p[4]==WIRE6_HELLO && cafa.phase) {
            wire6_link_t candidate={0};
            wire6_reply_t checked=wire6_receive(&candidate,p,n,fm1_ms);
            uint8_t response[2]={WIRE6_HELLO,checked.result==WIRE6_INVALID ? WIRE6_INVALID:WIRE6_BUSY};
            if(checked.action==WIRE6_SEND_ACK)
                cafa_send_to(6,checked.session,WIRE6_ACK,0,response,2);
            return 1;
        }
        return wire6_frame(p,n);
    }
    if(wire6.phase)return 1; /* legacy cannot steal a pending or active v6 owner */
    /* Held/step LED bytes 14..19 are not LCD content. A key press must not blank
     * the display just because its physical LED changes. */
    changed=n>=9u && n==9u+cafa_state_bytes(p[3]) && (memcmp(cafa.view,p+9,14) ||
            memcmp(cafa.view+20,p+29,cafa_state_bytes(p[3])-20u));
    action=cafa_receive(&cafa,p,n,fm1_ms);
    if (action==1 || action==5) {
        if (!cafa_send(CAFA_ACK,action==1 ? 0u : cafa.revision,&ack,1)) cafa_leave();
    } else if (action==2) {
#if FELUCCA_CONTROLLER
        cafa_idle_menu=cafa_idle_dirty=0;
        cafa_native_device_reset();
#endif
#if FELUCCA_DIRECT_PCM
        direct_session_stop();
#endif
        cafa_audio_on=1;
        cafa_silence_standalone();
        cafa_audio_on=1; cafa_first=1; cafa_tx_serial=0; cafa_buttons=cafa_keys=0;
        cafa_dirty=2; cafa_native_shown=0; scr_wake_now(); /* takeover invalidates the whole display cache */
    } else if (action==3 && changed && !cafa_dirty) cafa_dirty=1;
    else if (action==4) cafa_leave();
    /* The USB receive mailbox holds one frame. A heartbeat arriving directly
     * after STATE can be dropped before the main loop consumes STATE. Confirm
     * every applied snapshot itself, so valid state traffic cannot starve ACKs. */
    if ((action==2 || action==3) && !cafa_send(CAFA_ACK,cafa.revision,&ack,1)) cafa_leave();
    return 1;
}
/* Full held snapshots plus relative deltas, emitted atomically. */
static int cafa_emit(uint8_t *p,uint32_t b,uint32_t keys)
{
    uint32_t k;
    cafa_put14(p,b);
    for (k=0;k<4;k++) p[2+k]=(keys>>(7*k))&127u;
    if(cafa.version==5) {
        const uint8_t *shown=cafa_native_shown ? cafa_drawn : cafa.view;
        memcpy(p+20,shown+152,2);
    }
    if (!cafa_send(CAFA_INPUT,(cafa_tx_serial+1u)&16383u,p,cafa.version==5 ? 22u : 20u)) { cafa_leave(); return 0; }
    cafa_tx_serial=(cafa_tx_serial+1u)&16383u; cafa_first=0; cafa_buttons=b; cafa_keys=keys;
    return 1;
}
static int wire6_emit(uint8_t *p,uint32_t b,uint32_t keys)
{
    uint32_t k,bytes=24;
    cafa_put14(p,b);
    for(k=0;k<4;k++)p[2+k]=(uint8_t)((keys>>(7*k))&127u);
    if(wire6_device_owned)p[5]|=64u;
    wire6_put28(p+20,wire6.drawn_context);
    if(wire6_has(&wire6,WIRE6_NATIVE_PANEL)) {
        wire6_last_master=(uint16_t)clamp(song.master_q12,0,4096);
        cafa_put14(p+24,wire6_last_master);bytes=26;wire6_master_ms=fm1_ms;
    }
    if(!cafa_send_to(6,wire6.session,WIRE6_INPUT,(cafa_tx_serial+1u)&16383u,p,bytes)) {
        wire6_leave();return 0;
    }
    cafa_tx_serial=(cafa_tx_serial+1u)&16383u;cafa_first=0;
#if FELUCCA_DIAG
    cafa_diag_input((uint16_t)cafa_tx_serial,keys);
#endif
    cafa_buttons=b;cafa_keys=keys;return 1;
}
static int wire6_input(void)
{
    uint32_t k,b=0,keys=fm1_in.notes&((1u<<27)-1u),changed=0,pressed,np,bp=0,rb,rk;
    int32_t turns[7];
    uint8_t old_owned=wire6_device_owned;
    uint8_t p[26]={0},release[26]={0};
    if(wire6.phase && (!usb.config || wire6_expired(&wire6,fm1_ms))) {
        int active=wire6.phase==2;wire6_leave();if(active)return 1;
    }
    if(wire6.phase!=2)return 0;
    pressed=fm1_input_edges(0);np=fm1_input_note_edges()&((1u<<27)-1u);
    for(k=0;k<NB;k++) {
        b|=((fm1_in.buttons>>panel.btn[k])&1u)<<k;
        bp|=((pressed>>panel.btn[k])&1u)<<k;
    }
    for(k=0;k<NE;k++) {
        int32_t d=panel_enc(k);
        if(d>8191 || d< -8192) {wire6_leave();return 1;}
        changed|=d!=0;turns[k]=d;
        cafa_put14(release+6+2*k,8192);
    }
    b|=bp;keys|=np;
    if(wire6.view_len>=CAFA_NATIVE_HEADER && wire6.view[0]==CAFA_NATIVE_KIND) {
        /* The menu can consume the first turn immediately after it is drawn,
         * before the host has received the first device-owned input report. */
        int menu_input=cafa_device_menu.active &&
                       (changed || ((b^cafa_device_menu.buttons)&~(1u<<B_PLAY)));
        int mode=cafa_native_device_input(&b,&keys,turns);
        wire6_device_owned=(uint8_t)(mode==1);
        if(wire6_device_owned!=old_owned)changed=1;
        if(menu_input && !wire6_dirty)wire6_dirty=1;
    } else wire6_device_owned=0;
    for(k=0;k<NE;k++) {
        changed|=turns[k]!=0;
        cafa_put14(p+6+2*k,(uint32_t)(turns[k]+8192));
    }
    rb=cafa_buttons&bp;rk=cafa_keys&np;
    if((rb || rk) && !wire6_emit(release,cafa_buttons&~rb,cafa_keys&~rk))return 1;
    if(wire6_has(&wire6,WIRE6_NATIVE_PANEL) && fm1_ms-wire6_master_ms>=34u) {
        int gain=clamp(song.master_q12,0,4096),d=gain-(int)wire6_last_master;
        changed|=d>=8 || d<=-8 || (d && (gain==0 || gain==4096));
    }
    if(!changed && b==cafa_buttons && keys==cafa_keys && !cafa_first)return 1;
    wire6_emit(p,b,keys);return 1;
}
/* Return true while the standalone input must not consume physical intents. */
#if FELUCCA_CONTROLLER
static void cafa_idle_device_input(void)
{
    uint32_t k,b=0,keys=fm1_in.notes,pressed=fm1_input_edges(0);
    int32_t turns[7];
    int changed=0,mode;
    fm1_input_note_edges();
    for(k=0;k<NB;k++)
        b|=(((fm1_in.buttons|pressed)>>panel.btn[k])&1u)<<k;
    for(k=0;k<NE;k++) {
        turns[k]=panel_enc(k);
        changed|=turns[k]!=0;
    }
    changed|=b!=cafa_device_menu.buttons;
    mode=cafa_native_device_input(&b,&keys,turns);
    if(mode==2) {
        cafa_idle_menu=0;cafa_idle_home_t=0;
        ui.force=1;
    } else if(changed)cafa_idle_dirty=1;
}
#endif
static int cafa_input(void)
{
#if FELUCCA_CONTROLLER
    if(cafa_panel_midi_overflow) {
        /* Release the phone's held notes promptly. A full raw MIDI queue is
         * discarded before BYE, which has its own SysEx queue. */
        cafa_panel_midi_active=0;
        fm1_irq_off();
        mo_r=mo_w;
        so_r=so_w;                   /* retire pending display chunks for urgent BYE */
        fm1_irq_on();
        (void)cafa_send_to(6,wire6.session,WIRE6_BYE,0,0,0);
        wire6_leave();
        return 1;
    }
#endif
    uint32_t k,b=0,keys=fm1_in.notes,changed=0,pressed,np,bp=0,rb,rk;
    uint8_t p[24], release[24]={0};
    if(wire6.phase)return wire6_input();
    if (cafa.phase && (!usb.config || cafa_expired(&cafa,fm1_ms))) cafa_leave();
    if (cafa.phase!=2 && !cafa_release_guard) {
#if FELUCCA_CONTROLLER
        if(cafa_idle_menu) {cafa_idle_device_input();return 1;}
        fm1_input_edges(0);fm1_input_note_edges();
        for(k=0;k<NE;k++)panel_enc(k);
        if(fm1_in.buttons&(1u<<panel.btn[B_HOME])) {
            if(!cafa_idle_home_t)cafa_idle_home_t=fm1_ms+1u;
            else if((uint32_t)(fm1_ms+1u-cafa_idle_home_t)>=700u) {
                cafa_native_device_open_local();
                cafa_idle_menu=cafa_idle_dirty=1;
                cafa_idle_home_t=0;
                scr_wake_now();
            }
        } else cafa_idle_home_t=0;
        return 1;
#else
        return 0;
#endif
    }
    pressed=fm1_input_edges(0); np=fm1_input_note_edges();
    for (k=0;k<NB;k++) {
        b|=((fm1_in.buttons>>panel.btn[k])&1u)<<k;
        bp|=((pressed>>panel.btn[k])&1u)<<k;
    }
    for (k=0;k<NE;k++) {
        int32_t d=panel_enc(k);
        if (d>8191 || d< -8192) { cafa_leave(); return 1; }
        changed|=d!=0;
        cafa_put14(p+6+2*k,(uint32_t)(d+8192));
        cafa_put14(release+6+2*k,8192);
    }
    if (cafa_release_guard) {
        kb_prev=keys;
        if (!keys && !b) { cafa_release_guard=0; kb_boot_hold=0; }
        return 1;
    }
    /* Press latches preserve taps completed during LCD transfers. A re-press of a
     * previously held control emits its intervening release before the new press. */
    rb=cafa_buttons&bp; rk=cafa_keys&np;
    if ((rb || rk) && !cafa_emit(release,cafa_buttons&~rb,cafa_keys&~rk)) return 1;
    b|=bp; keys|=np;
    if (!changed && b==cafa_buttons && keys==cafa_keys && !cafa_first) return 1;
    cafa_emit(p,b,keys);
    return 1;
}
static void cafa_text(uint32_t x,uint32_t y,uint32_t w,const uint8_t *p,uint32_t n)
{
    char s[17]; memcpy(s,p,n); s[n]=0;
    draw_text_line(x,y,w,&AF_S,s,T_TEXT,T_BG,0);
}
static void wire6_text_copy(char *dst,const uint8_t *src,uint32_t n)
{
    memcpy(dst,src,n);while(n && dst[n-1]==' ')n--;dst[n]=0;
}
static int wire6_draw(void)
{
    const uint8_t *p=wire6.view;uint32_t k;char title[17],sub[13],b[28];
    if(wire6.phase!=2)return 0;
    if(p[0]==CAFA_NATIVE_KIND) {
        int full=wire6_dirty==2 || (wire6_region_dirty&CAFA_PAINT_FULL) ||
                 wire6_drawn_len<CAFA_NATIVE_HEADER ||
                 wire6_drawn_view[0]!=CAFA_NATIVE_KIND ||
                 wire6_drawn_screen!=p[6] || wire6_drawn_variant!=p[7] ||
                 wire6_drawn_palette!=ux.gen || wire6_drawn_prefs!=ui_prefs;
        if(full || wire6_dirty || wire6_region_dirty) {
            if(!((p[6]>=57u && p[6]<=62u) || p[6]==70u || p[6]==71u))
                cafa_device_menu.active=cafa_device_menu.requested=wire6_device_owned=0;
            cafa_native_confirm_backdrop=(uint8_t)(full || wire6_backdrop_dirty);
            if(full || wire6_region_dirty || wire6_device_owned || wire6_backdrop_dirty ||
               (p[1]&8u))cafa_native_profile_draw_regions(p,wire6.view_len,full,wire6_region_dirty);
            wire6_drawn_len=wire6.view_len;wire6_drawn_view[0]=CAFA_NATIVE_KIND;
            wire6_drawn_screen=p[6];wire6_drawn_variant=p[7];
            wire6_drawn_palette=ux.gen;wire6_drawn_prefs=ui_prefs;
        }
        wire6_dirty=wire6_backdrop_dirty=wire6_region_dirty=0;
        wire6.drawn_context=wire6_u28(p+2);
        return 1;
    }
    if(!wire6_dirty)return 1;
    wire6_dirty=wire6_region_dirty=0;
    cafa_native_device_reset();wire6_device_owned=0;
    if(wire6_drawn_len==wire6.view_len && !memcmp(wire6_drawn_view,p,wire6.view_len)) {
        wire6.drawn_context=wire6_u28(p+2);return 1;
    }
    lcd_fill(0,0,240,240,T_BG);
    wire6_text_copy(title,p+6,16);wire6_text_copy(sub,p+22,12);
    draw_text_box(8,7,224,&AF_M,title,T_THEME,0);
    draw_text_box(8,35,224,&AF_S,sub,T_TEXT,0);
    if(p[1]&1u)draw_text_box(8,56,108,&AF_S,"PENDING",T_THEME,0);
    if(p[1]&2u)draw_text_box(120,56,112,&AF_S,"ERROR",T_ACCENT,2);
    if(p[0]==1u) {
        uint32_t bi=wire6_u14(p+34)+1u,bc=wire6_u14(p+36);
        fmt_int(b,(int32_t)bi);str_cpy(b+str_len(b)," / ",sizeof b-str_len(b));
        fmt_int(b+str_len(b),(int32_t)bc);
        draw_text_box(8,75,224,&AF_S,b,T_DIM,0);
        for(k=0;k<4;k++) {
            const uint8_t *q=p+42+24*k;char label[9],value[13];uint32_t x=4u+59u*k;
            wire6_text_copy(label,q,8);wire6_text_copy(value,q+8,12);
            lcd_fill(x,111,56,91,T_SURF);
            draw_text_box(x+3,121,50,&AF_S,label,T_MID,0);
            draw_text_box(x+3,154,50,&AF_M,value,q[23]&1u ? T_THEME:T_DIM,0);
            if(q[22])lcd_fill(x+3,193,((uint32_t)wire6_u14(q+20)*50u)/16383u,3,T_THEME);
        }
        bi=wire6_u14(p+38)+1u;bc=wire6_u14(p+40);
        fmt_int(b,(int32_t)bi);str_cpy(b+str_len(b)," / ",sizeof b-str_len(b));
        fmt_int(b+str_len(b),(int32_t)bc);
        draw_text_box(8,213,224,&AF_S,b,T_DIM,0);
    } else if(p[0]==4u) {
        uint32_t selected=wire6_u14(p+34)-wire6_u14(p+38),visible=p[40];
        char name[6],level[13],pan[13];
        for(k=0;k<8u;k++) {
            const uint8_t *q=p+42u+8u*k;
            uint32_t y=73u+17u*k,selected_row=k==selected;
            lcd_fill(8,y,224,16,selected_row?T_SURF:T_BG);
            if(k>=visible)continue;
            wire6_text_copy(name,q,5);
            draw_text_box(11,y,73,&AF_S,name,selected_row?T_THEME:T_TEXT,0);
            if(q[7]&8u)lcd_fill(91,y+11,((uint32_t)wire6_u14(q+5)*67u)/16383u,3,T_THEME);
            draw_text_box(162,y,35,&AF_S,q[7]&1u?"M":"",T_ACCENT,0);
            draw_text_box(197,y,35,&AF_S,q[7]&2u?"S":"",T_THEME,0);
            if(!(q[7]&4u))lcd_fill(87,y+2,3,11,T_DIM);
        }
        wire6_text_copy(level,p+106,12);wire6_text_copy(pan,p+118,12);
        if(p[132]&1u)draw_text_box(8,211,106,&AF_S,level,T_TEXT,0);
        if(p[132]&2u)draw_text_box(122,211,106,&AF_S,pan,T_TEXT,0);
    } else if(p[0]==2u) {
        if(wire6_has(&wire6,2u)) {
            uint32_t tempo=wire6_u14(p+35),len;
            draw_text_box(8,105,224,&AF_M,p[34]&2u ? "RECORDING":p[34]&1u ? "PLAYING":"STOPPED",T_THEME,0);
            fmt_int(b,(int32_t)(tempo/10u));len=str_len(b);
            b[len++]='.';b[len++]=(char)('0'+tempo%10u);b[len]=0;
            str_cpy(b+len," BPM",sizeof b-len);
            draw_text_box(8,141,224,&AF_S,b,T_TEXT,0);
        } else draw_text_box(8,105,224,&AF_M,"INSTRUMENT",T_THEME,0);
    } else {
        static const char *const results[]={"COMPLETED","PENDING","UNSUPPORTED","INVALID",
            "STALE","BUSY","FAILED","CANCELLED"};
        draw_text_box(8,112,224,&AF_M,results[p[34]],p[34]==0u ? T_THEME:T_ACCENT,0);
    }
    draw_text_box(8,225,224,&AF_S,"OPEN COMPANION",T_DIM,0);
    wire6_drawn_len=wire6.view_len;memcpy(wire6_drawn_view,p,wire6.view_len);
    wire6.drawn_context=wire6_u28(p+2);
    return 1;
}
static __attribute__((noinline)) int cafa_draw(void)
{
#if FELUCCA_CONTROLLER
    static uint8_t countdown_drawn;
    if(ui.uboot) {
        if(countdown_drawn!=ui.uboot) {
            char seconds[8];
            fmt_int(seconds,(int32_t)ui.uboot);
            lcd_fill(0,0,240,240,T_BG);
            draw_text_box(8,83,224,&AF_M,"UPDATE MODE",T_THEME,0);
            draw_text_box(8,119,224,&AF_M,seconds,T_TEXT,0);
            countdown_drawn=ui.uboot;
        }
        return 1;
    }
    if(countdown_drawn) {
        countdown_drawn=0;
        wire6_dirty=cafa_dirty=2;
        cafa_idle_dirty=1;
        ui.force=1;
    }
#endif
    if(wire6.phase==2)return wire6_draw();
#if FELUCCA_CONTROLLER
    if(cafa_idle_menu) {
        if(cafa_idle_dirty || ui.force) {
            cafa_native_device_draw_local();
            cafa_idle_dirty=0;ui.force=0;
        }
        return 1;
    }
#endif
    const uint8_t *p=cafa.view, *old=cafa_drawn;
    uint32_t k, first=cafa_u14(p+5), head=cafa_u14(p+7), mask=cafa_u14(p+11)|((uint32_t)p[13]<<14);
    uint32_t old_first=cafa_u14(old+5), old_head=cafa_u14(old+7), old_mask=cafa_u14(old+11)|((uint32_t)old[13]<<14);
    int full=cafa_dirty==2;
    char b[24];
    static const char *const pages[]={"HOME","SEQ","SOUND","ENV","FX","MIX"};
    if (cafa.phase!=2) {
#if FELUCCA_CONTROLLER
        static uint8_t idle_drawn;
        if(!idle_drawn || ui.force) {
            lcd_fill(0,0,240,240,T_BG);
            draw_text_box(8,62,224,&AF_M,"WINDJAMMER CONTROLLER",T_THEME,0);
            draw_text_box(8,108,224,&AF_S,"OPEN COMPATIBLE APP",T_TEXT,0);
            draw_text_box(8,135,224,&AF_S,"USB MIDI + AUDIO READY",T_DIM,0);
            idle_drawn=1;ui.force=0;
        }
        return 1;
#else
        return 0;
#endif
    }
    if (cafa.version>=4 || (cafa.version==2 && p[1]==3)) {
        cafa_native_draw(p,old,full || !cafa_native_shown || p[0]!=old[0] || p[1]!=old[1] ||
                (cafa.version==5 && memcmp(p+152,old+152,2)),cafa_dirty!=0);
        cafa_native_shown=1; cafa_dirty=0;
        memcpy(cafa_drawn,p,cafa_state_bytes(cafa.version));
        return 1;
    }
    if (cafa_native_shown) { full=1; cafa_dirty=2; cafa_native_shown=0; }
    if (!cafa_dirty) return 1;
    cafa_dirty=0;
    /* One full clear on takeover only. Dynamic fields are composed in the
     * existing canvas before their small region is transferred to the LCD. */
    if(full)lcd_fill(0,0,240,240,T_BG);
    if(full || p[0]!=old[0]) {
        b[0]='T'; b[1]='1'+p[0]; b[2]=0;
        draw_text_box(8,7,32,&AF_M,b,T_THEME,0);
    }
    if(full || p[1]!=old[1])draw_text_box(47,7,100,&AF_M,pages[p[1]],T_TEXT,0);
    if(full || memcmp(p+9,old+9,2)) {
        fmt_int(b,(int32_t)cafa_u14(p+9)); draw_text_box(175,7,60,&AF_M,b,T_THEME,2);
    }
    if(full || memcmp(p+20,old+20,16))cafa_text(8,39,224,p+20,16);
    if(full || memcmp(p+36,old+36,16))cafa_text(8,61,224,p+36,16);
    for (k=0;k<16;k++) {
        uint32_t x=8+(k%8)*28, y=92+(k/8)*25;
        uint16_t col=(mask>>k)&1u ? T_THEME : T_SURF;
        if(full || ((mask^old_mask)>>k&1u) || (head==first+k)!=(old_head==old_first+k)) {
            cv_begin(24,20,col);
            if(head==first+k)cv_rect(0,17,24,3,T_TEXT);
            cv_blit(x,y);
        }
    }
    if(full || memcmp(p+3,old+3,4)) {
        fmt_int(b,(int32_t)first+1); str_cpy(b+str_len(b)," / ",sizeof b-str_len(b));
        fmt_int(b+str_len(b),(int32_t)cafa_u14(p+3));
        draw_text_box(8,144,145,&AF_S,b,T_DIM,0);
    }
    if(full || ((p[2]^old[2])&1u))
        draw_text_box(155,144,80,&AF_S,(p[2]&1u)?"PLAY":"STOP",T_THEME,2);
    for (k=0;k<4;k++) {
        if(full || memcmp(p+52+8*k,old+52+8*k,8))cafa_text(4+k*59,173,56,p+52+8*k,8);
        if(full || memcmp(p+84+2*k,old+84+2*k,2)) {
            fmt_int(b,(int32_t)cafa_u14(p+84+2*k));
            draw_text_box(4+k*59,193,56,&AF_M,b,T_TEXT,0);
        }
    }
    if(full)draw_text_box(8,222,224,&AF_S,
#if FELUCCA_CONTROLLER
        "COMPANION  /  HOST AUDIO",
#else
        "CAFA  /  PHONE ENGINE",
#endif
        T_DIM,0);
    memcpy(cafa_drawn,p,cafa_state_bytes(cafa.version));
    return 1;
}
static int cafa_leds(void)
{
    if(wire6.phase==2)return wire6_leds();
    uint32_t k, c, buttons=cafa_u14(cafa.view+14), keys=0;
    uint8_t nl[FM1_NCOL]={0};
    if (cafa.phase!=2) {
#if FELUCCA_CONTROLLER
        for(c=0;c<FM1_NCOL;c++)fm1_led[c]=fm1_led_dim[c]=fm1_led_breath[c]=fm1_led_mid[c]=0;
        return 1;
#else
        return 0;
#endif
    }
    led_pos_init();
    for (k=0;k<4;k++) keys|=(uint32_t)cafa.view[16+k]<<(7*k);
    for (k=0;k<NB;k++) led_put(nl,panel.btn[k],(buttons>>k)&1u);
    for (k=0;k<27;k++) led_put(nl,LED_KEY0+k,(keys>>k)&1u);
    if (cafa.view[2]&1u) nl[LED_PLAY_GREEN>>3]|=1u<<(LED_PLAY_GREEN&7u);
    for (c=0;c<FM1_NCOL;c++) { fm1_led[c]=nl[c]; fm1_led_dim[c]=fm1_led_breath[c]=fm1_led_mid[c]=0; }
    return 1;
}
static int wire6_leds(void)
{
    uint8_t masks[4][FM1_NCOL]={{0}};
    uint32_t c,k,i;
    if(wire6.phase!=2)return 0;
    if(!wire6_led_dirty)return 1;
    wire6_led_dirty=0;
    if(wire6.view_len>=CAFA_NATIVE_HEADER && wire6.view[0]==CAFA_NATIVE_KIND) {
        static uint8_t positions_ready;
        const uint8_t *leds=cafa_native_section(wire6.view,wire6.view_len,CAFA_NATIVE_LEDS,0);
        if(!positions_ready){led_pos_init();positions_ready=1;}
        if(leds) {
            for(i=0;i<4;i++) {
                const uint8_t *q=leds+7u*i;
                uint32_t buttons=wire6_u14(q),keys=wire6_u28(q+3);
                for(k=0;k<14;k++)led_put(masks[i],panel.btn[k],(buttons>>k)&1u);
                for(k=0;k<27;k++)led_put(masks[i],LED_KEY0+k,(keys>>k)&1u);
                if(q[2]&1u)masks[i][LED_PLAY_GREEN>>3]|=1u<<(LED_PLAY_GREEN&7u);
            }
            fm1_led_dim_level(leds[28]);
        } else {
            if(wire6.view[8]<12u)led_put(masks[0],panel.btn[wire6.view[8]],1);
            if(wire6.view[32]&1u)masks[0][LED_PLAY_GREEN>>3]|=1u<<(LED_PLAY_GREEN&7u);
            if(wire6.view[32]&2u)led_put(masks[0],panel.btn[B_REC],1);
            fm1_led_dim_level(settings_leds==LEDS_DIM_LO);
        }
    }
    for(c=0;c<FM1_NCOL;c++) {
        fm1_led[c]=masks[0][c];fm1_led_dim[c]=masks[1][c];
        fm1_led_breath[c]=masks[2][c];fm1_led_mid[c]=masks[3][c];
    }
    return 1;
}
