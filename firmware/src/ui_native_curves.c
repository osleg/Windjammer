/* SPDX-License-Identifier: GPL-3.0-only
 * Native Windjammer curve painters. Local and app-provided views share these bodies;
 * only curve acquisition differs. Included after ui_graph.c geometry helpers. */

#if FELUCCA_CAFA
static int cafa_curve_remote(void) { return cafa_native_profile_view != 0; }
static uint32_t cafa_curve_count(void)
{
    uint32_t len;const uint8_t *p=cafa_nv_section(3,&len);return p?cafa_native_u14(p):0u;
}
/* Piecewise-linear sampling of app display points, bounded by the validated
 * section. No local parameter mapping is applied to the application's curve. */
static int32_t cafa_curve_at(int32_t x,int32_t width)
{
    uint32_t len,j,n;const uint8_t *p=cafa_nv_section(3,&len);
    int32_t at=width>0?clamp(x,0,width)*16383/width:0;
    if(!p || !(n=cafa_native_u14(p)))return 0;
    p+=2;
    if(at<=cafa_native_u14(p))return cafa_native_u14(p+2);
    for(j=1;j<n;j++) {
        const uint8_t *q=p+4u*j;
        int32_t x1=cafa_native_u14(q),x0=cafa_native_u14(q-4);
        if(at<=x1){int32_t y0=cafa_native_u14(q-2),y1=cafa_native_u14(q+2);
            return x1>x0?y0+(y1-y0)*(at-x0)/(x1-x0):y1;}
    }
    return cafa_native_u14(p+4u*(n-1u)+2u);
}
static uint32_t cafa_curve_markers(int32_t *x,int32_t *y,uint32_t count,int32_t width)
{
    uint32_t len,j;const uint8_t *p=cafa_nv_section(4,&len);
    if(!p)return 0;
    if(count>p[0])count=p[0];
    for(j=0;j<count;j++){x[j]=cafa_native_u14(p+1u+13u*j)*width/16383;
        y[j]=cafa_native_u14(p+3u+13u*j)*1000/16383;}
    return count;
}
static void cafa_env_fill(int16_t *lv,int32_t w,int32_t *xa,int32_t *xd,int32_t *xr)
{
    int32_t x,mx[3]={0,0,w},my[3];
    cafa_curve_markers(mx,my,3,(int32_t)w);
    for(x=0;x<=w;x++)lv[x]=(int16_t)(cafa_curve_at(x,w)*1000/16383);
    *xa=mx[0];*xd=mx[1];*xr=mx[2];
}
#else
static int cafa_curve_remote(void) { return 0; }
#endif

static void cr_adsr(const track_t *t)
{
    int16_t *const lv = eg_lv;
    const page_t *pg = cur_page();
    uint8_t L[37];
    int32_t rows = (graph_ht - 6) / 9, i, pt[3], hc = ui.hot_t ? (int32_t)ui.hot_col : -1;
#if FELUCCA_CAFA
    if(cafa_curve_remote())cafa_env_fill(lv,216,&pt[0],&pt[1],&pt[2]);else
#endif
    env_fill4(t->p[pg->id[0]], t->p[pg->id[1]], t->p[pg->id[2]], t->p[pg->id[3]], lv, 216, &pt[0], &pt[1], &pt[2]);
    for (i = 0; i < 37; i++)
        L[i] = (uint8_t)(rows - (lv[i * 6] * rows + 500) / 1000);
    cr_dots(12, 3, 6, 9, L, 37, rows, T_DIM);
    cr_art(12, 3, 6, 9, L, 37, T_THEME);
    for (i = 0; i < 3; i++) {
        int32_t c = (pt[i] + 3) / 6;
        int hot = hc == i || (i > 0 && hc == i + 1);      /* (as graph_adsr: DECAY, SUSTAIN light the decay's end ..) */
        cr_skull(12 + c * 6 - 4, 3 + L[c > 36 ? 36 : c] * 9 - 4, hot ? CR_FLAME : T_TEXT, T_SURF, 0);
    }
}

