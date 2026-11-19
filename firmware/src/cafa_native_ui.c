/* SPDX-License-Identifier: GPL-3.0-only
 * First native CAFA page: the native ENV descriptor, cards and ADSR graph.
 * This is a read-only projection, NOT a second project or permission to let the
 * standalone input handler edit phone state. The phone still owns ADSR and DSP.
 * A bounded temporary track is on the main stack, not in audio RAM.
 * No standalone track/parameter/step/storage data is written. Audio remains in
 * cafa_audio_block, which does not consume tsel or the UI's borrowed fields. */
/* Keep new caches outside the compiler's merged-global groups: otherwise their
 * insertion moves unrelated hot arrays behind large offsets and regresses ISR
 * and STRUM code generation. This section is still ordinary zeroed BSS. */
static __typeof__(ui) cafa_native_cache __attribute__((section(".bss.cafa_ui")));
/* The host look is borrowed for a paint only. Physical settings and the
 * controller's palette remain untouched when the app closes or takes a
 * device-owned menu. Cached derived colours keep unchanged frames cheap. */
static __typeof__(ux) cafa_host_ux __attribute__((section(".bss.cafa_ui")));
static __typeof__(uf) cafa_host_uf __attribute__((section(".bss.cafa_ui")));
static uint8_t cafa_host_pref[35] __attribute__((section(".bss.cafa_ui")));
static uint8_t cafa_host_custom[10u + 2u * UI_NTRK] __attribute__((section(".bss.cafa_ui")));
static uint32_t cafa_host_local_gen, cafa_host_gen;
static uint8_t cafa_host_pref_len;
static int cafa_native_host_style(const uint8_t *view, uint32_t len)
{
    uint32_t n, i;
    const uint8_t *p = cafa_native_section(view, len, CAFA_NATIVE_PREFS, &n);
    const uint8_t *local_custom;
    if (!p || view[6] >= 57u && view[6] <= 62u || view[6] == 70u || view[6] == 71u) return 0;
    if (n != cafa_host_pref_len || cafa_host_local_gen != ux.gen || memcmp(p, cafa_host_pref, n)) {
        __typeof__(ux) local_ux = ux;
        __typeof__(uf) local_uf = uf;
        local_custom = ux_cpal;
        if (p[1] & 4u) for (i = 0; i < sizeof cafa_host_custom / 2u; i++) {
            uint32_t rgb = p[2u + 3u * i] | (uint32_t)p[3u + 3u * i] << 7 |
                           (uint32_t)p[4u + 3u * i] << 14;
            cafa_host_custom[2u * i] = (uint8_t)rgb;
            cafa_host_custom[2u * i + 1u] = (uint8_t)(rgb >> 8);
        }
        ux_cpal = (p[1] & 4u) ? cafa_host_custom : 0;
        palette_set(p[0]);
        cafa_host_ux = ux; cafa_host_uf = uf;
        cafa_host_ux.gen = 0x40000000u + ++cafa_host_gen;
        ux = local_ux; uf = local_uf; ux_cpal = local_custom;
        memcpy(cafa_host_pref, p, n);
        cafa_host_pref_len = (uint8_t)n;
        cafa_host_local_gen = ux.gen;
    }
    ux = cafa_host_ux; uf = cafa_host_uf;
    ui_prefs = (uint8_t)((ui_prefs & ~(PREF_LARGE | PREF_ANIM_OFF)) |
                         ((p[1] & 1u) ? PREF_LARGE : 0u) |
                         ((p[1] & 2u) ? PREF_ANIM_OFF : 0u));
    return 1;
}
/* Stable wire IDs are resolved explicitly; PAGES[] may be reordered locally. */
static const uint8_t cafa_native_page_desc[47] = {
    [CAFA_SCREEN_ENV]=0u,
    [CAFA_SCREEN_ENV2]=1u,
    [CAFA_SCREEN_LFO]=2u,
    [CAFA_SCREEN_LFO2]=3u,
    [CAFA_SCREEN_MOD]=4u,
    [CAFA_SCREEN_FX]=5u,
    [CAFA_SCREEN_FX_DELAY]=6u,
    [CAFA_SCREEN_FX_REVERB]=7u,
    [CAFA_SCREEN_DRIVE]=8u,
    [CAFA_SCREEN_INSERT]=9u,
    [CAFA_SCREEN_SLICER]=10u,
    [CAFA_SCREEN_DUCK]=11u,
    [CAFA_SCREEN_SCALE]=12u,
    [CAFA_SCREEN_CHORD]=13u,
    [CAFA_SCREEN_FOLLOW]=14u,
    [CAFA_SCREEN_EDIT1]=15u,
    [CAFA_SCREEN_EDIT2]=16u,
    [CAFA_SCREEN_OPERATOR]=17u,
    [CAFA_SCREEN_ALL_OPS]=18u,
    [CAFA_SCREEN_LANES]=19u,
    [CAFA_SCREEN_SOUND]=20u,
    [CAFA_SCREEN_LANE_MIX]=21u,
    [CAFA_SCREEN_SLICES]=22u,
    [CAFA_SCREEN_SAMPLES]=23u,
    [CAFA_SCREEN_OP1_ENV]=24u,
    [CAFA_SCREEN_OP2_ENV]=25u,
    [CAFA_SCREEN_OP3_ENV]=26u,
    [CAFA_SCREEN_OP4_ENV]=27u,
    [CAFA_SCREEN_OP_LEVEL]=28u,
    [CAFA_SCREEN_VOICE]=29u,
    [CAFA_SCREEN_VOICE2]=30u,
    [CAFA_SCREEN_MIXER]=31u,
    [CAFA_SCREEN_DELAY]=32u,
    [CAFA_SCREEN_REVERB]=33u,
    [CAFA_SCREEN_CHORUS]=34u,
    [CAFA_SCREEN_GLOBAL]=35u,
    [CAFA_SCREEN_PRESETS]=36u,
    [CAFA_SCREEN_USER]=37u,
    [CAFA_SCREEN_PATTERNS]=38u,
    [CAFA_SCREEN_PROJECT]=39u,
    [CAFA_SCREEN_ARP]=40u,
    [CAFA_SCREEN_ARP2]=41u,
    [CAFA_SCREEN_NOTES]=42u,
    [CAFA_SCREEN_FEEL]=43u,
    [CAFA_SCREEN_PATTERN]=44u,
    [CAFA_SCREEN_AUTOMATION]=45u,
    [CAFA_SCREEN_SONG]=46u,
};
/* Loom wire values are already display units. Never pass them through Skiff's
 * parameter descriptors: percentages, offsets and gate/rate scales differ. */