static int32_t lfo_view_at(const track_t *t, int32_t x, int32_t w)
{
#if FELUCCA_CAFA
    if(cafa_curve_remote())return cafa_curve_at(x,w)*4-32768;
#endif
    uint32_t k = lfo_view_t0(t) + (uint32_t)x * LFO_WIN / (uint32_t)w, ft, inc = lfo_inc(t);   /* (32-bit: no 64-bit
                                                         * division here) */
    uint64_t ph = (uint64_t)(t->p[P_LTRIG] ? t->lfo_ph - t->lfo_t * inc : (uint32_t)t->p[P_LPHASE] << 25) +
                  (uint64_t)k * inc;                   /* (from its phase at the retrigger: PHASE's (TRIG NOTE: turning
                                                         * PHASE shows at once), or (#16) FREE's, back from where it
                                                         * runs now; RND's cycles counted from there) */
    int32_t v = t->p[P_LWAVE] == 4 ? (int32_t)(((uint32_t)(ph >> 32) * 2654435761u) >> 16) - 32768 :
                lfo_wave((track_t *)t, (uint32_t)ph);
    if (t->p[P_LPOL])
        v = (v + 32768) >> 1;
    ft = t->p[P_LFADE] ? TIME_MS_X10[t->p[P_LFADE] & 127] * (FS / CTL) / 10000u : 0u;   /* FADE, ticks */
    return ft && k < ft ? v * (int32_t)((k << 14) / ft) >> 14 : v;   /* (FADE is under 2^18 ticks) */
}

static int32_t lfo_view_now(const track_t *t, int32_t w)
{
#if FELUCCA_CAFA
    if(cafa_curve_remote()){int32_t x=0,y=0;cafa_curve_markers(&x,&y,1,w);return x;}
#endif
    return (int32_t)((t->lfo_t - lfo_view_t0(t)) * (uint32_t)w / LFO_WIN);
}

static void cr_lfo(const track_t *t)
{
    uint8_t L[37];
    int32_t rows = (graph_ht - 8) / 10, i, now = lfo_view_now(t, 210) / 6;
    if (rows > 10)
        rows = 10;
    for (i = 0; i < 37; i++)
        L[i] = (uint8_t)(rows / 2 - lfo_view_at(t, i * 6, 216) * (rows / 2) / 32768);
    cr_art(12, 4, 6, 10, L, 37, T_LINE);
    cr_snake(12, 4, 6, 10, L, (uint32_t)now + 1u, T_THEME, CR_EMBER);   /* (its head at most at column 35: in the frame) */
}

static void graph_adsr(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    int remote=cafa_curve_remote();
    int32_t a = remote?0:4 + t->p[pg->id[0]] * 50 / 127, d = remote?0:6 + t->p[pg->id[1]] * 50 / 127,
            r = remote?0:6 + t->p[pg->id[3]] * 60 / 127;
    int32_t top = 6 * graph_ht / 100, bot = 88 * graph_ht / 100, sus = remote?0:t->p[pg->id[2]] * 1000 / 127;   /* 0..1000 */
    int32_t x0 = 12, x1 = x0 + a, x3 = 226 - r, i, px, py;
    int32_t e = 32768;                                                  /* exp(-4.6 u), Q15 */
#if FELUCCA_CAFA
    if(remote && !cafa_curve_count()) {
        cv_rect(PANEL_X0,top,PANEL_W,1,T_LINE);cv_rect(PANEL_X0,(top+bot)/2,PANEL_W,1,T_LINE);
        cv_rect(PANEL_X0,bot,PANEL_W,1,T_LINE);return;
    }
#endif
    if (ux.crypt) {                                  /* (Skiff C4) CRYPT: its own */
        cr_adsr(t);
        return;
    }
#define EGY(lvl) (bot - (lvl) * (bot - top) / 1000)
    if (ux.neon || ux.quark) {                       /* (Skiff N4) NEON: the sun's bands under the curve; (Q4) QUARK: a
                                                      * Monte Carlo fill */
        int32_t xa, xd, xr;
#if FELUCCA_CAFA
        if(remote)cafa_env_fill(eg_lv,214,&xa,&xd,&xr);else
#endif
        env_fill4(t->p[pg->id[0]], t->p[pg->id[1]], t->p[pg->id[2]], t->p[pg->id[3]], eg_lv, 214, &xa, &xd, &xr);
        for (i = 0; i <= 214; i++)
            eg_lv[i] = (int16_t)EGY(eg_lv[i]);     /* (the rows) */
        if (ux.neon)
            ne_bands(x0, eg_lv, 215, top, bot);
        else
            for (i = 0; i <= 214; i++)
                qk_mc(x0 + i, eg_lv[i], bot);
    }
    cv_rect(PANEL_X0, top, PANEL_W, 1, T_LINE);      /* (Skiff) as the B mockup: three guides, its top, middle, foot */
    cv_rect(PANEL_X0, (top + bot) / 2, PANEL_W, 1, T_LINE);
    cv_rect(PANEL_X0, bot, PANEL_W, 1, T_LINE);
#if FELUCCA_CAFA
    if(remote) {
        py=EGY(cafa_curve_at(0,214)*1000/16383);
        for(i=1;i<=214;i++){int32_t y=EGY(cafa_curve_at(i,214)*1000/16383);
            cv_line_t(x0+i-1,py,x0+i,y,c,2);py=y;}
    } else
#endif
    {
    cv_line_t(x0, bot, x1, top, c, 2);                                  /* attack: linear */
    px = x1;
    py = top;
    for (i = 1; i <= d; i++) {                                          /* decay: exponential to SUS */
        int32_t lvl;
        e = (e * (32768 - 150733 / d)) >> 15;           /* k^d = exp(-4.6) */
        lvl = sus + ((1000 - sus) * e >> 15);
        cv_line_t(px, py, x1 + i, EGY(lvl), c, 2);
        px = x1 + i;
        py = EGY(lvl);
    }
    cv_line_t(px, py, x3, EGY(sus), c, 2);                               /* sustain */
    px = x3;
    py = EGY(sus);
    e = 32768;
    for (i = 1; i <= r; i++) {                                          /* release: exponential to 0 */
        e = (e * (32768 - 150733 / r)) >> 15;
        cv_line_t(px, py, x3 + i, EGY(sus * e >> 15), c, 2);
        px = x3 + i;
        py = EGY(sus * e >> 15);
    }
    }
    {                                                /* (Skiff) and its points as 5 px blocks, as the mockup's: the
                                                      * peak (ATK), the decay's end (DEC), the release's start (REL),
                                                      * SUS both of the last two; the knob just turned's white */
        int32_t hc = ui.hot_t ? (int32_t)ui.hot_col : -1, hx[3] = {x1, x1 + d, x3}, hy[3] = {top, EGY(sus), EGY(sus)};
        uint32_t points=3;
#if FELUCCA_CAFA
        if(remote){points=cafa_curve_markers(hx,hy,3,214);
            for(i=0;i<(int32_t)points;i++){hx[i]+=x0;hy[i]=EGY(hy[i]);}}
#endif
        for (i = 0; i < (int32_t)points; i++) {                    /* (the peak lit by ATK; the decay's end by DEC, SUS; the release's
                                                      * start by SUS, REL) */
            int hot = hc == i || (i && hc == i + 1);
            if (ux.quark)                            /* ((Skiff Q4) QUARK: nuclei, the knob just turned's gold) */
                qk_nucleus(hx[i], hy[i], hot ? T_ACCENT : T_THEME, hot ? 9 : 7);
            else if (ux.neon)                        /* ((Skiff N4) NEON: sparkles, the knob just turned's big) */
                ne_spark(hx[i], hy[i], hot ? T_THEME : T_ACCENT, hot ? 2 : 1);
            else
                cv_rect(hx[i] - 2, hy[i] - 2, 5, 5, hot ? T_ACCENT : c);
        }
        if (ux.quark) {                              /* ((Skiff Q4) QUARK: DECAY's and RELEASE's half-lives over the
                                                      * sustain (under it near the top) */
            char hb[16];
            int32_t ly = EGY(sus) - 14 < 2 ? EGY(sus) + 6 : EGY(sus) - 14, lx = x1 + d + 10, rx;
            if (!remote && pg->id[1] == P_DEC) {                /* (the ENV page's own envelope: its times' table) */
                qk_halflife(hb, t->p[P_DEC]);
                if (lx + text_w(&AF_S, hb) <= 230)   /* (in the panel, or none) */
                    lx = cv_text(lx, ly, &AF_S, hb, hc == 1 ? T_ACCENT : T_MID) + 6;
                else
                    lx = 240;
                qk_halflife(hb, t->p[P_REL]);
                rx = x3 - 8 - text_w(&AF_S, hb);
                if (rx >= lx && rx > x1 + d)
                    cv_text(rx, ly, &AF_S, hb, hc == 3 ? T_ACCENT : T_MID);
            }
        }
    }
#undef EGY
}