static void cafa_loom_value(const uint8_t *p,uint32_t k,char *label,char *value,const char **unit)
{
    int32_t v=(int32_t)cafa_u14(p+84+2*k);
    uint32_t n=8;
    memcpy(label,p+52+8*k,8); label[8]=0;
    while(n && label[n-1]==' ')label[--n]=0;
    *unit="";
    if (str_eq(label,"UNAVAIL")) { str_cpy(value,"--",12);return; }
    if (str_eq(label,"PITCH") && v<=127) { note_name(value,(uint32_t)v);return; }
    if (str_eq(label,"LEN x100") || str_eq(label,"POS x100")) {
        str_cpy(label,str_eq(label,"LEN x100") ? "LENGTH" : "START",9);
        fmt_int(value,v/100);n=str_len(value);
        value[n]='.';value[n+1]=(char)('0'+v/10%10);value[n+2]=(char)('0'+v%10);value[n+3]=0;
        *unit="st";return;
    }
    if (str_eq(label,"OCT+3")) { str_cpy(label,"OCT",9);v-=3; }
    else if (str_eq(label,"INV+2")) { str_cpy(label,"INV",9);v-=2; }
    else if (str_eq(label,"SWING+23")) { str_cpy(label,"SWING",9);v-=23;*unit="%"; }
    else if (str_eq(label,"GATE x10")) {
        str_cpy(label,"GATE",9);fmt_int(value,v/10);n=str_len(value);
        value[n]='.';value[n+1]=(char)('0'+v%10);value[n+2]=0;*unit="st";return;
    } else if (str_eq(label,"RATE x2")) {
        str_cpy(label,"RATE",9);fmt_int(value,v/2);n=str_len(value);
        value[n]='.';value[n+1]=(char)('0'+(v%2)*5);value[n+2]=0;*unit="x";return;
    } else if (n && label[n-1]=='%') { label[--n]=0;while(n && label[n-1]==' ')label[--n]=0;*unit="%"; }
    else if (p[1]==2 || p[1]==3) *unit="%";
    if (str_eq(label,"MUTE") || str_eq(label,"LATCH") || str_eq(label,"SYNC") || str_eq(label,"ENABLED") || str_eq(label,"ENABLE") ||
        str_eq(label,"HIT") || str_eq(label,"ACC") || str_eq(label,"SKIP")) {
        str_cpy(value,v ? "ON" : "OFF",12);return;
    }
    fmt_int(value,v);
}
static void cafa_dynamic_value(const uint8_t *p,uint32_t k,char *label,char *value)
{
    uint32_t n=8;
    memcpy(label,p+52+8*k,8);label[8]=0;
    while(n && label[n-1]==' ')label[--n]=0;
    n=12;memcpy(value,p+100+12*k,12);value[12]=0;
    while(n && value[n-1]==' ')value[--n]=0;
}
static void cafa_loom_columns(const uint8_t *p)
{
    uint32_t k;
    memset(card_mot_mask,0,sizeof card_mot_mask);
    for(k=0;k<4;k++) {
        char label[9],value[13];const char *unit="";
        int dynamic=cafa.version==5 && p[1]==CAFA_DYNAMIC_PAGE;
        if(dynamic)cafa_dynamic_value(p,k,label,value);
        else cafa_loom_value(p,k,label,value,&unit);
        draw_column(k,label,value,unit,(dynamic ? !p[148+k] : str_eq(label,"UNITY")) ? VR_DIM : VR_LIVE,-1,ICON_NONE);
    }
}
/* A Loom ADSR preview: cubic time controls (2/3/3 seconds), exponential
 * decay/release, independent of the local envelope-time mapping. Sustain dwell is illustrative,
 * because a note-off timestamp is absent from the snapshot. Integer-only graph. */