static void graph_lfo(const track_t *t, uint16_t c)
{
    int32_t x, cy = graph_ht / 2, a = 38 * graph_ht / 100, py = cy;
#if FELUCCA_CAFA
    if(cafa_curve_remote()&&!cafa_curve_count()){cv_rect(PANEL_X0,cy,PANEL_W,1,T_RAISE);return;}
#endif
    if (ux.crypt) {                                  /* (Skiff C4) CRYPT: the snake */
        cr_lfo(t);
        return;
    }
    int32_t now = lfo_view_now(t, PANEL_W - 1);
    if (ux.quark) {                                  /* (Skiff Q4) QUARK: the orbit and its projection: the electron's
                                                      * height drawn out as the wave (its 2 s, lfo_view_at), the electron
                                                      * where the LFO is now, a dashed line to it on the wave */
        int32_t ox = 46, r = 34 * graph_ht / 100, x0 = 92, w = PANEL_X0 + PANEL_W - x0, nx = lfo_view_now(t, w - 1);
        int32_t ey = cy - lfo_view_at(t, nx, w) * r / 32768, dy = ey - cy, k = r, i;
        while (k > 0 && k * k > r * r - dy * dy)
            k--;
        qk_ellipse(ox, cy, r, r * 34 / 100, 0x15555555u, T_LINE, 32);
        qk_ellipse(ox, cy, r, r * 34 / 100, 0xEAAAAAABu, T_LINE, 32);
        qk_ellipse(ox, cy, r, r, 0, c, 48);
        cv_rect(ox - 2, cy - 2, 2, 2, T_TRK(0));
        cv_rect(ox + 1, cy - 2, 2, 2, T_TRK(1));
        cv_rect(ox - 1, cy + 1, 2, 2, T_TRK(2));
        cv_rect(x0, 4, 1, graph_ht - 8, T_LINE);
        cv_rect(x0, cy, w, 1, T_LINE);
        for (x = 0; x < w; x++) {
            int32_t y = cy - lfo_view_at(t, x, w) * r / 32768;
            if (x)
                cv_line_t(x0 + x - 1, py, x0 + x, y, c, 2);
            py = y;
        }
        for (i = ox + k + 4; i < x0 + nx - 3; i += 3)
            cv_rect(i, ey, 1, 1, T_ACCENT);
        qk_disc(ox + k, ey, 3, T_ACCENT);
        qk_disc(x0 + nx, ey, 2, T_ACCENT);
        return;
    }
    if (ux.neon) {                                   /* ((Skiff N4) NEON: the wave up in the panel, a floor under it) */
        cy = 40 * graph_ht / 100;
        a = 30 * graph_ht / 100;
    }
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    if (ux.neon) {                                   /* (Skiff N4) NEON: the wave a ridge of vector mountains on a grid */
        int32_t hy = cy + a + 6;                     /* floor (design/mock_neon.py a_lfo) */
        for (x = 0; x < PANEL_W; x += 4) {
            int32_t y = cy - lfo_view_at(t, x, PANEL_W) * a / 32768 + 4;
            if (y < hy)
                cv_rect(PANEL_X0 + x, y, 1, hy - y, ux_mix(NE_VIOLET, T_SURF, 55));
        }
        if (graph_ht - 1 - hy >= 8)
            ne_grid(PANEL_X0, PANEL_X0 + PANEL_W, hy, graph_ht - 1, NE_VIOLET, T_SURF);
    }
    for (x = 0; x < PANEL_W; x++) {                  /* (Skiff) its 2 s as it runs (lfo_view_at): RATE, PHASE, FADE */
        int32_t y = cy - lfo_view_at(t, x, PANEL_W) * a / 32768;
        if (x)
            cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y, ux.neon && x > now ? ux_mix(c, T_SURF, 55) : c, 2);
        py = y;
    }
    x = now;                                         /* .. and where it is now */
    if (ux.neon) {                                   /* (NEON: a white-hot light trail into it, a sparkle) */
        int32_t i;
        for (i = 0; i < 36 && x - i > 0; i++)
            cv_rect(PANEL_X0 + x - i, cy - lfo_view_at(t, x - i, PANEL_W) * a / 32768, 1, 2, ux_mix(T_TEXT, c, i * 100 / 36));
        ne_spark(PANEL_X0 + x, cy - lfo_view_at(t, x, PANEL_W) * a / 32768 + 1, T_ACCENT, 2);
    } else
    cv_rect(PANEL_X0 + x - 2, cy - lfo_view_at(t, x, PANEL_W) * a / 32768 - 2, 5, 5, T_TEXT);
}

static void graph_duck(const track_t *t, uint16_t c)
{
    uint32_t remote=cafa_curve_remote();
    uint32_t src = remote?0u:(uint32_t)t->p[P_DKSRC], k = remote?0u:(uint32_t)(t - trk), i, x, nh = 0;
    uint32_t bar = remote?0u:4u * beat_samples(), per=0,len=0,nb=0,rel=0,hit[NSTEP];
    const track_t *s;
    uint8_t L[37] = {0};                              /* (Skiff) CRYPT: its levels, a 6 px column each */
    if (!ux.crypt)
        cv_rect(8, 10, 224, 62, T_RAISE);
    if (!remote && (!src || src > NTRK || src - 1u == k)) {
        if (ux.crypt)                                /* (no ducking: the level full, dim) */
            cr_art(10, 11, 6, 10, L, 37, T_DIM);
        return;
    }
    if(!remote) {
    s = &trk[src - 1u];
    per = div_samples((uint32_t)s->p[P_SDIV]);
    len = s->p[P_SLEN] > 0 ? (uint32_t)s->p[P_SLEN] : 1u;
    nb = per ? (bar / per ? bar / per : 1u) : 0u;      /* (2BAR, 4BAR: its step 1 at the bar's start) */
    rel = TIME_MS_X10[t->p[P_DKREL] & 127] * 441u / 100u;
    for (i = 0; i < nb && i < NSTEP; i++) {
        uint32_t l = step_lanes(&s->step[i % len]);
        if (drum_track(s) ? l & 1u : l) {
            hit[nh++] = i * per;
            if (ux.crypt)
                cr_ch(7 + (int32_t)(i * per / (bar / 224u)), 88, &AF_S, '^', T_TRK(src - 1u));
            else
                cv_rect(8 + (int32_t)(i * per / (bar / 224u)), 78, 3, 8, T_TRK(src - 1u));
        }
    }
    }
#if FELUCCA_CAFA
    else {
        uint32_t length;const uint8_t *markers=cafa_nv_section(4,&length);
        for(i=0;markers&&i<markers[0];i++) {
            int32_t mx=cafa_native_u14(markers+1u+13u*i)*224/16383;
            if(ux.crypt)cr_ch(7+mx,88,&AF_S,'^',c);else cv_rect(8+mx,78,3,8,c);
        }
    }
#endif
    for (x = 0; x < 224u; x++) {
        uint32_t tau = x * (bar / 224u), j = nh, dip = 0;
#if FELUCCA_CAFA
        if(remote)dip=62u-(uint32_t)cafa_curve_at((int32_t)x,223)*62u/16383u;
        else
#endif
        {
        while (j && hit[j - 1u] > tau)
            j--;
        if (nh) {
            uint32_t dt = j ? tau - hit[j - 1u] : tau + bar - hit[nh - 1u];
            if (dt < rel)
                dip = 62u * (uint32_t)t->p[P_DKAMT] * ((rel - dt) / (rel / 1024u + 1u)) / (127u * 1024u);
        }
        }
        if (ux.crypt) {
            if (x % 6u == 2u && x / 6u < 37u)
                L[x / 6u] = (uint8_t)((dip * 6u + 31u) / 62u);
        } else if (ux.quark) {                       /* ((Skiff Q4) QUARK: a Monte Carlo fill under its level) */
            qk_mc(8 + (int32_t)x, 10 + (int32_t)dip, 72);
            cv_rect(8 + (int32_t)x, 10 + (int32_t)dip, 1, 2, c);
        } else if (ux.neon) {                        /* ((Skiff N4) NEON: the sun's bands under its level, lit on top) */
            ne_band_col(8 + (int32_t)x, 7 + (int32_t)dip, 10, 72);
            cv_rect(8 + (int32_t)x, 10 + (int32_t)dip, 1, 2, c);
        } else
            cv_rect(8 + (int32_t)x, 10 + (int32_t)dip, 1, 62 - (int32_t)dip, c);
    }
    if (ux.crypt) {                                  /* (Skiff) CRYPT: the level as ASCII art over its ':' shade (ENV's) */
        cr_dots(10, 11, 6, 10, L, 37, 6, T_DIM);
        cr_art(10, 11, 6, 10, L, 37, c);
    }
}