static void cafa_loom_graph(const uint8_t *p)
{
    uint32_t k,mask=cafa_u14(p+11)|((uint32_t)p[13]<<14),first=cafa_u14(p+5),head=cafa_u14(p+7);
    int32_t h=(int32_t)graph_h(),bottom=h-8;
    cv_begin(240,graph_h(),T_BG);
    if(p[1]==3) {
        uint32_t a=cafa_u14(p+84),d=cafa_u14(p+86),sus=cafa_u14(p+88),r=cafa_u14(p+90);
        /* Use 9 decay and 12 release time constants to reach near silence. */
        uint32_t at=2*a*a*a/1000+1,dt=27*d*d*d/1000+1,rt=36*r*r*r/1000+1;
        uint32_t total=at+dt+rt+1000,ax=12+216*at/total,dx=ax+216*dt/total,sx=dx+216*1000/total;
        int32_t top=6,sy=bottom-(bottom-top)*(int32_t)sus/100,px=(int32_t)ax,py=top;
        cv_line_t(12,bottom,ax,top,T_THEME,2);
        for(k=ax+1;k<=dx;k++) {
            uint32_t t=(k-ax)*1024/(dx-ax),q=1024+t*9/8,j;
            uint32_t e=32768;for(j=0;j<8;j++)e=e*1024/q;
            int32_t y=sy-(sy-top)*(int32_t)e/32768;
            cv_line_t(px,py,k,y,T_THEME,2);px=k;py=y;
        }
        cv_line_t(px,py,sx,sy,T_THEME,2);px=sx;py=sy;
        cv_text_r(232,2,&AF_S,"ENV PREVIEW",T_MID,T_BG);
        cv_line(sx,sy,sx,bottom,T_LINE);
        for(k=sx+1;k<=228;k++) {
            uint32_t t=(k-sx)*1024/(228-sx),q=1024+t*12/8,j;
            uint32_t e=32768;for(j=0;j<8;j++)e=e*1024/q;
            int32_t y=bottom-(bottom-sy)*(int32_t)e/32768;
            cv_line_t(px,py,k,y,T_THEME,2);px=k;py=y;
        }
    } else {
        /* The protocol contains occupancy, not note pitches. Two rows of eight
         * steps are truthful on every page; no invented piano-roll notes. */
        int32_t cell=(h-24)/2;if(cell<4)cell=4;
        for(k=0;k<16;k++) {
            int32_t x=8+(k%8)*28,y=3+(k/8)*(cell+3);
            cv_rrect(x,y,24,cell,3,(mask>>k)&1u ? T_THEME:T_SURF,T_BG);
            if(head==first+k)cv_rect(x,y+cell-2,24,2,T_TEXT);
        }
        { char b[24];
          int dynamic=cafa.version==5 && p[1]==CAFA_DYNAMIC_PAGE;
          str_cpy(b,dynamic ? "BANK " : "",sizeof b);
          fmt_int(b+str_len(b),dynamic ? (int32_t)cafa_u14(p+92)+1 : (int32_t)first+1);
          str_cpy(b+str_len(b)," / ",sizeof b-str_len(b));
          fmt_int(b+str_len(b),(int32_t)cafa_u14(p+(dynamic ? 94 : 3)));
          cv_text(8,h-15,&AF_S,b,T_MID);
          cv_text_r(232,h-15,&AF_S,(p[2]&4)?"MUTED":"STEPS",T_MID,T_BG); }
    }
    cv_blit(0,graph_y());
}
static __attribute__((noinline)) void cafa_native_draw(const uint8_t *p,const uint8_t *old,int full,int changed)
{
    if (!full && !changed && !cafa_native_cache.hot_t) return;
    static const uint8_t id[4]={P_ATK,P_DEC,P_SUS,P_REL};
    track_t view;
    track_t *saved_tsel=tsel;
    __typeof__(ui) saved_ui=ui;
    uint8_t selected=song.sel, playing=song.playing, rec=song.rec, free_on=fr.on;
    int8_t octave=song.octave;
    int16_t bpm=song.g[G_BPM];
    uint32_t k, head=cafa_u14(p+7);
    memset(&view,0,sizeof view);
    if (full) memset(&cafa_native_cache,0,sizeof cafa_native_cache);
    ui=cafa_native_cache;
    ui.force=(uint8_t)full; ui.home=0; ui.page=0; /* PAGES[0]: actual ENV, not a copied descriptor */
    ui.frame++;
    for (k=0;k<4;k++) {
        view.p[id[k]]=(int16_t)cafa_u14(p+84+2*k);
        if (!full && (memcmp(p+84+2*k,old+84+2*k,2) || (cafa.version==5 && p[1]==CAFA_DYNAMIC_PAGE && memcmp(p+100+12*k,old+100+12*k,12)))) { ui.hot_col=(uint8_t)k; ui.hot_t=40; }
    }
    view.p[P_SLEN]=16; /* never index the phone's 1024-step timeline in a 64-step native track */
    view.seq_idx=(uint16_t)(head==16383u ? 0 : head); /* ENV header only; no step renderer is entered */
    tsel=&view; song.sel=p[0]; song.playing=(p[2]&1u) && head!=16383u;
    song.rec=(uint8_t)(((p[2]>>1)&1u)<<p[0]); song.octave=(int8_t)((p[2]>>3)-3);
    song.g[G_BPM]=(int16_t)cafa_u14(p+9); fr.on=0;
    cafa_native_view=p; cafa_native_v4=cafa.version>=4;
    if (ui.hot_t) ui.hot_t--;
    if (full) draw_frame();
    draw_head();
    if (cafa_native_v4) { cafa_loom_columns(p); cafa_loom_graph(p); }
    else { draw_columns(); draw_graph(); }
    draw_foot();
    ui.force=0; cafa_native_cache=ui;
    cafa_native_view=0; cafa_native_v4=0; ui=saved_ui; tsel=saved_tsel;
    song.sel=selected; song.playing=playing; song.rec=rec; song.octave=octave;
    song.g[G_BPM]=bpm; fr.on=free_on;
}