static void graph_scope(uint16_t c)
{
    static int16_t snap[SCOPE_N];
    uint32_t remote=cafa_curve_remote(),w=remote?0u:scope_w,i,trig=0;
    uint32_t levels = !remote;
#if FELUCCA_CAFA
    if (remote) { uint32_t len; levels = cafa_nv_section(1, &len) != 0; }
#endif
    int32_t cy = (int32_t)cv_h / 2, py = cy, x, peak = 1500, a = cv_h >= H_GRAPH ? 46 : cy - 6;   /* (LARGE: the strip) */
    if (levels && cv_h >= H_GRAPH && (ux.quark || ux.crypt || ux.windjammer)) {   /* (2.5) the tracks' levels: their themes' own */
        scope_levels();
        if (ux.quark)
            qk_detector(120, cy);
        else if (ux.crypt)
            cr_creatures();
        else
            scope_sea();
        return;
    }
#if FELUCCA_CAFA
    if(remote&&!cafa_curve_count()){cv_rect(PANEL_X0,cy,PANEL_W,1,T_RAISE);return;}
#endif
    for (i = 0; i < SCOPE_N; i++) {
#if FELUCCA_CAFA
        if(remote)snap[i]=(int16_t)(cafa_curve_at((int32_t)i,SCOPE_N-1)*4-32768);else
#endif
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; !remote && i < SCOPE_N - 240u; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    if (ux.crypt) {                                  /* (Skiff N8) CRYPT, MENU > LARGE's strip: the sound burns: fire up
                                                      * from the foot as tall as the wave there (ember, amber, gold, its
                                                      * tips flame), flickering */
        int32_t foot = (int32_t)cv_h - 3, top = 6, k;
        for (x = 0; x < PANEL_W; x += 2) {
            int32_t s = snap[trig + (uint32_t)x], v = (s < 0 ? -s : s) * (foot - top) / peak;
            v += (int32_t)((((uint32_t)x * 7u + ui.frame * 13u) >> 1) % 5u) - 2;   /* (the flicker) */
            if (v < 2)
                v = 2;
            for (k = 0; k < 4; k++) {                /* its four bands, the foot's first */
                int32_t y0 = foot - v * (k + 1) / 4, y1 = foot - v * k / 4;
                cv_rect(PANEL_X0 + x, y0, 2, y1 - y0, k == 0 ? CR_EMBER : k == 1 ? T_THEME : k == 2 ? CR_GOLD : CR_FLAME);
            }
        }
        return;
    }
    if (ux.neon && cv_h >= H_GRAPH) {                /* (Skiff N8) NEON: the sound as the skyline: the wave a ridge of
                                                      * vector mountains over the horizon, the striped sun behind it
                                                      * swelling with how loud it is, the floor running toward the viewer */
        int32_t hy = (int32_t)cv_h * 64 / 100, my = hy / 2 + 2, ma = hy / 2 - 6;
        ne_sun(120, hy, 14 + (peak > 24000 ? 24000 : peak) * 22 / 24000, 30);
        ne_grid_run(PANEL_X0, PANEL_X0 + PANEL_W, hy, (int32_t)cv_h - 2, T_THEME, T_SURF, (int32_t)(ui.frame * 125u % 1000u));
        cv_rect(PANEL_X0, hy, PANEL_W, 1, ux_mix(T_THEME, T_SURF, 30));
        for (x = 0; x < PANEL_W; x += 4) {
            int32_t y = my - snap[trig + (uint32_t)x] * ma / peak + 3;
            if (y < hy)
                cv_rect(PANEL_X0 + x, y, 1, hy - y, ux_mix(NE_VIOLET, T_SURF, 45));
        }
        cy = my;
        a = ma;
    } else
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {
        int32_t y = cy - snap[trig + (uint32_t)x] * a / peak;    /* auto-scaled */
        if (x)
            cv_line_t(PANEL_X0 - 1 + x, py, PANEL_X0 + x, y, c, 2);
        py = y;
    }
}