/* A published kind-7 view is borrowed only for this paint pass.  In particular,
 * the panel painters must never materialise an application track in TSEL. */
enum { CAFA_PAINT_HEAD=1u, CAFA_PAINT_CARD0=2u, CAFA_PAINT_GRAPH=32u,
       CAFA_PAINT_FOOT=64u, CAFA_PAINT_ALL=127u, CAFA_PAINT_FULL=128u };
static uint8_t cafa_native_profile_regions(const uint8_t *old,uint32_t old_len,
                                            const uint8_t *view,uint32_t len)
{
    uint8_t dirty=0;uint32_t k,screen=view[6];int hot_changed=0;
    int held_header=screen>=CAFA_SCREEN_FX_LAYER && screen<=CAFA_SCREEN_SHIFT;
    uint32_t old_chrome_n,new_chrome_n;
    const uint8_t *old_chrome=cafa_native_section(old,old_len,CAFA_NATIVE_CHROME,&old_chrome_n);
    const uint8_t *new_chrome=cafa_native_section(view,len,CAFA_NATIVE_CHROME,&new_chrome_n);
    if(old_len<CAFA_NATIVE_HEADER || old[0]!=CAFA_NATIVE_KIND ||
       view[1]!=old[1] || memcmp(view+6,old+6,7u))return CAFA_PAINT_FULL;
    if((old_chrome==0)!=(new_chrome==0) || old_chrome_n!=new_chrome_n)return CAFA_PAINT_FULL;
    if(new_chrome) {
        if(memcmp(new_chrome,old_chrome,29u))dirty|=CAFA_PAINT_HEAD;
        if(memcmp(new_chrome+29u,old_chrome+29u,24u))dirty|=CAFA_PAINT_FOOT;
        if(memcmp(new_chrome+53u,old_chrome+53u,2u))dirty|=0x1eu;
    }
    {
        uint32_t old_n, new_n;
        const uint8_t *old_pref = cafa_native_section(old, old_len, CAFA_NATIVE_PREFS, &old_n);
        const uint8_t *new_pref = cafa_native_section(view, len, CAFA_NATIVE_PREFS, &new_n);
        if (old_n != new_n || (old_pref == 0) != (new_pref == 0) ||
            (new_pref && memcmp(old_pref, new_pref, new_n))) return CAFA_PAINT_FULL;
    }
    /* LED planes are applied by the matrix scanner, independently of LCD paint. */
    if(cafa_native_section(view,len,CAFA_NATIVE_LEDS,0))len-=32u;
    if(cafa_native_section(old,old_len,CAFA_NATIVE_LEDS,0))old_len-=32u;
    if(new_chrome) {
        /* Sections supplied by another app can follow CHROME. Preserve those
         * graph changes while excluding presentation-only bytes from the diff. */
        uint32_t old_end=(uint32_t)(old_chrome-old)+old_chrome_n;
        uint32_t new_end=(uint32_t)(new_chrome-view)+new_chrome_n;
        if(old_len-old_end!=len-new_end ||
           memcmp(old+old_end,view+new_end,len-new_end))dirty|=CAFA_PAINT_GRAPH;
        len=(uint32_t)(new_chrome-view-3u);
        old_len=(uint32_t)(old_chrome-old-3u);
    }
    if(memcmp(view+13,old+13,4u) || memcmp(view+22,old+22,2u) ||
       memcmp(view+30,old+30,27u))dirty|=CAFA_PAINT_HEAD;
    for(k=0;k<4u;k++)if(memcmp(view+112u+36u*k,old+112u+36u*k,36u)) {
        dirty|=(uint8_t)(CAFA_PAINT_CARD0<<k);
        if(view[112u+36u*k+33u]!=old[112u+36u*k+33u])
            hot_changed=1; /* The first hot card owns the shared highlight. */
    }
    if(hot_changed)dirty|=0x1eu|CAFA_PAINT_GRAPH;
    if(memcmp(view+13,old+13,4u))dirty|=0x1eu; /* Track colour on all cards. */
    if(new_chrome && memcmp(view+13,old+13,2u))dirty|=CAFA_PAINT_FOOT;
    if(memcmp(view+57,old+57,55u))dirty|=CAFA_PAINT_FOOT;
    if(len!=old_len || memcmp(view+17,old+17,5u) ||
       memcmp(view+24,old+24,6u) || memcmp(view+13,old+13,4u) ||
       (screen!=0u && screen!=1u && screen!=2u && screen!=3u &&
        ((!held_header && memcmp(view+22,old+22,2u)) || memcmp(view+30,old+30,3u))) ||
       (len==old_len && memcmp(view+CAFA_NATIVE_HEADER,old+CAFA_NATIVE_HEADER,
                               len-CAFA_NATIVE_HEADER)))dirty|=CAFA_PAINT_GRAPH;
    if((screen==12u || screen==13u || screen==14u || screen==17u || screen==18u || screen==28u ||
        ((screen==15u || screen==16u) && view[7]==CAFA_NATIVE_VARIANT_FM_OPERATOR)) &&
       memcmp(view+57,old+57,24u))dirty|=CAFA_PAINT_GRAPH;
    if(screen==5u && (dirty&0x1eu))dirty|=CAFA_PAINT_GRAPH;
    /* A beat cursor in the held layers and SHIFT affects only the header.
     * Other overlay changes may cross the ordinary region boundaries. */
    if(screen>=49u && dirty && !(held_header && dirty==CAFA_PAINT_HEAD))dirty=CAFA_PAINT_ALL;
    return dirty;
}
static void cafa_native_profile_columns_mask(uint8_t mask)
{
    const uint8_t *v=cafa_native_profile_view;
    const uint8_t *chrome=cafa_nv_section(CAFA_NATIVE_CHROME,0);
    uint32_t k;
    static const uint8_t icon[] = {ICON_NONE,ICON_PITCH,ICON_STEPS,ICON_WAVE,ICON_ENV,ICON_LFO_WAVE,
                                   ICON_MOD,ICON_MIX,ICON_SAVE,ICON_X_PLAY,ICON_X_REC,ICON_X_LOCK,ICON_CHIP};
    for(k=0;k<4u;k++) {
        if(!(mask&(CAFA_PAINT_CARD0<<k)))continue;
        const uint8_t *p=v+112u+36u*k;
        char label[13],value[15],unit[5];
        uint32_t kind=p[32],flags=p[33],value_pos=cafa_native_u14(p+30);
        uint32_t role=chrome ? (chrome[53u+k/2u]>>(3u*(k%2u)))&3u : 0u;
        if (v[6] == CAFA_SCREEN_ARP_LAYER && (flags & 8u) && value_pos != 8191u) role = 1u;
        cafa_native_text(label,p,12);cafa_native_text(value,p+12,14);cafa_native_text(unit,p+26,4);
        draw_column(k,label,value,unit,!kind || !(flags&1u) ? VR_DIM :
                    role==1u ? VR_ACCENT : role==2u ? VR_MID : role==3u ? VR_THEME : VR_LIVE,
                    kind==1u ? (int32_t)value_pos*1000/16383 : -1,
                    icon[p[34]]);
    }
}
static void cafa_native_profile_columns(void) { cafa_native_profile_columns_mask(0x1eu); }
static void cafa_native_profile_foot(const uint8_t *v)
{
    char sub[25], hint[32];
    const uint8_t *chrome=cafa_nv_section(CAFA_NATIVE_CHROME,0);
    cafa_native_text(sub,v+57,24);cafa_native_text(hint,v+81,31);
    if(chrome) {
        uint32_t page=cafa_native_u14(v+9), count=cafa_native_u14(v+11);
        sub[0]=0;
        if(v[6]<=CAFA_SCREEN_SONG && count>1u)fmt_kn(sub,(int32_t)page+1,(int32_t)count);
        if(!hint[0]) {
            char engine[9],sound[17];
            hint[0]='T';
            fmt_int(hint+1,(int32_t)cafa_native_u14(v+13)+1);
            str_cpy(hint+str_len(hint)," ",sizeof hint-str_len(hint));
            cafa_native_text(engine,chrome+29u,8u);
            cafa_native_text(sound,chrome+37u,16u);
            str_cpy(hint+str_len(hint),engine,sizeof hint-str_len(hint));
            if(sound[0]) {
                str_cpy(hint+str_len(hint)," ",sizeof hint-str_len(hint));
                str_cpy(hint+str_len(hint),sound,sizeof hint-str_len(hint));
            }
        }
    }
    cv_begin(240,foot_h(),T_BG);
    if(ux.neon)ne_rule(0,1);
    if(ux.quark)qk_rule(0);
    cv_free_text(8,foot_line_y(),&AF_S,hint,T_MID,T_BG,150);
    cv_free_text(164,foot_line_y(),&AF_S,sub,T_THEME,T_BG,68);
    cv_blit(0,foot_y());
}
static __attribute__((noinline)) void cafa_native_profile_draw_regions(const uint8_t *view,uint32_t len,
                                                                         int full,uint8_t regions)
{
    __typeof__(ui) saved=ui;
    __typeof__(ux) saved_ux=ux;
    __typeof__(uf) saved_uf=uf;
    const uint8_t *saved_custom=ux_cpal;
    uint8_t saved_prefs=ui_prefs;
    uint32_t id=view[6],k;
    cafa_native_profile_view=view;cafa_native_profile_len=len;
    cafa_native_host_style(view,len);
    ui.force=1;ui.frame=fm1_ms/34u;
    ui.page=id<47u ? cafa_native_page_desc[id] : cafa_native_page_desc[CAFA_SCREEN_ENV];
    ui.home=(uint8_t)(id==47u || id==48u);
    ui.home_scope=(uint8_t)(id==48u);
    ui.bank=(uint8_t)(cafa_native_u14(view+17)/16u);
    ui.cursor=(uint8_t)(cafa_native_u14(view+20)%NSTEP);
    ui.hot_t=0;ui.msg_t=0;ui.bpm_t=0;ui.layer=ui.shift=ui.menu=0;
    for(k=0;k<4u;k++)if(view[112u+36u*k+33u]&2u){ui.hot_t=1;ui.hot_col=(uint8_t)k;break;}
    if(full)lcd_fill(0,0,240,240,T_BG);
    if(!full && regions==CAFA_PAINT_HEAD && id>=CAFA_SCREEN_FX_LAYER && id<=CAFA_SCREEN_SHIFT) {
        draw_head();
    } else if(!cafa_native_overlay_draw(full)) {
        if(full)draw_frame();
        if(full || (regions&CAFA_PAINT_HEAD))draw_head();
        if(full)cafa_native_profile_columns();
        else cafa_native_profile_columns_mask(regions);
        if(full || (regions&CAFA_PAINT_GRAPH)) {
            cafa_native_profile_graph(view,len);
        }
        if(full || (regions&CAFA_PAINT_FOOT))cafa_native_profile_foot(view);
    }
    ui=saved;
    ux=saved_ux;uf=saved_uf;ux_cpal=saved_custom;ui_prefs=saved_prefs;
    cafa_native_profile_view=0;cafa_native_profile_len=0;
}
static void cafa_native_profile_draw(const uint8_t *view,uint32_t len,int full)
{ cafa_native_profile_draw_regions(view,len,full,CAFA_PAINT_ALL); }
static void cafa_native_profile_reset(void)
{
    cafa_native_profile_view=0;cafa_native_profile_len=0;
    cafa_host_pref_len=0;
    cafa_native_device_reset();
}
