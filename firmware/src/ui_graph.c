/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
#if FELUCCA_CAFA
static void graph_dx(const track_t *t,uint16_t c);
static void graph_fm_op(const track_t *t,uint16_t c);
static void graph_fm_all(const track_t *t,uint16_t c);
static void cafa_nv_notes(uint16_t c);
static void cafa_native_transient_draw(void);
enum { CAFA_TRACK_LANES = 6u, CAFA_PATTERN_CELLS = 64u };
static uint32_t cafa_nv_track_lanes(void)
{
    const uint8_t *rows = cafa_nv_section(CAFA_NATIVE_ROWS, 0);
    uint32_t count = rows ? rows[0] : 0u;
    return count < CAFA_TRACK_LANES ? count : CAFA_TRACK_LANES;
}
#endif
/* Felucca UI: the panel (Y_GRAPH .. Y_GRAPH + H_GRAPH) between the cards and the footer: a SURF area
 * (rounded, on BG) holding the page graphs (ADSR, LFO, steps, piano roll, drum grid, scale, FX, SLICER,
 * MOD, lists: presets, user presets, patterns, project slots, song), the HOME oscilloscope, the
 * instrument diagrams; the MIXER page draws four SURF columns instead. Each graph is redrawn only when
 * graph_signature() changes. The look:
 * curves THEME (2 px), guides and empty marks RAISE, captions MID / DIM, the active thing ACCENT,
 * bars rounded; a list's selected row is a THEME bar with INK text. */
#include "ui_crypt.c"                                 /* (Skiff C3) THEME CRYPT's stone, frames and sprites */
#include "ui_neon.c"                                  /* (Skiff N3) THEME NEON's glass, dusk, neon and sprites */
#include "ui_quark.c"                                 /* (Skiff Q2) THEME QUARK's cells, nuclei, tracks, the hazard sign */
#define PANEL_X0 10                                  /* the graphs' inner area: x 10..230 */
#define PANEL_W 220
#define GOY 11                                       /* graphs drawn on a 100 px scale sit at y 11..111 */
/* the height ADSR and LFO are drawn on: 100 px, or (MENU > LARGE's strip: draw_graph) the strip's ~ 56 */
static int32_t graph_ht = 100;

/* (Skiff 2.0) the envelope graphs' one buffer (ENV in every theme, ENV DEST: one is drawn at a time) */
static int16_t eg_lv[217] __attribute__((section(".pool")));
/* (Skiff 2.0) a column under a curve from (x, y) to the foot: NEON the sun's bands (from top), QUARK a Monte Carlo fill,
 * else fill */
static void pg_under(int32_t x, int32_t y, int32_t top, int32_t foot, uint16_t fill)
{
    if (ux.neon)
        ne_band_col(x, y, top, foot);
    else if (ux.quark)
        qk_mc(x, y, foot);
    else
        cv_rect(x, y, 1, foot - y, fill);
}

/* (Skiff C4) the envelope of ATTACK DECAY SUSTAIN RELEASE (0..127) over w + 1 px, 0..1000 a px, as graph_adsr draws it:
 * attack linear, decay and release exponential, the release at the end; *xa *xd *xr: the attack's peak, the decay's end,
 * the release's start */
static void env_fill4(int32_t at, int32_t de, int32_t su, int32_t re, int16_t *lv, int32_t w, int32_t *xa, int32_t *xd,
                      int32_t *xr)
{
    int32_t a = 4 + at * 50 / 127, d = 6 + de * 50 / 127, r = 6 + re * 60 / 127;
    int32_t sus = su * 1000 / 127, x3 = w - r, e = 32768, x;
    for (x = 0; x <= w; x++) {
        if (x <= a)
            lv[x] = (int16_t)(x * 1000 / a);
        else if (x <= a + d) {
            e = (e * (32768 - 150733 / d)) >> 15;
            lv[x] = (int16_t)(sus + ((1000 - sus) * e >> 15));
        } else if (x <= x3) {
            lv[x] = (int16_t)sus;
            e = 32768;
        } else {
            e = (e * (32768 - 150733 / r)) >> 15;
            lv[x] = (int16_t)(sus * e >> 15);
        }
    }
    *xa = a;
    *xd = a + d;
    *xr = x3;
}

/* (Skiff C4) CRYPT's ENV: the envelope as ASCII art (36 columns of 6 px) over its ':' shade, its three points skulls
 * (the knob just turned's the flame's colour) */


/* (Skiff) the LFO over time, as it runs (the graphs of LFO, LFO DEST): LFO_WIN of control ticks (2 s) from its retrigger
 * (a phrase's first note, voice.c: FADE rising from nothing), RATE's cycles in it (or (#16) SYNC's); past it, the last
 * 2 s (the window scrolls).
 * lfo_view_at: the value at px x of w (+-32768, FADE's share in; RND a value a cycle, the graph's own pick; UNI one
 * side); lfo_view_now: where the LFO is now (px) */
#define LFO_WIN (FS / CTL * 2u)
static uint32_t lfo_view_t0(const track_t *t) { return t->lfo_t > LFO_WIN ? t->lfo_t - LFO_WIN : 0u; }



/* (Skiff C4) CRYPT's LFO: its 2 s as ASCII art, faint, and as a snake up to where the LFO is now, its head there (past
 * 2 s the path scrolls under it) */


/* Matches voice.c: attack is linear, decay and release are exponential
 * (env += (target - env) * k each tick, ~99 % after the set time). Time
 * axis is the parameter value (the times themselves are exponential). */




/* step bar x of step i of a 16-step row: 4 groups, as the footer (9 px bars, 13 px apart, 4 px between groups),
 * the row centred: x 12 .. 228 */
static int32_t bar_x(uint32_t i) { return 12 + (int32_t)(i % 16u) * 13 + (int32_t)(i % 16u / 4u) * 4; }

/* PATTERN page: the 64 steps as 4 rows of 16 bars (an accent: the accent colour, a tie: a lower bar,
 * empty: a stub), the cursor and the playhead under them ((2.5) a FREE TAKE's picker: no cursor, no DIR) */
static void knob_arc(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t lo, int32_t hi,
                     uint16_t tr, uint16_t vc, uint16_t bg);
/* (2.5) the PRESETS knob, 20 px, top left (x, y): its ring's track MID, the arc to the value (pos of n) c, a pointer
 * to it TEXT, as a knob's notch; bg what is under it */
static void pr_knob(int32_t x, int32_t y, int32_t pos, int32_t n, uint16_t c, uint16_t bg)
{
    static const int8_t PT[7][2] = {{-5, 5}, {-7, 0}, {-5, -5}, {0, -7}, {5, -5}, {7, 0}, {5, 5}};   /* -135 .. 135 */
    int32_t a = n > 1 ? -384 + pos * 768 / (n - 1) : 0, k = (a + 384 + 64) / 128;   /* (KA_END 384: 135 degrees) */
    int32_t cx = x + KNOB_SMALL_R, cy = y + KNOB_SMALL_R;
    knob_arc(x, y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, -384, a, T_MID, c, bg);
    cv_line(cx, cy, cx + PT[k][0], cy + PT[k][1], T_TEXT);
}
/* (2.5, the user's 5B) a setting PRESETS turns on this page, as a card of its own in the panel, its top right at
 * (xr, y), 84 x 40: PRESETS (the SMK-37's K4) and its knob, the value (pos of n) in c, the job DIM; one line (lo:
 * 22 px, 112 wide, where 40 don't fit): PRESETS, the value, the knob */
#define PR_CARD_W 84
#define PR_CARD_H 40
#define PR_LINE_W 112
#define PR_LINE_H 22
static void pr_card(int32_t xr, int32_t y, const char *job, const char *val, int32_t pos, int32_t n, uint16_t c, int lo)
{
    int32_t x0 = xr - (lo ? PR_LINE_W : PR_CARD_W), x;
    cv_rrect(x0, y, lo ? PR_LINE_W : PR_CARD_W, lo ? PR_LINE_H : PR_CARD_H, 2, T_RAISE, T_SURF);
    if (lo) {
        x = cv_text(x0 + 5, y + 15 - AF_S.asc, &AF_S, kc_name(KC_PRESETS), T_MID) + 5;
        cv_text(x, y + 18 - AF_M.asc, &AF_M, val, c);
        pr_knob(xr - 2 * KNOB_SMALL_R - 3, y + 1, pos, n, c, T_RAISE);
        return;
    }
    cv_text(x0 + 5, y + 13 - AF_S.asc, &AF_S, kc_name(KC_PRESETS), T_MID);
    pr_knob(xr - 2 * KNOB_SMALL_R - 4, y + 3, pos, n, c, T_RAISE);
    cv_text(x0 + 5, y + 35 - AF_M.asc, &AF_M, val, c);
    cv_text_r(xr - 4, y + 35 - AF_S.asc, &AF_S, job, T_DIM, T_RAISE);
}
static void graph_steps(const track_t *t, uint16_t c)
{
    uint32_t i,len,cur,play;
    int32_t sm = graph_ht < 100;
    int remote=0;
#if FELUCCA_CAFA
    if(cafa_native_profile_view)remote=1;
#endif
    len=remote?0u:(uint32_t)t->p[P_SLEN];
    cur=remote || fr.on ? UINT32_MAX : ui.cursor;
    play=remote ? UINT32_MAX : t->seq_idx;
#if FELUCCA_CAFA
    if(remote) {
        uint32_t first=cafa_native_u14(cafa_native_profile_view+17);
        const uint8_t *cells=cafa_nv_section(CAFA_NATIVE_CELLS,0);
        len=cells?cafa_native_u14(cells):0u;
        if(len>CAFA_PATTERN_CELLS)len=CAFA_PATTERN_CELLS;
        sm=graph_h()<100u;
        if(cafa_native_profile_view[6]!=CAFA_SCREEN_TAKE_PICKER) {
            uint32_t abs=cafa_native_u14(cafa_native_profile_view+20);
            if(abs!=16383u && abs>=first && abs-first<len)cur=abs-first;
        }
        {uint32_t abs=cafa_native_u14(cafa_native_profile_view+22);
         if(abs!=16383u && abs>=first && abs-first<len)play=abs-first;}
    }
#endif
    int32_t bh = sm ? 10 : 14, pitch = sm ? 14 : 24, y0 = sm ? 2 : 4;
    for (i = 0; i < len && (remote || i < NSTEP); i++) {
        int32_t x = bar_x(i), y = y0 + (int32_t)(i / 16u) * pitch;
        const step_t *st=remote?0:&t->step[i];
        uint32_t on,tie,accent;
#if FELUCCA_CAFA
        if(remote) {
            const uint8_t *cell=cafa_nv_cell(i);
            uint32_t flags=cell?cell[2]:0u;
            on=flags&1u;tie=flags&8u;accent=flags&32u;
        } else
#endif
        {on=step_on(st);tie=step_tie(st);accent=(st->flags&SF_ACCENT)!=0u;}
        if (i % 16u == 0u && i + 16u <= len)
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS | AL_N(16), "step bars' row centred");
        if (ux.crypt) {                              /* (Skiff S1) CRYPT: ASCII, # a note, - a tie, . nothing */
            int32_t base = y + bh / 2 + 4;
            if (on)
                cr_ch(x + 2, base, &AF_S, '#', accent ? T_ACCENT : c);
            else if (tie)
                cr_ch(x + 3, base, &AF_S, '-', T_MID);
            else
                cr_ch(x + 4, base, &AF_S, '.', T_DIM);
        } else if (ux.quark) {                       /* (Skiff Q4) QUARK: a bunch, a tie its trail, nothing a dot */
            if (on)
                qk_disc(x + 4, y + bh / 2, bh > 10 ? 4 : 3, accent ? T_ACCENT : c);
            else if (tie)
                cv_rect(x, y + bh / 2, 9, 1, T_MID);
            else
                cv_rect(x + 4, y + bh / 2, 1, 1, T_DIM);
        } else if (ux.neon && on)           /* (Skiff S1) NEON: a lit tube */
            ne_tube(x, y, 9, bh, accent ? T_ACCENT : c, T_SURF);
        else if (on)
            cv_rrect(x, y, 9, bh, 2, accent ? T_ACCENT : c, T_SURF);
        else if (tie)
            cv_rrect(x, y + bh - 6, 9, 6, 1, T_MID, T_SURF);
        else
            cv_rrect(x, y + bh - 3, 9, 3, 1, T_RAISE, T_SURF);
        if (i == cur && ux.neon)                     /* ((Skiff S1) NEON: the cursor a laser, glowing) */
            cv_rect(x - 1, y + bh + 1 - sm, 11, 4, ux_mix(T_ACCENT, T_SURF, 60));
        if (i == cur)
            cv_rect(x, y + bh + 2 - sm, 9, 2, T_ACCENT);
        else if ((remote || song.playing) && i == play)
            cv_rect(x, y + bh + 2 - sm, 9, 2, ux.crypt ? CR_GOLD : T_TEXT);
    }
    if (!sm && !remote && !fr.on) {                             /* ((Skiff 2.0) the track's DIR, PRESETS': (2.5, 5B) its card, under
                                                      * the rows, right (a short pattern: where the rows' fourth is) */
        int32_t y = y0 + (int32_t)((len + 15u) / 16u) * pitch - (pitch - bh) + 5, bot = (int32_t)graph_h() - cv_oy - 2;
        uint32_t d = (uint32_t)t->p[P_DIR] % NELEM(N_DIR);   /* (bot: the panel's foot, on this scale) */
        int lo = y + PR_CARD_H > bot;
        if (y < 66)
            y = 66;
        if (y > bot - (lo ? PR_LINE_H : PR_CARD_H))
            y = bot - (lo ? PR_LINE_H : PR_CARD_H);
        pr_card(228, y, "DIR", N_DIR[d], (int32_t)d, (int32_t)NELEM(N_DIR), c, lo);
    }
#if FELUCCA_CAFA
    else if(!sm && remote && cafa_native_profile_view[6]!=CAFA_SCREEN_TAKE_PICKER) {
        const uint8_t *r=cafa_nv_row(0);char label[17],value[17];
        if(r) {
            int32_t y=66,bot=(int32_t)graph_h()-cv_oy-2,lo=y+PR_CARD_H>bot;
            cafa_native_text(label,r,16);cafa_native_text(value,r+16,16);
            if(y>bot-(lo?PR_LINE_H:PR_CARD_H))y=bot-(lo?PR_LINE_H:PR_CARD_H);
            pr_card(228,y,label,value,(int32_t)cafa_native_u14(r+41)*1000/16383,1001,c,lo);
        }
    }
#endif
}

/* STEP page (a melodic track): a piano roll of the cursor's 16-step page. PR_ROWS semitone rows of PR_RH px,
 * the highest on top; on the left the C rows named (S, DIM) and a keyboard strip (white keys RAISE, black keys a
 * shorter DIM; a key held on the track: its row ACCENT). Rows in the track's scale (SCL ROOT / SCALE; CHR: the white
 * keys) are tinted LANE, a C row closes with a RAISE line; step lines GRID, every 4th RAISE. A note is a 9 x 3 bar in
 * its column (an accent: TEXT, the row's full height; the cursor step's notes: ACCENT), chords stacked, lane hits as
 * their notes (a RATCH step's bars in its 2..4 parts, 1 px apart); a TIE carries the bars of the note before through
 * its column; a REST is empty; a slide is a TEXT diagonal from the end of its bar into the next note (not from a
 * RATCH step: it never slides out). Notes outside
 * the view: a 1 px mark on the edge. The cursor: a TEXT column frame; the playhead: a 1 px ACCENT line. The view
 * (proll.lo, the bottom row) fits the notes of the page, else centres on the cursor's note, and follows in steps (a
 * third of the way per frame); graph_signature() holds it, so a page that does not change is not drawn again. */
#define PR_ROWS 21                                  /* semitones shown (1.75 octaves) */
#define PR_RH 5                                     /* px per row */
#define PR_Y0 9                                     /* the top row (canvas y) */
#define PR_X0 35                                    /* the first step column */
#define PR_CW 12                                    /* px per step */
#define PR_KX 24                                    /* the keyboard strip, 9 px */
static struct {
    uint8_t lo, trk, init, moving;                    /* lo: the note of the bottom row; moving: on its way */
    uint32_t frame;
} proll;
static const uint8_t KEY_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

static int32_t pr_note(const step_t *st, uint32_t j)   /* the step's note j (0..3), then its lane hits (-1: none) */
{
    if (j < 4u)
        return j < st->n ? st->note[j] : -1;
    return (st->hit >> (j - 4u)) & 1u ? DRUM_LANE_NOTE[j - 4u] : -1;
}
static uint32_t pr_src(const track_t *t, uint32_t si, uint32_t len)   /* the step whose notes sound at si (NSTEP: none) */
{
    uint32_t k;
    for (k = 0; k < len && t->step[si].time == ST_TIE; k++)
        si = (si + len - 1u) % len;
    return t->step[si].time == ST_NOTE && (t->step[si].n || t->step[si].hit) ? si : NSTEP;   /* ((2.0) notes going on too) */
}
/* (2.0, #25) where the note of pitch p going on at step si started (back over the steps it goes on, TIEs too); NSTEP:
 * none (it holds nothing) */
static uint32_t pr_start(const track_t *t, uint32_t si, uint32_t p)
{
    uint32_t len = (uint32_t)t->p[P_SLEN], k;
    int32_t x;
    for (k = 0; k < len; k++) {
        const step_t *s = &t->step[si];
        if (s->time != ST_TIE) {
            if ((x = note_at(s, p)) < 0)
                return NSTEP;
            if (!step_tied(s, (uint32_t)x))
                return si;
        }
        si = (si + len - 1u) % len;
    }
    return NSTEP;
}
/* the rows lit by keys held on the selected track (bit r: row r from the top) */
static uint32_t pr_held(void)
{
    uint32_t k, i, m = 0;
    for (k = 0; k < KB_N; k++)
        if (kb_chn[k] && kb_trk[k] == song.sel)
            for (i = 0; i < kb_chn[k]; i++) {
                int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - kb_chord[k][i];
                if (r >= 0 && r < PR_ROWS)
                    m |= 1u << r;
            }
    return m;
}
/* once a frame: the view toward the page's notes (centred; once there it stays while they fit); at once on a redraw
 * forced, a gap in the frames, another track or MENU > ANIM OFF */
static void pr_follow(const track_t *t)
{
    uint32_t i, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t lo = 127, hi = -1, tgt, d, cur = proll.lo;
    int snap = ui.force || !proll.init || proll.trk != song.sel || ui.frame != proll.frame + 1u || (ui_prefs & PREF_ANIM_OFF);
    if (proll.init && proll.frame == ui.frame)
        return;
    for (i = 0; i < 16u && base + i < len; i++) {
        uint32_t s = pr_src(t, base + i, len);
        for (j = 0; s < NSTEP && j < 4u + NLANE; j++) {
            int32_t n = pr_note(&t->step[s], j);
            if (n < 0)
                continue;
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    }
    if (hi < 0)
        lo = hi = last_note;
    if (hi - lo <= PR_ROWS - 1)                     /* all fit: centred, then kept while they fit */
        tgt = !snap && !proll.moving && lo >= cur && hi <= cur + PR_ROWS - 1 ? cur : (lo + hi + 1) / 2 - PR_ROWS / 2;
    else {                                            /* too wide: the cursor's (lowest) note in the middle */
        const step_t *st = &t->step[ui.cursor];
        tgt = (st->n ? st->note[0] : (lo + hi) / 2) - PR_ROWS / 2;
    }
    tgt = clamp(tgt, 0, 128 - PR_ROWS);
    d = tgt - cur;
    proll.lo = (uint8_t)(snap ? tgt : cur + (d / 3 ? d / 3 : d > 0 ? 1 : d < 0 ? -1 : 0));
    proll.moving = proll.lo != tgt;
    proll.trk = (uint8_t)song.sel;
    proll.frame = ui.frame;
    proll.init = 1;
}
static int32_t pr_row_y(int32_t n) { return PR_Y0 + ((int32_t)proll.lo + PR_ROWS - 1 - n) * PR_RH; }
/* a bar w px wide in `hits` equal parts, 1 px apart (a RATCH step; 1: the bar) */
static void pr_split(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t hits)
{
    int32_t k, pw = (w - (int32_t)hits + 1) / (int32_t)hits;
    if (hits < 2u) {
        cv_rect(x, y, w, h, c);
        return;
    }
    for (k = 0; k < (int32_t)hits; k++)
        cv_rect(x + k * (pw + 1), y, pw, h, c);
}
/* the bars of step s in column x (in its RATCH parts, hits; a TIE's carried bars: 1): a 1 px edge mark for a note
 * out of view; first: row y of the step's first note */
static void pr_bar(int32_t x,int32_t y,int32_t w,uint16_t c,int acc,uint32_t hits)
{
    int32_t ybot=PR_Y0+PR_ROWS*PR_RH;
    if(y<PR_Y0)pr_split(x,PR_Y0,w,1,c,hits);
    else if(y>=ybot)pr_split(x,ybot-1,w,1,c,hits);
    else if(ux.quark && hits==1u) {
        int32_t k2;uint16_t cc=acc && c!=T_ACCENT?T_TEXT:c;
        qk_disc(x+1,y+2,1,cc);
        for(k2=x+4;k2<x+w;k2+=2)
            cv_rect(k2,y+2,1,1,ux_mix(cc,T_SURF,50*(k2-x)/(w>1?w:1)));
    } else if(ux.crypt && hits==1u)
        cr_blade(x,y+1,w,PR_RH-2,acc && c!=T_ACCENT?T_TEXT:c);
    else if(ux.neon && hits==1u)
        ne_tube(x,y+1,w,PR_RH-2,acc && c!=T_ACCENT?T_TEXT:c,T_SURF);
    else if(acc)pr_split(x,y,w,PR_RH,c==T_ACCENT?c:T_TEXT,hits);
    else pr_split(x,y+1,w,PR_RH-2,c,hits);
}
static void pr_bars(const step_t *st, int32_t x, int32_t w, uint16_t c, int acc_all, int32_t *first, uint32_t hits)
{
    uint32_t j;
    for (j = 0; j < 4u + NLANE; j++) {
        int32_t n = pr_note(st, j), y;
        int acc;
        if (n < 0)
            continue;
        acc = acc_all || (j >= 4u && ((st->acc >> (j - 4u)) & 1u));
        y = pr_row_y(n);
        if (*first < -1000)
            *first = y;
        pr_bar(x,y,w,c,acc,hits);
    }
}
/* (Skiff 2.0) SEQ > FEEL (STEP's tiles before, the B mockup's): the 16 steps of the cursor's bar as tiles, two rows of
 * eight (the keyboard's two halves), each over its number. A note: a tile of the track's colour, in INK low in it its
 * value of FEEL's knob turned last (nsel.view: CHANCE first; OFF, a REST, half lit); a TIE: the tile half way to the
 * empty one's; empty: SURF (FLAT, LINE: RAISE); the step edited: white, a range framed (SHIFT + PRESETS). A chance under
 * 100 % or a condition: a 4 px INK block in its corner (the mockup's), a lock: one at its top right. The number under
 * it: MID, the step playing's in REC's colour. BLOCKS: on the black ground, as the mockup; the block centred up and
 * down in the panel. A drum track: the grid (graph_grid), as on NOTES */
#define TL_W 27                                       /* a tile 27 x 33, 29 apart from x 4: 4 .. 233 */
#define TL_H 33
#define TL_P 58                                       /* the rows 58 apart: a tile, its number, the gap */
#define TL_NY 15                                      /* a note's name: its capitals rows 18 .. 27 of the tile */
static void graph_tiles(const track_t *t, uint16_t c)
{
    int remote=0;
#if FELUCCA_CAFA
    if(cafa_native_profile_view)remote=1;
#endif
    uint32_t i,len=remote?0u:(uint32_t)t->p[P_SLEN],base=remote?0u:ui.bank*16u;
    uint64_t locks=remote?0u:motion_lock_steps(trk_index(t));
#if FELUCCA_CAFA
    if(remote) {
        uint32_t bytes;const uint8_t *cells=cafa_nv_section(CAFA_NATIVE_CELLS,&bytes);
        len=cells?cafa_native_u14(cells):0u;
        base=cafa_native_u14(cafa_native_profile_view+17);
    }
#endif
    int32_t top = ((int32_t)cv_h - (TL_P + TL_H + 3 + AF_S_CAP_H)) / 2;
    uint16_t empty = T_SURF, g = T_BG;
    cv_rect(0, 0, 240, (int32_t)cv_h, T_BG);
    cv_bg = T_BG;
    for (i = 0; i < 16u && (remote?i<len:base+i<len); i++) {
        uint32_t si=base+i,cur=0u;const step_t *st=remote?0:&t->step[si];
        uint32_t flags=0u,played=0u,prob=0u;
#if FELUCCA_CAFA
        if(remote) {
            const uint8_t *cell=cafa_nv_cell(i);
            flags=cell?cell[2]:0u;
            cur=(si==cafa_native_u14(cafa_native_profile_view+20)) || (flags&2u);
            played=(si==cafa_native_u14(cafa_native_profile_view+22)) || (flags&4u);
            prob=flags&32u;
        } else
#endif
        {cur=si==ui.cursor && !nsel.a1;played=song.playing && si==t->seq_idx;}
        int32_t x = 4 + 29 * (int32_t)(i % 8u), y = top + TL_P * (int32_t)(i / 8u);
        int on=remote?(flags&1u):step_starts(st)&&st->n,tie=remote?(flags&8u):step_tie(st);
        uint16_t fill = cur ? T_TEXT : on ? (!remote && st->time == ST_REST ? ux_mix(c, empty, 70) : c) : tie ? ux_mix(c, empty, 50) :
                        empty;
        char nb[8];
        if (ux.neon && (on || cur))                   /* ((Skiff N5) NEON: a note a lit tube, the edited one white) */
            ne_tube(x, y, TL_W, TL_H, fill, g);
        else
        cv_rrect(x, y, TL_W, TL_H, 3, fill, g);
        if (ux.crypt && (on || cur))                  /* ((Skiff S1) CRYPT: a plaque, its border engraved; the edited
                                                       * one's gold) */
            cv_frame(x + 2, y + 2, TL_W - 4, TL_H - 4, cur ? CR_GOLD : ux_mix(fill, T_INK, 45));
        if (on) {
#if FELUCCA_CAFA
            if(remote){const uint8_t *cell=cafa_nv_cell(i);cafa_native_text(nb,cell+3,4);}
            else
#endif
            (void)feel_text(nb, st, nsel.view);
            GFX_HOOK_ALIGN(x, 0, x + TL_W, 0, AL_H, "step tile note centred");
            cv_text_in(x, y + TL_NY, TL_W, &AF_S, nb, T_INK, fill);
            if (remote?prob:step_chance(st)<100u || st->cond)
                cv_rect(x + 2, y + 2, 4, 4, T_INK);
        }
        if (
#if FELUCCA_CAFA
            remote ? (flags & (cafa_nv_section(CAFA_NATIVE_PREFS,0) ? 16u : 8u)) :
#endif
            ((locks >> si)&1u))
            cv_rect(x + TL_W - 6, y + 2, 4, 4, on || cur ? T_INK : T_MID);
        fmt_int(nb, (int32_t)si + 1);
        GFX_HOOK_ALIGN(x, 0, x + TL_W, 0, AL_H, "step tile number centred");
        cv_text_in(x, y + TL_H + 3 - AF_S_CAP_Y, TL_W, &AF_S, nb, played ? T_REC : T_MID, g);
    }
    for (i = 0; !remote && nsel.a1 && i < 2u; i++) {              /* (2.0) a range: framed, a row at a time */
        int32_t a = (int32_t)sel_lo() - (int32_t)(base + 8u * i), b = (int32_t)sel_hi() - (int32_t)(base + 8u * i);
        a = a < 0 ? 0 : a;
        b = b > 7 ? 7 : b;
        if (a <= b)
            cv_frame(2 + 29 * a, top + TL_P * (int32_t)i - 2, 29 * (b - a) + TL_W + 4, TL_H + 4, T_TEXT);
    }
}
static void graph_roll(const track_t *t, uint16_t c)
{
#if FELUCCA_CAFA
    if(cafa_native_profile_view){cafa_nv_notes(c);return;}
#endif
    uint32_t i, k, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u, ncol, mask = scale_mask(t), held = pr_held();
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t r, ybot = PR_Y0 + PR_ROWS * PR_RH, gw;
    char nb[8];
    ncol = base < len ? (len - base < 16u ? len - base : 16u) : 0u;
    gw = (int32_t)ncol * PR_CW + 1;
    for (i = sel_lo(); nsel.picked && i <= sel_hi() && i < NSTEP; i++)   /* ((2.0) the notes picked: their keys lit, */
        for (k = 0; sel_step(t, i) && k < t->step[i].n; k++)             /* as keys held) */
            if (sel_voice(i, k) && (r = (int32_t)proll.lo + PR_ROWS - 1 - t->step[i].note[k]) >= 0 && r < PR_ROWS)
                held |= 1u << r;
    for (r = 0; r < PR_ROWS; r++) {                 /* rows: the lane, the key, the C names */
        int32_t n = (int32_t)proll.lo + PR_ROWS - 1 - r, y = PR_Y0 + r * PR_RH;
        uint32_t pc = (uint32_t)n % 12u;
        int in = mask == 0xFFFu ? !KEY_BLACK[pc] : (int)((mask >> ((uint32_t)(n - t->p[P_ROOT] + 120) % 12u)) & 1u);
        if (in)
            cv_rect(PR_X0, y, gw, PR_RH, T_LANE);
        if ((held >> r) & 1u)
            cv_rect(PR_KX, y, 9, PR_RH - 1, T_ACCENT);
        else
            cv_rect(PR_KX, y, KEY_BLACK[pc] ? 5 : 9, PR_RH - 1, KEY_BLACK[pc] ? T_DIM : T_RAISE);
        if (pc == 0u) {
            cv_rect(PR_X0, y + PR_RH - 1, gw, 1, T_RAISE);
            note_name(nb, (uint32_t)n);
            GFX_HOOK_ALIGN(0, y, 23, y + PR_RH, AL_V, "piano roll C label on its row");
            GFX_HOOK_ALIGN(0, y, 23, y + PR_RH, AL_R | AL_PASS, "piano roll C labels' right edge");
            {   /* its ink ends at x 23, a column before the keys (by the advance, C1's 1 ended 2 px short) */
                int32_t b[4];
                text_ink(&AF_S, nb, b);
                cv_text_on(23 - b[2], y - 5, &AF_S, nb, T_DIM, T_SURF);   /* (the widest, "C-1", inside the panel) */
            }
        }
    }
    for (i = 0; i <= ncol; i++)                       /* step lines, the beats brighter */
        cv_rect(PR_X0 + (int32_t)i * PR_CW, PR_Y0, 1, ybot - PR_Y0, i % 4u ? T_GRID : T_RAISE);
    {   /* the cursor ((2.0) the selection's range: SHIFT + PRESETS) framed */
        int32_t a = (int32_t)sel_lo() - (int32_t)base, b = (int32_t)sel_hi() - (int32_t)base, w;
        a = a < 0 ? 0 : a;
        b = b >= (int32_t)ncol ? (int32_t)ncol - 1 : b;
        w = (b - a + 1) * PR_CW + 1;
        if (a <= b && ux.neon)                        /* ((Skiff N5) NEON: a laser) */
            ne_laser(PR_X0 + a * PR_CW, PR_Y0 - 2, w, ybot - PR_Y0 + 4, T_SURF);
        else if (a <= b && ux.quark)                  /* ((Skiff Q4) QUARK: gold corners) */
            qk_corners(PR_X0 + a * PR_CW, PR_Y0 - 2, w, ybot - PR_Y0 + 4, T_ACCENT, 4);
        else if (a <= b)                              /* ((Skiff S1) CRYPT: a gold frame) */
            cv_frame(PR_X0 + a * PR_CW, PR_Y0 - 2, w, ybot - PR_Y0 + 4, ux.crypt ? CR_GOLD : T_TEXT);
    }
    for (i = 0; i < ncol; i++) {
        uint32_t si = base + i;
        int32_t x = PR_X0 + (int32_t)i * PR_CW;
        if (song.playing && si == t->seq_idx && ux.neon)   /* ((Skiff S1) NEON: the playhead a laser, glowing) */
            cv_rect(x + 5, PR_Y0, 3, ybot - PR_Y0, ux_mix(T_ACCENT, T_SURF, 60));
        if (song.playing && si == t->seq_idx)
            cv_rect(x + 6, PR_Y0, 1, ybot - PR_Y0, ux.crypt ? CR_GOLD : T_ACCENT);
        if ((locks >> si) & 1u)                       /* a parameter lock: a mark under the step */
            cv_rect(x + 3, ybot + 3, 7, 2, T_ACCENT);
    }
    for (i = 0; i < ncol; i++) {                      /* the notes */
        uint32_t si = base + i, s = pr_src(t, si, len), nx = (si + 1u) % len;
        const step_t *st, *ns = &t->step[nx];
        int32_t x = PR_X0 + (int32_t)i * PR_CW, y = -10000;
        uint16_t col;
        if (s == NSTEP)
            continue;                                 /* a REST, empty, or a TIE after one */
        st = &t->step[s];
        r = sel_step(t, s);                           /* ((2.0) selected: lit; picked notes: only those, over it) */
        col = (r && !nsel.picked) || (!nsel.a1 && si == ui.cursor) ? T_ACCENT : c;
        for (k = 0; k < 1u + (uint32_t)(r && nsel.picked); k++) {
            step_t pk = *st;
            uint32_t v;
            if (k) {                                  /* (the picked ones) */
                for (pk.n = 0, v = 0; v < st->n; v++)
                    if (sel_voice(s, v) && !step_tied(st, v))
                        pk.note[pk.n++] = st->note[v];
                pk.hit = 0;
                col = T_ACCENT;
            } else if (s == si && step_ties(st)) {    /* ((2.0, #25) the notes going on: on through the gap before, lit
                                                       * as their start is; the ones starting here as below) */
                for (pk.n = 0, v = 0; v < st->n; v++) {
                    uint32_t b = pr_start(t, si, st->note[v]), lit;
                    step_t one = *st;
                    if (!step_tied(st, v)) {
                        pk.note[pk.n++] = st->note[v];
                        continue;
                    }
                    lit = b < NSTEP && sel_step(t, b) &&
                          (!nsel.picked || ((nsel.vp[b] >> (uint32_t)note_at(&t->step[b], st->note[v])) & 1u));
                    one.note[0] = st->note[v];
                    one.n = 1;
                    one.hit = 0;
                    pr_bars(&one, x - 1, 12, lit || (!nsel.a1 && si == ui.cursor) ? T_ACCENT : c,
                            b < NSTEP && (t->step[b].flags & SF_ACCENT), &y, 1u);
                }
            }
            if (s == si)                              /* ((Skiff) where it plays: its NUDGE, inside the grid) */
                pr_bars(&pk, clamp(x + 2 + st->nudge * PR_CW / 64, PR_X0 + 1, PR_X0 + gw - 10), 9, col,
                        (st->flags & SF_ACCENT) != 0u, &y, step_ratchet(st));
            else                                      /* a TIE: the bars on through the gap before */
                pr_bars(&pk, x - 1, 12, col, (st->flags & SF_ACCENT) != 0u, &y, 1u);
        }
        if ((st->flags & SF_SLIDE) && step_ratchet(st) == 1u &&   /* (a ratcheted step never slides out) */
            ns->time == ST_NOTE && ns->n && y >= PR_Y0 && y < ybot) {
            int32_t ny = clamp(pr_row_y(ns->note[0]), PR_Y0, ybot - PR_RH);
            cv_line(x + 10, y + 2, x + 14, ny + 2, T_TEXT);   /* the slide into the next note */
        }
    }
    {   /* ((Skiff U4, L5) its scale, the footer's before ("C MIN"): under the roll at its right */
        char sc[12];
        str_cpy(sc, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
        str_cpy(sc + str_len(sc), " ", 4);
        str_cpy(sc + str_len(sc), N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)NELEM(N_SCALE) - 1)], 8);
        cv_text_r(PR_X0 + 16 * PR_CW, ybot + 2, &AF_S, sc, T_DIM, T_SURF);
    }
}
/* SEQ > NOTES on a DRUM track: the grid, 8 lanes x the 16 steps of the page shown. Lanes by their two-letter
 * names (BD SD CP CH OH TM RS CB; CG CL CY on the other kits). A hit is a rounded square (accented: the
 * accent; a RATCH step's: its 2..4 parts as bars), an empty step a dot (brighter on the beats and on the selected lane); the selected lane is
 * underlaid RAISE, the cursor framed in TEXT; a TEXT bar over the step playing */
static void graph_grid(const track_t *t, uint16_t c)
{
    int remote=0;
#if FELUCCA_CAFA
    if(cafa_native_profile_view)remote=1;
#endif
    uint32_t l,i,len=remote?0u:(uint32_t)t->p[P_SLEN],base=remote?0u:ui.bank*16u;
    uint64_t locks=remote?0u:motion_lock_steps(trk_index(t));
#if FELUCCA_CAFA
    if(remote){uint32_t bytes;const uint8_t *cells=cafa_nv_section(CAFA_NATIVE_CELLS,&bytes);
        len=cells?cafa_native_u14(cells)/8u:0u;
        base=cafa_native_u14(cafa_native_profile_view+17);}
#endif
    int32_t y0 = 6;
    for (i = 0; i < 16u && (remote?i<len:base+i<len); i++)     /* a parameter lock: a mark over the step */
        if (!remote && ((locks >> (base + i)) & 1u))
            cv_rect(41 + (int32_t)i * 12, 1, 8, 2, T_ACCENT);
    for (l = 0; l < NLANE; l++) {
        int32_t y = y0 + 4 + (int32_t)l * 14;
        int sel=remote?0:l==ui.lane;
#if FELUCCA_CAFA
        if(remote)sel=l==cafa_native_u14(cafa_native_profile_view+24)%NLANE;
#endif
        uint16_t row = sel ? T_RAISE : T_SURF;
        if (sel) {
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS, "drum lane fill centred");
            cv_rrect(6, y, 228, 14, 3, T_RAISE, T_SURF);   /* (6 .. 234, as a list row; the hits 40 .. 230) */
        }
        GFX_HOOK_ALIGN(0, y + 2, 0, y + 12, AL_V, "drum lane label on its hits' line");
        const char *lane_name=remote?"":drum_lane_abbr(t,l);
#if FELUCCA_CAFA
        char remote_lane[17];
        if(remote){const uint8_t *r=cafa_nv_row(l);if(r)cafa_native_text(remote_lane,r,16);else remote_lane[0]=0;lane_name=remote_lane;}
#endif
        cv_text_on(10, y - 1, &AF_S, lane_name, !remote && ((drum_mute[trk_index(t)] >> l) & 1u) ? T_DIM :   /* ((1.3) muted:
                                                                                                    * dim) */
                   sel ? T_ACCENT : T_MID, row);   /* ink rows 2 .. 11, as the hits */
        for (i = 0; i < 16u && (remote?i<len:base+i<len); i++) {
            const step_t *st=remote?0:&t->step[base+i];
            uint32_t b = 1u << l;
            uint32_t on,accent,ratchet;int32_t nudge=0;
#if FELUCCA_CAFA
            if(remote){const uint8_t *cell=cafa_nv_cell(l*16u+i);uint32_t flags=cell?cell[2]:0u;
                on=flags&1u;accent=flags&32u;ratchet=1u;
                if(cell && cell[3]>='2' && cell[3]<='4' && cell[4]==' ')ratchet=cell[3]-'0';
                if(cell)nudge=((int32_t)cafa_native_u14(cell)-8192)*12/16383;}
            else
#endif
            {on=(step_lanes(st)&b)!=0u;accent=(step_accents(st)&b)!=0u;ratchet=step_ratchet(st);nudge=st->nudge*12/64;}
            int32_t x=40+(int32_t)i*12,hx=clamp(x+nudge,40,220);
            if(on && ratchet>1u)
                pr_split(hx,y+2,10,10,accent?T_ACCENT:c,ratchet);
            else if(on && ux.crypt)
                cr_ch(hx+2,y+11,&AF_S,'#',accent?T_ACCENT:c);
            else if(on && ux.quark) {
                qk_disc(hx+5,y+7,4,accent?T_ACCENT:c);
                cv_rect(hx+3,y+5,1,1,ux_mix(accent?T_ACCENT:c,T_TEXT,60));
            } else if(on && ux.neon)
                ne_tube(hx,y+2,10,10,accent?T_ACCENT:c,row);
            else if(on)
                cv_rrect(hx,y+2,10,10,2,accent?T_ACCENT:c,row);
            else
                cv_rect(x + 4, y + 6, 2, 2, i % 4u == 0u || sel ? T_MID : T_DIM);
        }
        if (sel) {                                     /* the cursor ((2.0) the range): a 1 px frame ((Skiff N5) NEON: in
                                                       * the accent) */
            int32_t a=0,b=0;
#if FELUCCA_CAFA
            if(remote){uint32_t cursor=cafa_native_u14(cafa_native_profile_view+20);a=b=cursor>=base?(int32_t)(cursor-base):-1;}
            else
#endif
            {a=(int32_t)sel_lo()-(int32_t)base;b=(int32_t)sel_hi()-(int32_t)base;}
            a = a < 0 ? 0 : a;
            b = b > 15 ? 15 : b;
            if (a <= b && (remote?(uint32_t)a<len:base+(uint32_t)a<len))
                cv_frame(39 + a * 12, y + 1, (b - a) * 12 + 12, 12, ux.neon ? T_ACCENT : ux.crypt ? CR_GOLD : T_TEXT);
        }
    }
#if FELUCCA_CAFA
    if(remote){uint32_t play=cafa_native_u14(cafa_native_profile_view+22);
        if(play>=base && play-base<len && play-base<16u)cv_rect(40+(int32_t)(play-base)*12,y0-1,10,3,T_TEXT);}
    else
#endif
    if(song.playing && t->seq_idx<len && t->seq_idx/16u==ui.bank)
        cv_rect(40+(int32_t)(t->seq_idx%16u)*12,y0-1,10,3,T_TEXT);
}
/* SCL: the 12 keys as rounded bars (black keys high, white keys low): in the scale THEME, the root the
 * accent, out of the scale RAISE */
static void graph_scale(const track_t *t, uint16_t c)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
#if FELUCCA_CAFA
    if(cafa_native_profile_view) {
        uint32_t bytes,i;const uint8_t *keys=cafa_nv_section(6,&bytes);
        uint32_t root=keys?keys[19]%12u:0u;
        for(i=0;i<12u;i++) {
            uint32_t note=60u+i,active=keys && (keys[note/7u]&(1u<<(note%7u)));
            int32_t x=13+(int32_t)i*18;
            cv_rrect(x+3,BLACK[i]?6:38,10,52,3,active?(i==root?T_ACCENT:c):T_RAISE,T_SURF);
        }
        return;
    }
#endif
    uint32_t i, mask = scale_mask(t);
    for (i = 0; i < 12u; i++) {
        uint32_t deg = (i + 12u - (uint32_t)t->p[P_ROOT]) % 12u;
        int32_t x = 13 + (int32_t)i * 18;
        uint16_t col = (mask >> deg) & 1u ? (deg == 0u ? T_ACCENT : c) : T_RAISE;
        cv_rrect(x + 3, BLACK[i] ? 6 : 38, 10, 52, 3, col, T_SURF);
    }
}
/* CHORD (SCL 2): the chord's name ("Cm7": the last one the track played; before any, the one on its ROOT from
 * C4) over the keys from the C below its lowest note (two octaves, three for a wide one): its notes THEME, its
 * root's ACCENT, the others RAISE. OFF or a kit: what to do instead */
static void panel_note(const char *a, const char *b, const char *c);
static void panel_alert(uint32_t id, const char *a);
/* (2.5, the user's 7B) CHORD with STRUM on (a POLY track): the chord's name, then its notes in time: a row a note (low at
 * the bottom, named), its bar from when the strum plays it (0 .. 120 ms across), the root's ACCENT; the stroke a line
 * through their starts in the order played; NEON tubes, QUARK a particle and its trail, CRYPT # rows, the stroke in
 * blood. Top right "DOWN 40 ms" ("ALT: DOWN, THEN UP": the downstroke drawn) */
static void graph_strum(uint16_t c, const uint8_t *nn, uint32_t n, int32_t r,
                        const char *title, int32_t speed, int32_t direction)
{
    int32_t i, pitch, th, sx[CHORD_MAX], sy[CHORD_MAX], ms = clamp(speed, 0, 100), dir = clamp(direction, 0, 2);
    char b[24];
    cv_text_on(14, 8, &AF_M, title, T_TEXT, T_SURF);
    if (dir == 2)
        str_cpy(b, "ALT: DOWN, THEN UP", sizeof b);
    else {
        str_cpy(b, dir ? "UP " : "DOWN ", 8);
        fmt_int(b + str_len(b), ms);
        str_cpy(b + str_len(b), " ms", 4);
    }
    cv_text_r(226, 8 + AF_M.asc - AF_S.asc, &AF_S, b, T_MID, T_SURF);
    pitch = n > 1u ? 64 / ((int32_t)n - 1) : 0;      /* (rows centred 104 .. 40: the panel shows y up to 118) */
    pitch = pitch > 20 ? 20 : pitch;
    th = pitch >= 14 ? 10 : pitch - 4 < 4 ? 4 : pitch - 4;
    cv_rect(43, 30, 1, 82, T_LINE);                  /* (when the chord is played: 0 ms) */
    for (i = 0; i < (int32_t)n; i++) {
        int32_t o = dir == 1 ? (int32_t)n - 1 - i : i, d = n > 1u ? ms * o / ((int32_t)n - 1) : 0;
        int32_t y = 104 - i * pitch - th / 2, xs = 44 + d * 182 / 120, x;
        uint16_t col = (uint32_t)nn[i] % 12u == (uint32_t)r % 12u ? T_ACCENT : c;
        char nb[8];
        note_name(nb, nn[i]);
        cv_text(12, y + th - 1 - AF_S.asc, &AF_S, nb, T_MID);
        if (ux.crypt)
            for (x = xs; x + 6 <= 226; x += 7)
                cr_ch(x, y + th, &AF_S, '#', col);
        else if (ux.neon)
            ne_tube(xs, y, 226 - xs, th, col, T_SURF);
        else if (ux.quark) {
            cv_rect(xs + 6, y + th / 2, 226 - xs - 6, 1, ux_mix(col, T_SURF, 45));
            qk_disc(xs + 4, y + th / 2, 4, col);
        } else
            cv_rrect(xs, y, 226 - xs, th, 3, col, T_SURF);
        sx[o] = xs;
        sy[o] = y + th / 2;
    }
    for (i = 1; i < (int32_t)n; i++)                 /* (the stroke, in the order played) */
        cv_line_t(sx[i - 1], sy[i - 1], sx[i], sy[i], ux.crypt ? CR_BLOOD : ux.neon ? T_ACCENT : T_TEXT, 1);
}
#if FELUCCA_CAFA
/* Typed stock CHORD/FOLLOW rows are optional; a generic ROWS section keeps the older keyboard view. */
static int cafa_graph_chord_follow(uint16_t c, int follow)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint32_t bytes, i, n=0, hi=0, count, lo, by, mode, source;
    const uint8_t *rows=cafa_nv_section(CAFA_NATIVE_ROWS,&bytes), *r, *keys;
    char tag[17], title[25], caption[17];
    uint8_t nn[CHORD_MAX];
    if(!rows || (rows[0]!=1u && rows[0]!=2u))return 0;
    r=rows+1u;cafa_native_text(tag,r,16u);
    if(!str_eq(tag,follow?"FOLLOW VIEW V1":"CHORD VIEW V1"))return 0;
    mode=cafa_native_u14(r+41u);source=r[40];
    if(mode>4u || (follow && mode==4u) || source>cafa_native_u14(cafa_native_profile_view+15u) ||
       (mode==3u && rows[0]!=1u) || (mode!=3u && rows[0]!=2u))return 0;
    cafa_native_text(title,cafa_native_profile_view+57u,24u);
    if(mode<3u) {
        char hint[25];cafa_native_text(hint,rows+1u+48u+16u,24u);
        panel_note(title,hint[0]?hint:0,0);
        return 1;
    }
    keys=cafa_nv_section(CAFA_NATIVE_KEYBOARD,&bytes);
    if(!keys)return 0;
    for(i=0;i<128u;i++)if(keys[i/7u]&(1u<<(i%7u))) {
        hi=i;if(n<CHORD_MAX)nn[n++]=(uint8_t)i;
    }
    if(!n)return 0;
    lo=cafa_native_u14(r+43u);by=cafa_native_u14(r+45u);
    if(lo>127u || lo>hi || by>139u)return 0;
    if(mode==4u) {
        const uint8_t *detail=rows+1u+48u;
        if(cafa_native_u14(detail+41u)>100u || cafa_native_u14(detail+43u)>2u)return 0;
        graph_strum(c,nn,n,keys[19],title,(int32_t)cafa_native_u14(detail+41u),
                    (int32_t)cafa_native_u14(detail+43u));
        return 1;
    }
    cafa_native_text(caption,r+16u,16u);
    cv_text_on(14,8,&AF_M,title,T_TEXT,T_SURF);
    GFX_HOOK_ALIGN(0,0,0,8+AF_M.asc,AL_B,"chord caption on the name's baseline");
    cv_text_r(226,8+AF_M.asc-AF_S.asc,&AF_S,caption,T_MID,T_SURF);
    count=hi-lo<24u?24u:36u;
    GFX_HOOK_ALIGN(0,0,240,0,AL_H|AL_CELLS|AL_N(count),"chord keys centred");
    for(i=0;i<count;i++) {
        uint32_t note=lo+i,on=note<128u&&(keys[note/7u]&(1u<<(note%7u)));
        uint32_t w=216u/count;
        int32_t x=13+(int32_t)(i*w);
        uint16_t active=follow&&source?T_TRK(source-1u):c;
        cv_rrect(x,BLACK[note%12u]?36:62,(int32_t)w-2,48,w>6u?3:2,
                 !on?T_RAISE:follow?(by>=128u?note%12u==by-128u:note==by)?T_ACCENT:active:
                    note%12u==keys[19]%12u?T_ACCENT:c,T_SURF);
    }
    return 1;
}
#endif
static void graph_chord(const track_t *t, uint16_t c)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
#if FELUCCA_CAFA
    if(cafa_native_profile_view) {
        if(cafa_graph_chord_follow(c,0))return;
        const uint8_t *keys;uint32_t bytes,i,lo=48u,count=24u,root;
        char title[25];keys=cafa_nv_section(6,&bytes);
        if(keys) {
            for(i=0;i<128u;i++)if(keys[i/7u]&(1u<<(i%7u))){lo=i-i%12u;break;}
            root=keys[19];
        } else root=0u;
        cafa_native_text(title,cafa_native_profile_view+57,24);
        cv_text_on(14,8,&AF_M,title,T_TEXT,T_SURF);
        cv_text_r(226,8+AF_M.asc-AF_S.asc,&AF_S,cafa_native_profile_view[6]==14u?"FOLLOW":"CHORD",T_MID,T_SURF);
        for(i=0;i<count;i++) {
            uint32_t note=lo+i,on=keys && note<128u && (keys[note/7u]&(1u<<(note%7u)));
            int32_t x=13+(int32_t)(i*9u);
            cv_rrect(x,BLACK[note%12u]?36:62,7,48,2,!on?T_RAISE:note%12u==root%12u?T_ACCENT:c,T_SURF);
        }
        return;
    }
#endif
    uint8_t nn[CHORD_MAX];
    uint32_t n, i, j, k = trk_index(t), lo, cnt, w;
    int32_t r;
    uint16_t mask;
    char b[12];
    const char *cap = "LAST";
    if (!t->p[P_CHRD]) {
        panel_note("CHORD KEYS OFF", "[K1] DIA3: IN-KEY CHORDS", 0);   /* (the title says what they are) */
        return;
    }
    if (chord_kit(t)) {
        panel_note("NO CHORDS ON KITS", 0, 0);
        return;
    }
    if (chord_last[k].n) {
        r = chord_last[k].root;
        mask = chord_last[k].mask;
        n = chord_last[k].n;
        for (i = 0; i < n; i++)
            nn[i] = chord_last[k].note[i];
    } else {
        n = chord_make(t, (uint32_t)(60 + t->p[P_ROOT]), nn, &r, &mask);
        cap = "ON ROOT";
    }
    if (trk_vmode(t) != V_POLY && !t->p[P_AMODE])
        cap = "ROOT ONLY";                              /* MONO / LEGATO / UNISON ((2.0.3) the ARP takes it whole) */
    else if (t->p[P_STRUM] && trk_vmode(t) == V_POLY) {   /* ((2.5) STRUM on: the notes in time) */
        chord_name(b, (uint32_t)r, mask);
        graph_strum(c, nn, n, r, b, t->p[P_STRUM], t->p[P_SDIR]);
        return;
    }
    chord_name(b, (uint32_t)r, mask);
    cv_text_on(14, 8, &AF_M, b, T_TEXT, T_SURF);
    GFX_HOOK_ALIGN(0, 0, 0, 8 + AF_M.asc, AL_B, "chord caption on the name's baseline");
    cv_text_r(226, 8 + AF_M.asc - AF_S.asc, &AF_S, cap, T_MID, T_SURF);   /* on the name's baseline */
    lo = nn[0] - nn[0] % 12u;
    cnt = nn[n - 1u] - lo < 24u ? 24u : 36u;
    w = 216u / cnt;
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS | AL_N(cnt), "chord keys centred");
    for (i = 0; i < cnt; i++) {
        uint32_t note = lo + i, on = 0;
        int32_t x = 13 + (int32_t)(i * w);              /* (216 px of keys and their gaps: x 13 .. 227) */
        for (j = 0; j < n; j++)
            on |= nn[j] == note;
        cv_rrect(x, BLACK[note % 12u] ? 36 : 62, (int32_t)w - 2, 48, w > 6u ? 3 : 2,
                 !on ? T_RAISE : note % 12u == (uint32_t)r % 12u ? T_ACCENT : c, T_SURF);
    }
}
/* (2.1) FOLLOW (SCL 3): the chord of the track it follows, as CHORD draws one: its name over the keys from the C below
 * its lowest note, its notes in that track's colour, the note this track goes by (NOTE) ACCENT; on the name's baseline
 * what this track does with it ("T2 KEY +5", "T2 SCALE -2", "T2 FIT"). OFF, a kit to follow, no chord yet: notes */
static void graph_follow(const track_t *t)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
#if FELUCCA_CAFA
    if(cafa_native_profile_view){if(!cafa_graph_chord_follow(T_THEME,1))graph_chord(0,T_THEME);return;}
#endif
    uint32_t l = (uint32_t)t->p[P_FOLLOW] - 1u, i, lo, cnt, w, by, rel;
    const fol_src_t *s;
    char b[12], cap[24];
    if (l >= NTRK || l == trk_index(t)) {
        panel_note("FOLLOW OFF", "[K1] A TRACK TO FOLLOW", 0);
        return;
    }
    s = &fol_src[l];
    str_cpy(cap, N_DKSRC[l + 1u], sizeof cap);
    if (chord_kit(&trk[l])) {
        str_cpy(cap + str_len(cap), " PLAYS A KIT", sizeof cap - str_len(cap));
        panel_note(cap, "NO CHORDS TO FOLLOW", 0);
        return;
    }
    if (!s->pc) {
        str_cpy(cap + str_len(cap), ": NO CHORD YET", sizeof cap - str_len(cap));
        panel_note(cap, "PLAYS AS WRITTEN", 0);
        return;
    }
    rel = (s->pc >> s->root | (uint32_t)s->pc << (12u - s->root)) & 0xFFFu;   /* (its notes above its root) */
    if (rel == 1u)
        str_cpy(b, N_NOTE[s->root % 12u], sizeof b);   /* (a single note: its name) */
    else
        chord_name(b, s->root, rel);
    cv_text_on(14, 8, &AF_M, b, T_TEXT, T_SURF);
    str_cpy(cap + str_len(cap), " ", sizeof cap - str_len(cap));
    str_cpy(cap + str_len(cap), N_FMODE[t->p[P_FMODE] % 3], sizeof cap - str_len(cap));
    if (t->p[P_FMODE] != FM_FIT) {
        int32_t sh = fol_shift(t, s);
        str_cpy(cap + str_len(cap), sh > 0 ? " +" : " ", sizeof cap - str_len(cap));
        fmt_int(cap + str_len(cap), sh);
    }
    GFX_HOOK_ALIGN(0, 0, 0, 8 + AF_M.asc, AL_B, "follow caption on the name's baseline");
    cv_text_r(226, 8 + AF_M.asc - AF_S.asc, &AF_S, cap, T_MID, T_SURF);
    by = t->p[P_FMODE] == FM_FIT || t->p[P_FNOTE] == FN_ROOT ? 128u + s->root :   /* (the note it goes by: 128 + a */
         t->p[P_FNOTE] == FN_LOW ? s->lo : t->p[P_FNOTE] == FN_HIGH ? s->hi : s->last;   /* pitch class, every octave) */
    lo = s->lo - s->lo % 12u;
    cnt = s->hi - lo < 24u ? 24u : 36u;
    w = 216u / cnt;
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS | AL_N(cnt), "follow keys centred");
    for (i = 0; i < cnt; i++) {
        uint32_t note = lo + i, on = note >= s->lo && note <= s->hi && ((s->pc >> (note % 12u)) & 1u);
        int32_t x = 13 + (int32_t)(i * w);
        cv_rrect(x, BLACK[note % 12u] ? 36 : 62, (int32_t)w - 2, 48, w > 6u ? 3 : 2,
                 !on ? T_RAISE : (by >= 128u ? note % 12u == by - 128u : note == by) ? T_ACCENT : T_TRK(l), T_SURF);
    }
}
/* the four sends as faders under their cards: a RAISE slot, the THEME fill from the bottom, a cap */
static void graph_fx(const track_t *t, uint16_t c)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
#if FELUCCA_CAFA
        if(cafa_native_profile_view) {
            const uint8_t *p=cafa_native_profile_view+112u+36u*i;
            int32_t h=(int32_t)cafa_native_u14(p+30)*76/16383,x=CARD_X(i)+26;
            cv_rrect(x,6,4,82,2,T_RAISE,T_SURF);
            if(ux.crypt)cr_meter(x-1,88,h,!h);
            else if(ux.quark)qk_tower(x-3,88,10,80,h,c,T_ACCENT);
            else if(ux.neon)ne_vu(x-3,88,10,80,h,0,ux_mix(T_SURF,T_TEXT,8),T_TEXT);
            else {if(h>3)cv_rrect(x,88-h,4,h,2,c,T_RAISE);cv_rrect(x-4,85-h,12,6,3,c,T_SURF);}
            continue;
        }
#endif
        int32_t h = t->p[P_DIST + i] * 76 / 127, x = CARD_X(i) + 26;
        cv_rrect(x, 6, 4, 82, 2, T_RAISE, T_SURF);
        if (ux.crypt) {                              /* (Skiff) CRYPT: the send a torch (the MIXER's) */
            cr_meter(x - 1, 88, h, !h);
            continue;
        }
        if (ux.quark) {                              /* (Skiff Q4) QUARK: a calorimeter tower (the MIXER's) */
            qk_tower(x - 3, 88, 10, 80, h, c, T_ACCENT);
            continue;
        }
        if (ux.neon) {                               /* (Skiff N4) NEON: a VU column (the MIXER's), its cap white */
            ne_vu(x - 3, 88, 10, 80, h, 0, ux_mix(T_SURF, T_TEXT, 8), T_TEXT);
            continue;
        }
        if (h > 3)
            cv_rrect(x, 88 - h, 4, h, 2, c, T_RAISE);
        cv_rrect(x - 4, 85 - h, 12, 6, 3, c, T_SURF);
    }
}
/* (Skiff U3, R6) EDIT > LANES (DRUM): the eight lane levels as faders (FX's), the lane KNOB 1 picks white, the lanes'
 * names under them as the grid writes them (BD SD ..), the picked one's on a white tile. Lane i's column at x 3 + 29 i:
 * its fader's track len px up to y1 at x + 12, the level h px; its name's tile at y */
static void lane_fader(int32_t x, int32_t y1, int32_t len, int32_t h, int sel, uint16_t c)
{
    uint16_t col = sel ? T_THEME : c;
    cv_rrect(x + 12, y1 - len, 4, len, 2, T_RAISE, T_SURF);
    if (ux.crypt)                                    /* (Skiff) CRYPT: the level a torch (the MIXER's) */
        cr_meter(x + 11, y1, h, !h);
    else if (ux.quark)                               /* (Skiff Q4) QUARK: a calorimeter tower, the picked lane's capped */
        qk_tower(x + 9, y1, 10, len, h, c, sel ? T_ACCENT : 0);
    else if (ux.neon)                                /* (Skiff N4) NEON: a VU column, the picked lane's cap the accent */
        ne_vu(x + 9, y1, 10, len, h, 0, ux_mix(T_SURF, T_TEXT, 8), sel ? T_ACCENT : T_TEXT);
    else {
        if (h > 3)
            cv_rrect(x + 12, y1 - h, 4, h, 2, col, T_RAISE);
        cv_rrect(x + 8, y1 - 3 - h, 12, 6, 3, col, T_SURF);
    }
}
/* (2.0, #35, design/mockups/v2_kits.png A) .. a lane of a user kit with no sample: its track empty, its name DIM */
static void lane_fader_of(const track_t *t, uint32_t i, int32_t x, int32_t y1, int32_t len, int32_t h, int sel, uint16_t c)
{
    if (drum_lane_has(t, i))
        lane_fader(x, y1, len, h, sel, c);
    else
        cv_rrect(x + 12, y1 - len, 4, len, 2, T_RAISE, T_SURF);
}
static void lane_tile(const track_t *t, uint32_t i, int32_t x, int32_t y, int sel)
{
    if (sel)
        cv_rect(x + 2, y, 24, 16, T_THEME);
    GFX_HOOK_ALIGN(x + 2, y, x + 26, y + 16, AL_HV, "lane name centred under its fader");
    cv_text_in(x + 2, y + CAP_IN(S, 16), 24, &AF_S, drum_lane_abbr(t, i), sel ? T_INK : drum_lane_has(t, i) ? T_MID : T_DIM,
               sel ? T_THEME : T_SURF);
}
static void graph_lanes(const track_t *t, uint16_t c)
{
    uint32_t i;
    for (i = 0; i < 8u; i++) {
        int32_t x = 3 + 29 * (int32_t)i;
#if FELUCCA_CAFA
        if(cafa_native_profile_view) {
            const uint8_t *r=cafa_nv_row(i);char name[17];
            uint32_t sel=cafa_native_u14(cafa_native_profile_view+24)==i;
            int32_t h=r?(int32_t)cafa_native_u14(r+41)*70/16383:0;
            if(r)cafa_native_text(name,r,16);else name[0]=0;
            lane_fader(x,82,76,h,(int)sel,c);
            if(sel)cv_rect(x+2,90,24,16,T_THEME);
            cv_text_in(x+2,90+CAP_IN(S,16),24,&AF_S,name,sel?T_INK:T_MID,sel?T_THEME:T_SURF);
            continue;
        }
#endif
        int sel = i == ui.lane % 8u;
        lane_fader_of(t, i, x, 82, 76, t->p[P_LN0 + i] * 70 / 127, sel, c);
        lane_tile(t, i, x, 90, sel);
    }
}
/* PAN as the MIXER writes it: C, L31, R100 (params.c F_BIPCT's); v holds 6 */
static void pan_txt(char *v, int32_t pan)
{
    if (!pan) {
        v[0] = 'C';
        v[1] = 0;
        return;
    }
    v[0] = pan < 0 ? 'L' : 'R';
    fmt_int(v + 1, pan < 0 ? -pan * 100 / 64 : pan * 100 / 63);   /* (R100 at the top) */
}
/* (2.0, #36, the user's pick 1A) EDIT > LANE MIX (DRUM, DRUM-X): LANES' faders 20 px shorter, under each its PAN: a
 * row 20 px from x + 4, C its middle, the pan a mark on it, and as the MIXER writes it; the names 2 px lower
 * (design/mock_v2_feats.py lanes_graph). CRYPT: a skull on a dotted leader; NEON: a spark on a glass line; QUARK: a
 * nucleus on its energy levels */
static void graph_lmix(const track_t *t, uint16_t c)
{
    uint32_t i, l = 0u;
#if FELUCCA_CAFA
    if(cafa_native_profile_view)l=cafa_native_u14(cafa_native_profile_view+24)%8u;
    else
#endif
    l=page_lane();
    for (i = 0; i < 8u; i++) {
        int32_t x = 3 + 29 * (int32_t)i, pan = 0, px, k;
        int sel = i == l;
        uint16_t col = sel ? T_THEME : c;
        char v[6];
#if FELUCCA_CAFA
        if(cafa_native_profile_view) {
            const uint8_t *r=cafa_nv_row(i);char name[17];
            int32_t h=r?(int32_t)cafa_native_u14(r+41)*53/16383:0;
            pan=r?(int32_t)cafa_native_u14(r+43)*127/16383-64:0;
            px=x+13+pan*9/64;
            if(r)cafa_native_text(name,r,16);else name[0]=0;
            lane_fader(x,62,56,h,sel,c);
            cv_rect(x+4,71,20,2,T_RAISE);cv_rect(x+13,69,1,7,T_RAISE);cv_rect(px-2,69,5,7,col);
            pan_txt(v,pan);
            cv_text_in(x,78+CAP_IN(S,12),28,&AF_S,v,sel?T_TEXT:T_MID,T_SURF);
            if(sel)cv_rect(x+2,92,24,16,T_THEME);
            cv_text_in(x+2,92+CAP_IN(S,16),24,&AF_S,name,sel?T_INK:T_MID,sel?T_THEME:T_SURF);
            continue;
        }
#endif
        pan=clamp(t->p[P_LPAN0+i],-64,63);px=x+13+pan*9/64;
        lane_fader_of(t, i, x, 62, 56, t->p[P_LN0 + i] * 53 / 127, sel, c);   /* (100 %: the cap on the track's top) */
        if (ux.crypt) {
            cr_leader(x + 4, x + 24, 72);
            cr_skull(px - 4, 68, col, T_SURF, 0);
        } else if (ux.neon) {
            cv_rect(x + 4, 72, 20, 1, ux_mix(T_SURF, T_TEXT, 25));
            ne_spark(px, 72, col, 1);
        } else if (ux.quark) {
            for (k = 0; k < 20; k += 4)
                cv_rect(x + 4 + k, 72, 2, 2, ux_mix(T_SURF, T_TEXT, 12));
            qk_nucleus(px, 72, col, 5);
        } else {
            cv_rect(x + 4, 71, 20, 2, T_RAISE);
            cv_rect(x + 13, 69, 1, 7, T_RAISE);
            cv_rect(px - 2, 69, 5, 7, col);
        }
        pan_txt(v, pan);
        GFX_HOOK_ALIGN(x, 0, x + 28, 0, AL_H, "lane pan centred under its fader");
        cv_text_in(x, 78 + CAP_IN(S, 12), 28, &AF_S, v, sel ? T_TEXT : T_MID, T_SURF);
        lane_tile(t, i, x, 92, sel);
    }
}
/* SLICER page: the pattern's 16 steps, a 'x' step a full bar; a '.' step: GATE a bar as high as it stays
 * open (DEPTH), STUT hatched (it repeats the last 'x'); the step playing underlined. Grey when OFF. */
static void graph_slicer(const track_t *t, uint16_t c)
{
    int remote=0;
#if FELUCCA_CAFA
    if(cafa_native_profile_view)remote=1;
#endif
    uint32_t i,pat=remote?0u:sl_pattern(t),mode=remote?SL_GATE:(uint32_t)t->p[P_SLCR],cur=remote?0u:sl[t-trk].idx;
    int32_t open=remote?0:70-t->p[P_SLDEPTH]*70/127;
#if FELUCCA_CAFA
    if(remote){uint32_t at=cafa_native_u14(cafa_native_profile_view+22),first=cafa_native_u14(cafa_native_profile_view+17);
        cur=at>=first && at-first<16u?at-first:16u;}
#endif
    for (i = 0; i < 16u; i++) {
        int32_t x = bar_x(i), y;
        uint32_t hit=(pat>>i)&1u;int32_t height=open;
#if FELUCCA_CAFA
        if(remote){const uint8_t *cell=cafa_nv_cell(i);uint32_t flags=cell?cell[2]:0u;
            hit=flags&1u;mode=(flags&64u)?SL_OFF:(flags&16u)?SL_STUT:SL_GATE;
            height=cell?(int32_t)cafa_native_u14(cell)*70/16383:0;}
#endif
        uint16_t col=mode==SL_OFF?T_DIM:c;
        if (ux.crypt) {                              /* (Skiff) CRYPT: # columns: an 'x' step full, a GATE step ':' as
                                                       * high as it stays open, STUT '=' (it repeats the last 'x') */
            int32_t n=hit || mode==SL_STUT?8:(height+4)/9,r;
            for (r = 0; r < n; r++)
                cr_ch(x + 2, 79 - r * 9, &AF_S, hit ? '#' : mode == SL_STUT ? '=' : ':', hit || mode == SL_STUT ? col : T_DIM);
        } else if (ux.quark && mode != SL_STUT) {    /* (Skiff Q4) QUARK: towers: an 'x' step full, a GATE step as high as
                                                      * it stays open, dim */
            qk_tower(x, 80, 9, 72, hit ? 72 : height, hit ? col : T_DIM, 0);
        } else if (hit) {
            if (ux.neon && mode != SL_OFF)           /* ((Skiff N4) NEON: a lit tube, its glow) */
                cv_rect(x - 1, 7, 11, 74, ux_mix(col, T_SURF, 65));
            cv_rrect(x, 8, 9, 72, 2, mode == SL_OFF ? T_RAISE : col, T_SURF);
        } else if (mode == SL_STUT) {
            for (y = 8; y < 80; y += 4)
                cv_rect(x, y, 9, 1, col);
        } else {
            cv_rrect(x, 77, 9, 3, 1, T_RAISE, T_SURF);
            if (height > 3)
                cv_rrect(x, 80 - height, 9, height, 2, T_RAISE, T_SURF);
        }
        if (mode != SL_OFF && i == cur)
            cv_rect(x, 84, 9, 2, T_ACCENT);
    }
}
/* (Skiff) DUCK page (duck.c): the track's level over one bar of its source's steps (four beats; the source's DIV,
 * its pattern looped, no swing): full up to the box's top, down by DEPTH at each hit (the source's colour under it,
 * a DRUM source's KICK lane only), back in a line over RELEASE; a hit's dip from the bar before runs into the bar's
 * start. BY OFF (or the track itself): the empty box */

/* MOD page: the four slots as rows "1 LFO > CUT +50%", the one KNOB 2..4 edit selected, slots that do
 * nothing (SRC, DST or AMT at 0) dim */
/* MOD ((2.0, D 1A) design/mockups/v2_mod.png): the seven fixed routings as a grid, the LFO and the ENV a row each, PITCH
 * CUT SHAPE AMP a column (the ENV's AMP is the envelope itself: -), the route picked white; the matrix's four slots under
 * it as routes 8 .. 11 (SOURCE -> DEST AMOUNT), the picked one in the theme's band. A route doing nothing: DIM (a zero,
 * mod_dead, a slot without source, place or amount) */
static void graph_mod(const track_t *t, uint16_t c)
{
    static const char *const COL[4] = {"PITCH", "CUT", "SHAPE", "AMP"};
    static const uint8_t CX[4] = {84, 124, 166, 208};
#if FELUCCA_CAFA
    if(cafa_native_profile_view) {
        uint32_t q,k,selected=cafa_native_u14(cafa_native_profile_view+24);
        for(k=0;k<4u;k++) {
            const uint8_t *r=cafa_nv_row(k);char label[17];
            if(r)cafa_native_text(label,r,16);else label[0]=0;
            cv_text_on(CX[k]-text_w(&AF_S,label)/2,16-AF_S.asc,&AF_S,label,T_MID,T_SURF);
        }
        for(q=0;q<8u;q++) {
            const uint8_t *r=cafa_nv_row(q);char value[17];
            int32_t base=35+21*(int32_t)(q/4u),x=CX[q%4u];
            int sel=q==selected;
            if(q%4u==0u)cv_text_on(12,base-AF_S.asc,&AF_S,q?"ENV":"LFO",T_TEXT,T_SURF);
            if(r)cafa_native_text(value,r+16,16);else value[0]=0;
            if(sel)cv_rect(x-19,base-14,39,19,T_THEME);
            cv_text_on(x-text_w(&AF_M,value)/2,base-AF_M.asc,&AF_M,value,sel?T_INK:r && !(r[40]&16u)?c:T_DIM,
                       sel?T_THEME:T_SURF);
        }
        cv_rect(8,64,224,1,T_LINE);
        for(k=0;k<4u;k++) {
            const uint8_t *r=cafa_nv_row(8u+k);char source[17],dest[17],amount[9],num[8];
            int32_t y=67+18*(int32_t)k,sel=8u+k==selected;
            uint16_t bg=sel?T_THEME:T_SURF,fg=sel?T_INK:r && !(r[40]&16u)?c:T_DIM;
            if(r){cafa_native_text(source,r,16);cafa_native_text(dest,r+16,16);cafa_native_text(amount,r+32,8);}
            else{source[0]=dest[0]=amount[0]=0;}
            if(sel)cv_rrect(6,y,228,17,4,T_THEME,T_SURF);
            fmt_int(num,(int32_t)k+8);
            cv_text_on(12,y+13-AF_S.asc,&AF_S,num,sel?T_INK:T_MID,bg);
            cv_free_text(34,y+13-AF_S.asc,&AF_S,source,fg,bg,44);
            cv_line(78,y+8,95,y+8,fg);cv_line(91,y+4,95,y+8,fg);cv_line(91,y+12,95,y+8,fg);
            cv_free_text(102,y+13-AF_S.asc,&AF_S,dest,fg,bg,78);
            cv_text_r(228,y+13-AF_S.asc,&AF_S,amount,fg,bg);
        }
        return;
    }
#endif
    uint32_t k, q;
    char b[16];
    const char *unit;
    for (k = 0; k < 4u; k++)
        cv_text_on(CX[k] - text_w(&AF_S, COL[k]) / 2, 16 - AF_S.asc, &AF_S, COL[k], T_MID, T_SURF);
    for (q = 0; q < 8u; q++) {                          /* (q: the route, row by row; 7, ENV to AMP: -) */
        int32_t base = 35 + 21 * (int32_t)(q / 4u), x = CX[q % 4u], v = q < MOD_SLOT0 ? t->p[MOD_FIXED[q]] : 0;
        uint32_t sel = q < MOD_SLOT0 && q == mod_ui_route;   /* ((2.0.2, #61) route 8 is the first slot's, not the -) */
        if (q % 4u == 0u)
            cv_text_on(12, base - AF_S.asc, &AF_S, q ? "ENV" : "LFO", T_TEXT, T_SURF);
        if (q < MOD_SLOT0)
            param_format(&TP[MOD_FIXED[q]], v, b, &unit);
        else
            str_cpy(b, "-", sizeof b);
        if (sel)
            cv_rect(x - 19, base - 14, 39, 19, T_THEME);
        cv_text_on(x - text_w(&AF_M, b) / 2, base - AF_M.asc, &AF_M, b,
                   sel ? T_INK : !v || mod_dead(t, q) ? T_DIM : c, sel ? T_THEME : T_SURF);
    }
    cv_rect(8, 64, 224, 1, T_LINE);
    for (k = 0; k < NMSLOT; k++) {
        const int16_t *p = &t->p[P_M1SRC + 3u * k];
        int32_t y = 67 + 18 * (int32_t)k, on = p[0] && p[1] && p[2], sel = MOD_SLOT0 + k == mod_ui_route, n;
        uint16_t bg = sel ? T_THEME : T_SURF, col = sel ? T_INK : on ? c : T_DIM, ar = sel ? T_INK : on ? T_TEXT : T_DIM;
        if (sel)
            cv_rrect(6, y, 228, 17, 4, T_THEME, T_SURF);
        fmt_int(b, (int32_t)(MOD_SLOT0 + k) + 1);
        cv_text_on(12, y + 13 - AF_S.asc, &AF_S, b, sel ? T_INK : T_MID, bg);
        cv_text_on(34, y + 13 - AF_S.asc, &AF_S, N_MSRC[clamp(p[0], 0, MS_N - 1)], col, bg);
        cv_line(78, y + 8, 95, y + 8, ar);              /* an arrow */
        cv_line(91, y + 4, 95, y + 8, ar);
        cv_line(91, y + 12, 95, y + 8, ar);
        cv_text_on(102, y + 13 - AF_S.asc, &AF_S, mod_dst_name(t, p[1]), col, bg);
        param_format(&TP[P_M1AMT], p[2], b, &unit);
        n = (int32_t)str_len(b);
        b[n] = ' ';
        str_cpy(b + n + 1, unit, sizeof b - (uint32_t)n - 1u);
        cv_text_on(204 - text_w(&AF_S, b), y + 13 - AF_S.asc, &AF_S, b, col, bg);
    }
}

static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &t->step[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
        h = (h ^ (st->hit | (uint32_t)st->acc << 8 | (uint32_t)st->note[2] << 16 | (uint32_t)st->note[3] << 24)) * 16777619u;
        h = (h ^ (uint8_t)st->nudge) * 16777619u;   /* ((Skiff) the roll and the grid draw it) */
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

/* One shared, reduced sample display. Decode only in the UI/main loop with a
 * private IMA state, 512 samples per frame; no audio voice state is touched.
 * Cache follows the sample zone, selected note and user-slot generation. */
#define SAMPLE_WAVE_COLS 96u
static struct {
    int16_t lo[SAMPLE_WAVE_COLS], hi[SAMPLE_WAVE_COLS];
    const smp_zone_t *zone;
    uint32_t key, gen, pos;
    int32_t pred, index, peak;
    uint8_t ready;
} sample_wave;

static uint32_t sample_wave_zone(const track_t *t)
{
    uint32_t si = (uint32_t)t->p[P_E0] % SMP_NALL, i, zi = 0xFFFFu, k;
    if (cur_page()->graph == GR_SMIX && (i = smx_zone(&k)) != 0xFFu)   /* ((#21) SAMPLES: the one picked) */
        return 0x8000u | k << 5 | i;
    if (si < SMP_NSETS) {
        const smp_set_t *set = &SMP_SETS[si];
        for (i = 0; i < set->nz; i++)
            if (last_note >= SMP_ZONES[set->z0 + i].lo && last_note <= SMP_ZONES[set->z0 + i].hi) zi = set->z0 + i;
        if (zi == 0xFFFFu && set->nz) zi = set->z0;
    } else {
        uint32_t k = si - SMP_NSETS;
        for (i = 0; i < usr_nz[k]; i++)
            if (last_note >= usr_zone[k][i].lo && last_note <= usr_zone[k][i].hi) zi = 0x8000u | k << 5 | i;
        if (zi == 0xFFFFu && usr_nz[k]) zi = 0x8000u | k << 5;
    }
    return zi;
}

static void sample_wave_tick(const track_t *t)
{
    uint32_t zi = sample_wave_zone(t), key = zi + ((uint32_t)last_note << 16), i;
    if (key != sample_wave.key || sample_wave.gen != smp_user_gen || !sample_wave.zone) {
        sample_wave.key = key; sample_wave.gen = smp_user_gen;
        sample_wave.zone = zi == 0xFFFFu ? 0 : smp_zone(zi);
        sample_wave.pos = 0; sample_wave.pred = sample_wave.index = 0;
        sample_wave.peak = 1; sample_wave.ready = 0;
        for (i = 0; i < SAMPLE_WAVE_COLS; i++) sample_wave.lo[i] = sample_wave.hi[i] = 0;
    }
    const smp_zone_t *z = sample_wave.zone;
    if (!z || !z->n || sample_wave.ready) return;
    for (i = 0; i < 512u && sample_wave.pos < z->n; i++) {
        uint32_t pos = sample_wave.pos, b = SMP_DATA[z->off + (pos >> 1)];
        uint32_t code = (pos & 1u) ? b >> 4 : b & 15u;
        int32_t step = IMA_STEP[sample_wave.index], d = step >> 3;
        if (code & 4u) d += step;
        if (code & 2u) d += step >> 1;
        if (code & 1u) d += step >> 2;
        sample_wave.pred = clamp(sample_wave.pred + ((code & 8u) ? -d : d), -32768, 32767);
        sample_wave.index = clamp(sample_wave.index + IMA_IDX[code & 7u], 0, 88);
        uint32_t col = pos * SAMPLE_WAVE_COLS / z->n;
        int32_t v = sample_wave.pred, a = v < 0 ? -v : v;
        if (v < sample_wave.lo[col]) sample_wave.lo[col] = (int16_t)v;
        if (v > sample_wave.hi[col]) sample_wave.hi[col] = (int16_t)v;
        if (a > sample_wave.peak) sample_wave.peak = a;
        sample_wave.pos++;
    }
    sample_wave.ready = sample_wave.pos == z->n;
}

/* A waveform envelope uses two points at each x: low, then high. Legacy
 * single-value curves keep their original rendering. */
#if FELUCCA_CAFA
static int graph_wave_pairs(const uint8_t *curve, uint32_t n)
{
    uint32_t i;
    if(n<4u || (n&1u))return 0;
    for(i=0;i<n;i+=2u)
        if(cafa_native_u14(curve+2u+4u*i)!=cafa_native_u14(curve+6u+4u*i))return 0;
    return 1;
}
#endif
static void graph_sample(uint16_t c)
{
#if FELUCCA_CAFA
    if(cafa_native_profile_view) {
        const uint8_t *curve,*markers;uint32_t bytes,n,i,paired;
        curve=cafa_nv_section(3,&bytes);
        cv_rect(PANEL_X0,48,PANEL_W,1,T_RAISE);
        if(!curve){panel_note("NO SAMPLE",0,0);return;}
        n=cafa_native_u14(curve);
        paired=graph_wave_pairs(curve,n);
        for(i=0;i<n;i+=paired?2u:1u) {
            int32_t x=12+(int32_t)cafa_native_u14(curve+2u+4u*i)*216/16383;
            int32_t y=paired?48-((int32_t)cafa_native_u14(curve+4u+4u*i)-8192)*38/8192:48-(int32_t)cafa_native_u14(curve+4u+4u*i)*38/16383;
            int32_t y2=paired?48-((int32_t)cafa_native_u14(curve+8u+4u*i)-8192)*38/8192:48;
            cv_line(x,y,x,y2,c);
        }
        markers=cafa_nv_section(4,&bytes);
        if(markers)for(i=0;i<markers[0];i++) {
            int32_t x=12+(int32_t)cafa_native_u14(markers+1u+13u*i)*216/16383;
            cv_line(x,4,x,90,T_MID);
        }
        return;
    }
#endif
    uint32_t i;
    const smp_zone_t *z = sample_wave.zone;
    cv_rect(PANEL_X0, 48, PANEL_W, 1, T_RAISE);
    for (i = 0; i < SAMPLE_WAVE_COLS; i++) {
        int32_t x = 12 + (int32_t)i * 216 / (SAMPLE_WAVE_COLS - 1u);
        cv_line(x, 48 - sample_wave.hi[i] * 38 / sample_wave.peak,
                x, 48 - sample_wave.lo[i] * 38 / sample_wave.peak, c);
    }
    if (z->looped && TSEL->p[P_E3]) {
        int32_t left = 12 + (int32_t)(z->ls * 216u / z->n), right = 12 + (int32_t)(z->le * 216u / z->n);
        cv_line(left, 4, left, 90, T_MID); cv_line(right, 4, right, 90, T_MID);
    }
}

#if FELUCCA_CAFA
static void cafa_nv_slices(void)
{
    const uint8_t *curve,*markers;uint32_t bytes,mb,n,i,paired,selected=cafa_native_u14(cafa_native_profile_view+24);
    const int32_t width=216;
    curve=cafa_nv_section(3,&bytes);markers=cafa_nv_section(4,&mb);
    if(!curve){panel_alert(ICON_X_NOFILE,"SAMPLE NOT FOUND");return;}
    n=cafa_native_u14(curve);paired=graph_wave_pairs(curve,n);
    for(i=0;i<n;i+=paired?2u:1u) {
        int32_t x=12+(int32_t)cafa_native_u14(curve+2u+4u*i)*width/16383;
        int32_t v=(int32_t)cafa_native_u14(curve+4u+4u*i)-8192;
        int32_t v2=paired?(int32_t)cafa_native_u14(curve+8u+4u*i)-8192:0;
        cv_line(x,46-v*38/8192,x,46-v2*38/8192,T_THEME);
    }
    if(markers) {
        uint32_t count=markers[0], flagged=0;
        for(i=0;i<count;i++) flagged|=markers[1u+13u*i+4u]&1u;
        for(i=0;i<count;i++) {
            int chosen=flagged?(markers[1u+13u*i+4u]&1u)!=0:i==selected;
            int32_t x=12+(int32_t)cafa_native_u14(markers+1u+13u*i)*width/16383;
            cv_rect(x,chosen?2:4,chosen?2:1,chosen?88:84,chosen?T_ACCENT:T_MID);
        }
        cv_rrect(12,96,width,3,1,T_RAISE,T_SURF);
        for(i=0;i<count;i++) {
            int chosen=flagged?(markers[1u+13u*i+4u]&1u)!=0:i==selected;
            int32_t x=12+(int32_t)cafa_native_u14(markers+3u+13u*i)*(width-1)/16383;
            cv_rect(x,94,1,7,chosen?T_ACCENT:T_MID);
        }
    }
    {
        const uint8_t *detail = cafa_nv_row(0);
        if (detail) {
            char length[24], source[12];
            cafa_native_text(length, detail + 16u, 16u);
            cafa_native_text(source, detail + 32u, 8u);
            cv_text(12, 106, &AF_S, "LEN", T_MID);
            cv_text(38, 106, &AF_S, length, T_MID);
            cv_text_r(228, 106, &AF_S, source, T_THEME, T_SURF);
        }
    }
}
#endif
#if FELUCCA_SLICE
/* SLICES (ui_slice.c): the waveform from the marker before the selected one to the one after it (the selected
 * slice tinted, its marker the accent, outside the slices DIM), under it the whole material with every marker and
 * the view; then the selected slice's length and the source */
static void graph_slices(void)
{
#if FELUCCA_CAFA
    if(cafa_native_profile_view){cafa_nv_slices();return;}
#endif
    uint32_t a, b, len, n = slice_count(), j = slice_sel(), i, src, div, ma, mb;
    char t[20], u[12];
    if (!slice_src(&src, &div) || !n) {             /* (it plays a sine: eng_slice.c) */
        panel_alert(ICON_X_NOFILE, "SAMPLE NOT FOUND");
        return;
    }
    slice_view(&a, &b, &len);
    slice_env(a, b);
    ma = slice_mark(0);
    mb = slice_mark(n);
#define SPX(p) (12 + (int32_t)((uint32_t)((p) - a) * SP_COLS / (b - a)))   /* p in [a, b] */
    if (j < n) {                                     /* the selected slice */
        uint32_t s0 = slice_mark(j), s1 = slice_mark(j + 1u);
        int32_t x0 = SPX(s0 > a ? s0 : a), x1 = SPX(s1 < b ? s1 : b);
        if (x1 > x0)
            cv_rect(x0, 6, x1 - x0, 80, T_TINT);
    }
    for (i = 0; i < SP_COLS; i++) {
        uint32_t p = a + (uint32_t)((uint32_t)i * (b - a) / SP_COLS);
        cv_line(12 + (int32_t)i, 46 - sp.hi[i] * 38 / 64, 12 + (int32_t)i, 46 - sp.lo[i] * 38 / 64,
                p >= ma && p < mb ? T_THEME : T_DIM);
    }
    for (i = 0; i <= n; i++) {                       /* the markers in the view (the selected one last: on top) */
        uint32_t p = slice_mark(i);
        if (i != j && p >= a && p <= b)
            cv_rect(SPX(p), 4, 1, 84, T_MID);
    }
    {
        uint32_t p = slice_mark(j);
        if (p >= a && p <= b)
            cv_rect(SPX(p) > 226 ? 226 : SPX(p), 2, 2, 88, T_ACCENT);
    }
#undef SPX
    cv_rrect(12, 96, (int32_t)SP_COLS, 3, 1, T_RAISE, T_SURF);   /* the whole: every marker, the view */
    for (i = 0; i <= n; i++)
        cv_rect(12 + (int32_t)((uint32_t)slice_mark(i) * (SP_COLS - 1u) / len), 94, 1, 7, i == j ? T_ACCENT : T_MID);
    {
        int32_t x0 = 12 + (int32_t)((uint32_t)a * SP_COLS / len), x1 = 12 + (int32_t)((uint32_t)b * SP_COLS / len);
        cv_rect(x0, 103, x1 - x0 < 2 ? 2 : x1 - x0, 2, T_THEME);
    }
    str_cpy(t, "LEN ", sizeof t);
    slice_time(u, j < n ? slice_mark(j + 1u) - slice_mark(j) : len - mb);
    str_cpy(t + 4, u, sizeof t - 4);
    str_cpy(t + str_len(t), " S", sizeof t - str_len(t));
    cv_text(12, 106, &AF_S, t, T_MID);
    cv_text_r(228, 106, &AF_S, N_SLC_SRC[src], SLC_SRC_USR(src) ? T_THEME : T_DIM, T_SURF);
}
#endif

/* WHEEL: the nine drawbars as rounded bars over RAISE slots (the bars of the knob just turned: the accent),
 * their footages under them */
static void graph_wheel(const track_t *t, uint16_t c)
{
    uint32_t k;
    static const char *const names[9] = {"16", "5.3", "8", "4", "2.7", "2", "1.6", "1.3", "1"};
    for (k = 0; k < 9u; k++) {
        int32_t x = 11 + (int32_t)k * 25, level;
        const uint8_t *bar = 0;
        const char *footage = names[k];
        char remote_name[17];
        int remote = 0;
#if FELUCCA_CAFA
        remote = cafa_native_profile_view != 0;
        if (remote) {
            bar = cafa_nv_row(k);
            if (bar) { cafa_native_text(remote_name, bar, 16u); footage = remote_name; }
        }
        level = remote ? (bar ? ((int32_t)cafa_native_u14(bar + 41u) * 8 + 8191) / 16383 : 0) : drw_level(t->p, k);
#else
        level = drw_level(t->p, k);
#endif
        if (!remote && t->p[P_E4] && k == 8u) level = 0; /* DSP's percussion cancels the 1-foot bar. */
        int hot = !remote && ui.hot_t && cur_page()->id[0] == P_E0 &&
                  (ui.hot_col == 0u || ui.hot_col == (k < 2u ? 1u : k < 4u ? 2u : 3u));
        if (ux.quark)                                /* (Skiff Q4) QUARK: a drawbar a calorimeter tower */
            qk_tower(x + 4, 75, 10, 72, level * 9, hot ? T_ACCENT : c, 0);
        else if (ux.neon)                            /* (Skiff N4) NEON: a drawbar a VU column as far out as it is */
            ne_vu(x + 4, 75, 10, 72, level * 9, hot ? T_ACCENT : 0, ux_mix(T_SURF, T_TEXT, 8), 0);
        else if (ux.crypt) {                         /* (Skiff) CRYPT: a drawbar a # column as far out as it is */
            int32_t r;
            for (r = 0; r < level; r++)
                cr_ch(x + 7, 74 - r * 8, &AF_S, '#', hot ? T_ACCENT : c);
            if (!level)
                cr_ch(x + 7, 74, &AF_S, '.', hot ? T_ACCENT : T_DIM);
        } else {
        cv_rrect(x + 7, 2, 4, 73, 2, T_RAISE, T_SURF);
        if (level) cv_rrect(x + 3, 75 - level * 8, 12, level * 8, 3, hot ? T_ACCENT : c, T_SURF);
        else cv_rrect(x + 3, 73, 12, 2, 1, hot ? T_ACCENT : T_DIM, T_SURF);
        }
        GFX_HOOK_ALIGN(x + 7, 0, x + 11, 0, AL_H, "wheel footage under its drawbar");
        cv_text_in(x + 3, 82, 12, &AF_S, footage, T_MID, T_SURF);   /* under the bar, by its ink */
    }
}
/* The FM charts' parts: an operator box 21 x 17 (rows 23 px apart, columns 24), junction dots, Manhattan routes */
#define FM_BH 17
static void fm_dot(int32_t x, int32_t y, uint16_t c) { cv_rect(x - 1, y - 1, 3, 3, c); }
/* a Manhattan route, 1 px: down from (x0, y0) to the jog row jy, across to x1, down to y1 */
static void fm_route(int32_t x0, int32_t y0, int32_t x1, int32_t jy, int32_t y1, uint16_t c)
{
    cv_rect(x0, y0, 1, jy - y0, c);
    cv_rect(x0 < x1 ? x0 : x1, jy, (x0 < x1 ? x1 - x0 : x0 - x1) + 1, 1, c);
    cv_rect(x1, jy, 1, y1 - jy, c);
}
/* a feedback loop on the operator box at (x, y): out of its right side, up, back in on top with an arrow */
static void fm_loop(int32_t x, int32_t y, uint16_t c)
{
    cv_rect(x + 11, y + 5, 5, 1, c);
    cv_rect(x + 15, y - 4, 1, 9, c);
    cv_rect(x, y - 4, 15, 1, c);
    cv_rect(x, y - 4, 1, 4, c);
    cv_line(x - 2, y - 3, x - 1, y - 2, c); cv_line(x + 2, y - 3, x + 1, y - 2, c);
}
/* the output bus under the carriers (the leftmost at cl, the rightmost at cr) at y bus, an arrow out on the right */
static void fm_bus(int32_t cl, int32_t cr, int32_t bus)
{
    cv_rect(cl, bus, cr + 24 - cl, 1, T_MID);
    cv_line(cr + 20, bus - 3, cr + 23, bus, T_MID); cv_line(cr + 20, bus + 3, cr + 23, bus, T_MID);
}

#ifndef FM6_CHART_HOOK
#define FM6_CHART_HOOK(kind, a, b) ((void)0)   /* ui_render.c (host tests): the chart's parts one by one (its lint) */
#endif
enum { FMH_BOX, FMH_ROUTE, FMH_CAR, FMH_BUS, FMH_FB, FMH_LABEL, FMH_END };
/* FM6's 32 algorithms as charts. FM6_CELL: per operator 1..6 its cell, the column in 24 px steps (bits 0..2) and
 * the row above the output bus (bits 4..5; row 0 = the carriers). The routes, the carriers and the feedback
 * operator are fm6_core.c's FM6_ALG itself (fm6_routes, fm6_car_ops, fm6_fb_op). ui_test.c checks the cells
 * against them (a modulator one row above what it modulates, the carriers on the bottom row and only they, one
 * operator per cell) and the routes against the 32 algorithms written out; ui_render.c lints every chart as drawn
 * (no route through a box or touching another route, no two boxes overlapping, all inside the panel). The cells:
 * a search for the fewest and shortest bends, the operators numbered left to right. */
static const uint8_t FM6_CELL[32][6] = {
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x31}, {0x00, 0x10, 0x02, 0x12, 0x22, 0x32}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x21},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x21}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12},
    {0x00, 0x10, 0x01, 0x11, 0x12, 0x22}, {0x00, 0x10, 0x01, 0x12, 0x11, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x13, 0x23},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x03, 0x12, 0x13, 0x14},
    {0x00, 0x10, 0x02, 0x11, 0x12, 0x13}, {0x00, 0x10, 0x01, 0x11, 0x20, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x21, 0x22},
    {0x01, 0x10, 0x11, 0x21, 0x12, 0x22}, {0x01, 0x12, 0x10, 0x20, 0x11, 0x21}, {0x01, 0x10, 0x12, 0x11, 0x21, 0x31},
    {0x00, 0x10, 0x20, 0x01, 0x02, 0x11}, {0x00, 0x01, 0x10, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x10, 0x02, 0x03, 0x12},
    {0x00, 0x10, 0x01, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x11, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x13},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x13}, {0x00, 0x01, 0x11, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x11, 0x03, 0x13, 0x14},
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x02}, {0x00, 0x01, 0x02, 0x12, 0x03, 0x13}, {0x00, 0x01, 0x02, 0x12, 0x22, 0x03},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x14}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}};
/* algorithm a's routes: m[i] bit j: operator j + 1 modulates operator i + 1. FM6_ALG runs the operators sixth
 * first, each reading a bus (or none) and writing one (or adding to it): who wrote a bus modulates its reader */
static void fm6_routes(uint32_t a, uint8_t *m)
{
    const uint8_t *f = FM6_ALG[a & 31u];
    uint8_t bus[4] = {0, 0, 0, 0};
    uint32_t k;
    for (k = 0; k < 6u; k++) {
        uint32_t op = 5u - k, in = (f[k] >> 4) & 3u, o = f[k] & 3u;
        m[op] = in ? bus[in] : 0u;
        bus[o] = (uint8_t)(((f[k] & FM6_OADD) ? bus[o] : 0u) | 1u << op);
    }
}
/* the operator with feedback in algorithm a (0..5: operator 1..6) */
static uint32_t fm6_fb_op(uint32_t a)
{
    uint32_t k;
    for (k = 0; k < 5u && (FM6_ALG[a & 31u][5u - k] & 0xC0u) != 0xC0u; k++)
        ;
    return k;
}
/* the carriers of algorithm a: bit k operator k + 1 (fm6_carriers counts the sixth first) */
static uint32_t fm6_car_ops(uint32_t a)
{
    uint32_t c = fm6_carriers(a), r = 0, k;
    for (k = 0; k < 6u; k++)
        r |= (c >> (5u - k) & 1u) << k;
    return r;
}
/* the algorithm FM6 plays on track t (0..31): ALG, or the patch's at PAT */
static uint32_t fm6_alg_of(const track_t *t)
{
    return t->p[P_E0] >= 1 && t->p[P_E0] <= 32 ? (uint32_t)t->p[P_E0] - 1u : fm6_patch[(t - trk) % NTRK][FP_ALG] & 31u;
}
#if FELUCCA_CAFA
static void cafa_nv_fm_network(uint16_t c)
{
    uint32_t k,i,n,maxcol=0u,maxrow=0u,carriers=0u,selected=cafa_native_u14(cafa_native_profile_view+24);
    uint32_t bytes;const uint8_t *rows=cafa_nv_section(1,&bytes);
    const uint8_t *edges=cafa_nv_section(9,&bytes),*series=cafa_nv_section(8,&bytes);
    int32_t x[6],y[6],bus,cl=240,cr=0;char title[25];
    n=rows?rows[0]:0u;if(n>6u)n=6u;
    if(!n){panel_note("NO OPERATORS",0,0);return;}
    for(k=0;k<n;k++) {
        const uint8_t *r=cafa_nv_row(k);
        uint32_t col=r?cafa_native_u14(r+41):k,row=r?cafa_native_u14(r+43):0u;
        if(col>5u)col=5u;if(row>5u)row=5u;
        if(col>maxcol)maxcol=col;if(row>maxrow)maxrow=row;
        if(r && (r[40]&2u))carriers|=1u<<k;
    }
    bus=50+(27+23*(int32_t)maxrow)/2;
    for(k=0;k<n;k++) {
        const uint8_t *r=cafa_nv_row(k);
        uint32_t col=r?cafa_native_u14(r+41):k,row=r?cafa_native_u14(r+43):0u;
        if(col>5u)col=5u;if(row>5u)row=5u;
        x[k]=120+(int32_t)col*24-(int32_t)maxcol*12;
        y[k]=bus-6-FM_BH-23*(int32_t)row;
        if(carriers&(1u<<k)){if(x[k]<cl)cl=x[k];if(x[k]>cr)cr=x[k];}
    }
    if(edges)for(i=0;i<edges[0];i++) {
        const uint8_t *e=edges+1u+5u*i;
        if(e[0]<n && e[1]<n && (e[2]&1u)) {
            uint16_t ec=cafa_native_u14(e+3u)>8191u?c:T_MID;
            if(e[0]==e[1])fm_loop(x[e[0]],y[e[0]],ec);
            else fm_route(x[e[0]],y[e[0]]+FM_BH,x[e[1]],y[e[1]]-3,y[e[1]],ec);
        }
    }
    if(carriers) {
        for(k=0;k<n;k++)if(carriers&(1u<<k))cv_rect(x[k],y[k]+FM_BH,1,bus-y[k]-FM_BH,T_MID);
        fm_bus(cl,cr,bus);
    }
    for(k=0;k<n;k++) {
        const uint8_t *r=cafa_nv_row(k);uint32_t active=r && !(r[40]&16u),pick=k==selected;
        uint16_t fill=(carriers&(1u<<k))&&active?c:T_RAISE;
        char label[17],num[5];
        if(r)cafa_native_text(label,r,16);else label[0]=0;
        if(pick)fill=T_ACCENT;
        cv_rrect(x[k]-10,y[k],21,FM_BH,3,fill,T_SURF);
        fmt_int(num,(int32_t)k+1);
        cv_text_in(x[k]-10,y[k]+CAP_IN(S,FM_BH),21,&AF_S,num,fill==T_RAISE?T_MID:T_INK,fill);
        if(r && label[0])cv_free_text(x[k]-10,y[k]+FM_BH+2,&AF_S,label,T_MID,T_SURF,38);
    }
    if(series) {
        const uint8_t *p=series+1u;
        for(i=0;i<series[0];i++) {
            uint32_t id=p[0],count=p[2],j;int32_t px=0,py=0;
            if(id<n)for(j=0;j<count;j++) {
                int32_t sx=10+(int32_t)cafa_native_u14(p+3u+6u*j)*220/16383;
                int32_t sy=graph_ht-4-(int32_t)cafa_native_u14(p+5u+6u*j)*12/16383;
                if(j)cv_line_t(px,py,sx,sy,id==selected?T_ACCENT:T_MID,1);
                px=sx;py=sy;
            }
            p+=3u+6u*count;
        }
    }
    cafa_native_text(title,cafa_native_profile_view+57,24);
    cv_text(10,4-GOY,&AF_S,title,T_MID);
}
#endif
/* FM6's EDIT pages: the algorithm, drawn as graph_fm draws DIGITAL's, "ALG 05" top left. Carriers THEME with INK
 * numerals, modulators RAISE with THEME ones; an operator at output level 0: a DIM numeral (a carrier on RAISE).
 * Routes THEME, the feedback loop MID (DIM at feedback 0), the output bus MID. The knob just turned in ACCENT:
 * ALG and PTCH the label, FB the loop, MLVL the routes, MRAT MEG VMOD the modulators, DTUN the carriers. */
static void graph_fm6(const track_t *t, uint16_t c)
{
#if FELUCCA_CAFA
    if(cafa_native_profile_view){cafa_nv_fm_network(c);return;}
#endif
    uint32_t alg = fm6_alg_of(t), k, j, car = fm6_car_ops(alg), top = 0, hi = 0, fbop = fm6_fb_op(alg), hotset = 0;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    const uint8_t *pt = fm6_patch[(t - trk) % NTRK];
    int32_t x[6], y[6], bus, cl = 240, cr = 0, fb = clamp(pt[FP_FB] + t->p[P_E1], 0, 7);
    uint8_t m[6], fan[6] = {0, 0, 0, 0, 0, 0};
    uint16_t rc = hot == P_E2 ? T_ACCENT : c;
    char b[7] = {'A', 'L', 'G', ' ', (char)('0' + (alg + 1u) / 10u), (char)('0' + (alg + 1u) % 10u), 0};
    if (hot == P_E3 || hot == P_E4 || hot == P_E5)
        hotset = 63u & ~car;
    else if (hot == P_E6)
        hotset = car;
    fm6_routes(alg, m);
    for (k = 0; k < 6u; k++) {
        uint32_t col = FM6_CELL[alg][k] & 7u, row = FM6_CELL[alg][k] >> 4;
        hi = col > hi ? col : hi;
        top = row > top ? row : top;
        for (j = 0; j < 6u; j++)
            fan[j] += (uint8_t)(m[k] >> j & 1u);
    }
    bus = 50 + (27 + 23 * (int32_t)top) / 2;         /* the chart centred on the 100 px scale */
    for (k = 0; k < 6u; k++) {
        x[k] = 120 + (int32_t)(FM6_CELL[alg][k] & 7u) * 24 - (int32_t)hi * 12;
        y[k] = bus - 6 - FM_BH - 23 * (int32_t)(FM6_CELL[alg][k] >> 4);
        if (car >> k & 1u) { cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    if (sheet_of(cur_page()->graph)) {               /* ((FM6 LAB T) ALL OPS: the theme's mark round the operator picked,
                                                      * under the chart: a gold frame, a laser, a cell's brackets) */
        k = fm_ui[0] % 6u;
        if (ux.crypt)
            cv_frame(x[k] - 12, y[k] - 2, 25, FM_BH + 4, CR_GOLD);
        else if (ux.neon)
            ne_laser(x[k] - 12, y[k] - 2, 25, FM_BH + 4, T_SURF);
        else if (ux.quark)
            qk_corners(x[k] - 13, y[k] - 3, 27, FM_BH + 6, T_ACCENT, 4);
    }
    for (k = 0; k < 6u; k++) {                       /* into k: its sources down to its jog row, across, down */
        for (j = 0; j < 6u; j++)
            if (m[k] >> j & 1u) {
                FM6_CHART_HOOK(FMH_ROUTE, j, k);
                fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], rc);
                if (fan[j] > 1u) fm_dot(x[j], y[k] - 3, rc);          /* one operator modulating several */
                if (m[k] & (m[k] - 1u)) fm_dot(x[k], y[k] - 3, rc);   /* several modulating one */
            }
        if (car >> k & 1u) {
            FM6_CHART_HOOK(FMH_CAR, k, 0);
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    FM6_CHART_HOOK(FMH_BUS, 0, 0);
    fm_bus(cl, cr, bus);
    FM6_CHART_HOOK(FMH_FB, fbop, 0);
    fm_loop(x[fbop], y[fbop], hot == P_E1 ? T_ACCENT : fb ? T_MID : T_DIM);
    for (k = 0; k < 6u; k++) {                       /* ((FM6 LAB) ALL OPS: the operator picked, ACCENT) */
        uint32_t on = pt[(5u - k) * FP_OP + FP_OL] != 0, pick = sheet_of(cur_page()->graph) && k == fm_ui[0] % 6u;
        uint32_t fill_c = ((car >> k & 1u) && on) || pick;
        uint16_t f = hotset >> k & 1u || pick ? T_ACCENT : c, fill = fill_c ? f : T_RAISE;
        char n[2] = {(char)('1' + k), 0};
        FM6_CHART_HOOK(FMH_BOX, k, 0);
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "FM6 operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, n, fill_c ? T_INK : on ? f : T_DIM, fill);
    }
    FM6_CHART_HOOK(FMH_LABEL, 0, 0);
    cv_text(10, 4 - GOY, &AF_S, b, hot == P_E0 || hot == P_E7 ? T_ACCENT : T_MID);
    FM6_CHART_HOOK(FMH_END, alg, 0);
}

#if FELUCCA_FM4
/* DIGITAL's eight algorithms as FM charts, read from src/eng_digital.c's switch (alg) (ui_test.c checks
 * these tables against it). FM_CELL: per operator 1..4 its grid cell, the column in 24 px steps (bits 0..2)
 * and the row above the output bus (bits 4..5; row 0 = the carriers). FM_MOD: per destination operator d a
 * nibble at bit 4 d of the operators modulating it (bit k = operator k + 1). Operator 4 feeds itself (FB). */
static const uint8_t FM_CELL[8][4] = {
    {0x00, 0x10, 0x20, 0x30}, {0x01, 0x11, 0x20, 0x22}, {0x01, 0x10, 0x20, 0x12}, {0x01, 0x12, 0x10, 0x20},
    {0x00, 0x10, 0x02, 0x12}, {0x00, 0x02, 0x04, 0x12}, {0x00, 0x02, 0x04, 0x14}, {0x00, 0x02, 0x04, 0x06}};
static const uint16_t FM_MOD[8] = {0x0842, 0x00C2, 0x004A, 0x0806, 0x0802, 0x0888, 0x0800, 0x0000};
/* Modulators above what they modulate, the carriers on the bottom row over the output bus (an arrow out).
 * Carriers THEME with INK numerals, modulators RAISE with THEME ones; an operator at LEVEL 0: DIM numeral.
 * Routes by IDX: DIM at 0, MID, THEME from 64; op 4's feedback loop MID (DIM at FB 0). The knob just turned
 * (an operator's ratio or level, IDX, FB): ACCENT. Junction dots where routes split or merge. */
static void graph_fm(const track_t *t, uint16_t c)
{
#if FELUCCA_CAFA
    if(cafa_native_profile_view){cafa_nv_fm_network(c);return;}
#endif
    uint32_t alg = (uint32_t)t->p[P_E0] & 7u, k, j, mods = FM_MOD[alg], car = 0, hop = 9;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    int32_t x[4], y[4], lo = 7, hi = 0, top = 0, bus, cl = 240, cr = 0, idx = t->p[P_E4];
    uint16_t ec = hot == P_E4 ? T_ACCENT : !idx ? T_DIM : idx < 64 ? T_MID : c;
    if (hot >= P_E1 && hot <= P_E3) hop = hot - P_E1 + 1u;
    else if (hot >= P_FM1_ATK && hot < P_E0) hop = (hot - P_FM1_ATK) / 5u;
    for (k = 0; k < 4u; k++) {
        int32_t col = FM_CELL[alg][k] & 7, row = FM_CELL[alg][k] >> 4;
        lo = col < lo ? col : lo; hi = col > hi ? col : hi; top = row > top ? row : top;
    }
    bus = 50 + (27 + 23 * top) / 2;                  /* the chart centred on the 100 px scale */
    for (k = 0; k < 4u; k++) {
        x[k] = 120 + (FM_CELL[alg][k] & 7) * 24 - (lo + hi) * 12;
        y[k] = bus - 6 - FM_BH - 23 * (FM_CELL[alg][k] >> 4);
        if (FM_CELL[alg][k] < 0x10) { car |= 1u << k; cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 4u; k++) {                       /* into k: sources down to its jog row, across, down */
        uint32_t m = (mods >> (4u * k)) & 15u;
        for (j = 0; j < 4u; j++) if (m >> j & 1u) {
            uint32_t fan = mods & (0x1111u << j);
            fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], ec);
            if (fan & (fan - 1u)) fm_dot(x[j], y[k] - 3, ec);   /* one operator modulating several */
        }
        if (m & (m - 1u)) fm_dot(x[k], y[k] - 3, ec);           /* several modulating one */
        if (car >> k & 1u) {
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    fm_bus(cl, cr, bus);                             /* the output bus and its arrow */
    fm_loop(x[3], y[3], hot == P_E6 ? T_ACCENT : t->p[P_E6] ? T_MID : T_DIM);   /* op 4's feedback */
    for (k = 0; k < 4u; k++) {
        char b[2] = {(char)('1' + k), 0};
        uint32_t on = t->p[P_FM1_LEVEL + k * 5u] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = k == hop ? T_ACCENT : c, fill = fill_c ? f : T_RAISE;
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "DIGITAL operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, b, fill_c ? T_INK : on ? f : T_DIM, fill);
    }
}
#endif
/* Save-bank validity is expensive (CRC/import). Refresh it once per redraw or
 * twice per second, rather than parsing four projects on every UI frame. */
static char graph_pname[4][13];                       /* .. and the slots' names ("" none) */
static uint32_t graph_pname_sig;
static int graph_project_used(uint32_t slot)
{
    static uint32_t ms, frame;
    static uint8_t mask, ready;
    if (!ready || fm1_ms - ms >= 500u || (ui.force && frame != ui.frame)) {
        uint32_t i; mask = 0;
        for (i = 0; i < 4u; i++) mask |= (uint8_t)((project_name(i, graph_pname[i]) != 0) << i);
        graph_pname_sig = fnv(2166136261u, graph_pname, sizeof graph_pname);
        ready = 1; ms = fm1_ms; frame = ui.frame;
    }
    return (mask >> (slot & 3u)) & 1u;
}
static const char *graph_project_name(uint32_t slot)  /* (after graph_project_used) */
{
    return graph_pname[slot & 3u];
}

/* MENU > LARGE's strip under the tall cards (ui.c LK_TALL): HOME's scope, ENV's ADSR, LFO's wave, PATTERN's steps,
 * MIXER's tracks drawn small; on the other pages the page's title and number (their charts need the whole panel) */
enum { SK_NONE, SK_SCOPE, SK_ADSR, SK_LFO, SK_STEPS, SK_TRK, SK_TITLE };
static uint32_t strip_kind(void)
{
    uint32_t g;
    if (large_kind() != LK_TALL)
        return SK_NONE;
    if (ui.home)
        return SK_SCOPE;
    g = cur_page()->graph;
    return g == GR_ADSR ? SK_ADSR : g == GR_LFO ? SK_LFO : g == GR_STEPS ? SK_STEPS : g == GR_TRK ? SK_TRK : SK_TITLE;
}
/* the page's title and its number in its family ("ENV DEST 2/2", EDIT: the engine's "OSC 1/2"); the piano roll: its
 * scale ("C MIN"); ti holds 20 (the footer, the strip) */
static void page_title(char *ti)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i, n = 0, k = 0;
    const char *pt = pg->scope == SC_ENGINE ? e->page_title[pg->id[0] != P_E0] : 0;   /* EDIT: the engine's */
    for (i = 0; i < NPAGES; i++)                     /* ((Skiff U3, N6) GLO's: MIXER first, then the others) */
        if (page_group(i) == page_group(ui.page) && page_visible(i)) {
            n++;
            if (page_order(i) <= page_order(ui.page))
                k++;
        }
    str_cpy(ti, pt ? pt : grid_on() ? "GRID" : pg->title, 12);
    if (n > 1) {
        str_cpy(ti + str_len(ti), " ", 4);
        fmt_kn(ti + str_len(ti), (int32_t)k, (int32_t)n);
    }
    if (pg->graph == GR_ROLL && !drum_track(t)) {  /* the piano roll: its tinted rows' scale ("C MIN") */
        str_cpy(ti, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
        str_cpy(ti + str_len(ti), " ", 4);
        str_cpy(ti + str_len(ti), N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 8);
    }
}
/* (Skiff U4, L5) where the page is in its family, for the footer: "2/3" ("": a family of one; a sheet (DRUM-X's,
 * FM6's) too (#7): SELECT's rows are on SHIFT's map) */
static void page_pos(char *ti)
{
    uint32_t i, n = 0, k = 0;
    ti[0] = 0;
    for (i = 0; i < NPAGES; i++)
        if (page_group(i) == page_group(ui.page) && page_visible(i)) {
            n++;
            if (page_order(i) <= page_order(ui.page))
                k++;
        }
    if (n > 1u) {
        fmt_kn(ti, (int32_t)k, (int32_t)n);
    }
}
/* the strip's title: L centred (M when L lacks a glyph or is too wide) */
static void graph_title(void)
{
    char ti[20];
    const aafont_t *f = &AF_L;
    int32_t y;
    page_title(ti);
    if (!large_face_has(ti) || text_w(f, ti) > 220)
        f = &AF_M;
    y = f == &AF_L ? CAP_IN(L, LG_H_GRAPH) : CAP_IN(M, LG_H_GRAPH);
    GFX_HOOK_ALIGN(0, 0, 240, LG_H_GRAPH, AL_HV, "large strip title centred");
    cv_text_in(0, y, 240, f, ti, T_THEME, T_SURF);
}

#include "ui_home.c"                                  /* Skiff: HOME's four tracks (graph_tracks, home_sig) */
/* (2.5, #97, the user's 8C) MENU > LARGE on HOME: the strip under the tall cards names the six tracks' sounds, two
 * columns of three, the selected one a band of its colour (the themes' lanes as HOME's: CRYPT's stone, NEON's sign,
 * QUARK's beam); HOME tapped: the scope, as before */
static void graph_names(void)
{
    uint32_t k, shown = NTRK, first = 0u, selected = song.sel;
    int remote = 0;
#if FELUCCA_CAFA
    remote = cafa_native_profile_view != 0;
    if (remote) {
        shown = cafa_nv_track_lanes();
        first = cafa_native_u14(cafa_native_profile_view + 26u);
        selected = cafa_native_u14(cafa_native_profile_view + 24u);
    }
#endif
    int32_t rh = ((int32_t)graph_h() - 8) / 3;
    char name[20], d[6];
    cv_rect(0, 0, 240, (int32_t)graph_h(), T_BG);
    for (k = 0; k < shown; k++) {
        const track_t *t = remote ? 0 : &trk[k];
        const uint8_t *r = 0;
        uint32_t track = first + k;
#if FELUCCA_CAFA
        if (remote) r = cafa_nv_row(k);
#endif
        int sel = track == selected, ib = sel && !ux.quark;
        int32_t x = 3 + (int32_t)(k / 3u) * 118, y = 2 + (int32_t)(k % 3u) * (rh + 2);
        uint16_t col = (remote ? r && (r[40] & 4u) : t->p[P_MUTE]) ? T_DIM : T_TRK(track), lane = sel ? (ux.quark ? ux_mix(col, T_BG, 80) : col) :
                                                                ux.quark ? T_BG : T_SURF;
        if (ux.crypt && !sel)
            cr_stone(x, y, 116, rh, lane);
        else if (ux.neon && sel) {
            uint16_t st[2] = {col, ux_mix(col, NE_VIOLET, 70)};
            ne_hgrad(x, y, 116, rh, st, 2);
        } else
            cv_rect(x, y, 116, rh, lane);
        if (ux.quark && sel) {
            cv_rect(x, y, 116, 1, col);
            cv_rect(x, y + rh - 1, 116, 1, col);
        }
        fmt_int(d, (int32_t)track + 1);
        int32_t name_x = track < 9u ? 22 : 10 + text_w(&AF_M, d);
        cv_text_on(x + 6, y + CAP_IN(M, rh), &AF_M, d, ib ? T_INK : col, lane);
        if (remote) {
#if FELUCCA_CAFA
            if (r) cafa_native_text(name, r, 16u); else name[0] = 0;
#endif
        } else sound_name(t, name);
        cv_free_text(x + name_x, y + CAP_IN(M, rh), &AF_M, name[0] ? name : "--", ib ? T_INK : T_TEXT, lane, 112 - name_x);
    }
}

/* (Skiff U5) an EDIT page of ANALOG (its own graphs: ui_pgraph.c graph_osc, graph_flt) */
static int analog_page(const page_t *pg, const track_t *t)
{
    return pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_ANALOG;
}

static uint32_t graph_signature(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    if (ui.home)                                     /* scope: redraw every other frame; Skiff's tracks ((2.5, 8C) and
                                                      * LARGE's names): on a change */
        return ui.home_scope ? h ^ (ui.frame / 2u) : home_sig();
    h ^= (uint32_t)pg->graph * 131u + TSEL->eng_req + song.sel * 7777u + ui.page * 1291u;
    if (strip_kind() == SK_TITLE)                    /* MENU > LARGE: the page's title (its engine, its number) */
        return h ^ 0x5A17u;
    if ((pg->graph == GR_NONE && !analog_page(pg, t)) || pg->graph == GR_SMIX) h ^= ui.frame / 2u;   /* the scope; (#21)
                                                      * SAMPLES' waveform as EDIT's */
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    h ^= (uint32_t)TSEL->preset * 7u + (uint32_t)song.g[G_SLOT] * 13u + TSEL->user * 257u + up_gen * 7919u + ui.uslot * 104729u;
    if (pg->graph == GR_CHORD) {                     /* the last chord played */
        h = (h ^ (chord_last[song.sel].root + 131u * chord_last[song.sel].mask)) * 16777619u;
        for (i = 0; i < CHORD_MAX; i++)
            h = (h ^ chord_last[song.sel].note[i]) * 16777619u;
        h ^= (uint32_t)t->engine * 389u;             /* (MONO and kits follow the sounding engine) */
    }
    if (pg->graph == GR_FOLLOW) {                    /* ((2.1) the chord of the track it follows, that track's sound) */
        const fol_src_t *s = &fol_src[(uint32_t)(t->p[P_FOLLOW] + NTRK - 1) % NTRK];
        h = (h ^ (s->pc + 4099u * s->lo + 257u * s->hi + 65537u * s->last)) * 16777619u;
        h ^= (uint32_t)trk[(uint32_t)(t->p[P_FOLLOW] + NTRK - 1) % NTRK].engine * 389u;
    }
    if (pg->graph == GR_MOD)
        h ^= (mod_ui_route + 1u) * 40503u;
    if (sheet_of(pg->graph))                         /* (Skiff phase 7; FM6 LAB) a sheet's: what it edits (SHEETS) */
        h ^= sheet_of(pg->graph)->sig();
    if (pg->graph == GR_SLCR && t->p[P_SLCR])        /* the SLICER's step playing */
        h ^= (sl[song.sel].idx + 1u) * 2654435761u;
    if (pg->graph == GR_SLOTS) {                     /* (a checksum over each slot) */
        for (i = 0; i < 4u; i++)
            h ^= (uint32_t)graph_project_used(i) << (20u + i);
        h += graph_pname_sig;
    }
    if (pg->graph == GR_LANES || pg->graph == GR_LMIX)   /* (Skiff U3) the levels, the lane; ((2.0, #36) the pans; #35 */
        for (i = 0; i < 8u; i++)                         /* a user kit's lanes: an upload) */
            h = (h ^ (uint32_t)t->p[P_LN0 + i] << 8 ^ (uint32_t)(t->p[P_LPAN0 + i] + 64) << 16 ^ (i == page_lane()) ^
                 smp_user_gen << 24) * 16777619u;
    if (pg->graph == GR_EVENTS) {                    /* AUTOMATION: the rows, the one selected, + ADD LOCK's place; (U3)
                                                      * the top row's PLAY and count */
        h = (h ^ (ui.ev_top * 3u + motion_enabled(t)) * 7919u ^ motion_count(t) * 131u) * 16777619u;
        uint8_t idx[MOTION_MAX];
        uint32_t n = ev_rows(idx);
        for (i = 0; i < n; i++)
            h = (h ^ ((uint32_t)motion.event[idx[i]].place << 24 | (uint32_t)(motion.event[idx[i]].id | motion.event[idx[i]].lock << 8) << 15 |
                      (uint16_t)motion.event[idx[i]].value)) * 16777619u;
        h = (h ^ (n + 1u) * 40503u ^ ev_row(n) * 2654435761u ^ ((uint32_t)ui.ev_step << 8 | ui.ev_id) * 104729u) * 16777619u;
    }
#if FELUCCA_SLICE
    if (pg->graph == GR_SLICES && slice_page_ok()) h ^= slice_sig();
#endif
    if (pg->graph == GR_FEEL || step_page(pg->graph)) {   /* (Skiff 2.0) the selection: its range, its picks; FEEL's
                                                      * tiles' knob */
        for (i = sel_lo(); nsel.picked && i <= sel_hi(); i++)
            h = (h ^ nsel.vp[i] << (i & 15u)) * 16777619u;
        h ^= (sel_lo() * 64u + sel_hi() + 1u) * 2246822519u + nsel.view * 104729u;
    }
    if (pg->graph == GR_ADSR)                        /* (Skiff) the point (curve) of the knob
                                                      * just turned */
        h ^= (ui.hot_t ? ui.hot_col + 1u : 0u) * 65537u;
    if (pg->graph == GR_LFO)                         /* (Skiff) the LFO as it runs: a px of its 2 s (lfo_view_at) */
        h ^= (t->lfo_t / (LFO_WIN / 216u) + 1u) * 2654435761u;
    if (pg->scope == SC_GLOBAL || pg->graph == GR_FXDLY || pg->graph == GR_FXREV)   /* (Skiff U5) GLO's buses and GLOBAL
                                                      * draw the song's settings; (2.5) FX > DELAY, REVERB too (a track's
                                                      * page: PRESETS' MODE card, the echoes, the tail) */
        for (i = 0; i < G_COUNT; i++)
            h = (h ^ (uint16_t)song.g[i]) * 16777619u;
    if (pg->graph == GR_VOICE2) {                    /* (Skiff U5) the voices sounding, the next one */
        for (i = 0; i < NVOICE; i++)
            h = (h ^ t->v[i].active << i) * 16777619u;
        h ^= (t->rr + 1u) * 40503u;
    }
    if (pg->graph == GR_ARP || pg->graph == GR_ARP2) {   /* (Skiff U5) the notes held */
        for (i = 0; i < t->nheld && i < 16u; i++)
            h = (h ^ t->held[i]) * 16777619u;
        h ^= (t->nheld + 1u) * 104729u;
    }
    if (pg->graph == GR_BANK)                        /* (Skiff phase 5) the pool, the one picked (and the lists: below) */
        h = (h ^ (bank_gen() * 2654435761u + ui.bslot * 40503u + ui.bact * 7919u)) * 16777619u;   /* ((2.0) ACTION) */
    if (pg->graph == GR_SONG || pg->graph == GR_BANK) {   /* (Skiff phase 5) the lists, where each track is, what is
                                                      * staged, the entry picked */
        const uint8_t *lb = &lists.ent[0][0][0];
        for (i = 0; i < sizeof lists.ent; i++)
            h = (h ^ lb[i]) * 16777619u;
        for (i = 0; i < NTRK; i++)
            h = (h ^ (bank_cur(i) | bank_next(i) << 8 | (uint32_t)trk_pid[i] << 16)) * 16777619u;
        h = (h ^ (sg.entry * 40503u + song_pick() * 769u + bank_gen() * 2654435761u)) * 16777619u;   /* ((2.0) COPY+) */
    }
    if (pg->scope == SC_ENGINE && (ENGINES[t->eng_req % NENGINES] == &ENG_WHEEL || t->eng_req % NENGINES == ENGI_FM6 ||
                                   (FELUCCA_FM4 && t->eng_req % NENGINES == ENGI_DIGITAL)))
        h ^= (ui.hot_t ? ui.hot_col + 1u : 0u) * 65537u;
    if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_FM6)   /* the patch (PAT's algorithm, levels, FB) */
        h ^= (fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u;
    if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_SAMPLE) h ^= sample_wave.pos * 13u + sample_wave.key;
    if (pg->graph == GR_STEPS || step_page(pg->graph) || pg->graph == GR_FEEL) {
        uint32_t ph = song.playing ? t->seq_idx : 0xFFFFu;
        if (pg->graph != GR_STEPS && ph / 16u != ui.bank)
            ph = 0xFFFFu;                            /* the roll shows the cursor's bank only */
        h ^= steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.lane * 104723u + ui.bank * 613u +
             drum_mute[trk_index(t)] * 2654435761u + (uint32_t)t->p[P_DIR] * 6007u;   /* ((1.3) the lanes' mutes: the
                                                      * names dim; (2.0) PATTERN's DIR) */
        if (pg->graph != GR_STEPS) {                 /* the parameter locks' marks (the roll, the grid) */
            uint64_t lk = motion_lock_steps(trk_index(t));
            h = (h ^ (uint32_t)lk ^ (uint32_t)(lk >> 32) * 2654435761u) * 16777619u;
        }
        if (step_page(pg->graph) && !drum_track(t)) {   /* the roll: its view, the keys held */
            pr_follow(t);
            h = (h ^ (proll.lo + 1u)) * 16777619u;
            h = (h ^ pr_held()) * 16777619u;
        }
    }
    return h;
}
/* 4-letter engine tags of the preset list */
static const char *eng_abbr(const char *name)
{
    static const char *const A[][2] = {{"ANALOG", "ANLG"},
#if FELUCCA_FM4
                                       {"DIGITAL", "DGTL"},
#endif
                                       {"PHASE", "PHAS"}, {"LOFI", "LOFI"},
                                       {"SAMPLE", "SMPL"}, {"VOICE", "VOCL"}, {"TRIO", "TRIO"}, {"WHEEL", "WHEL"},
                                       {"GRAIN", "GRAN"}, {"PHYS", "PHYS"}, {"DRUM", "DRUM"},
                                       {"NOISE", "NOIS"}, {"FM6", "FM6"}, {"SLICE", "SLCE"}, {"DRUM-X", "DRMX"}};
    uint32_t i;
    for (i = 0; i < sizeof A / sizeof A[0]; i++)
        if (str_eq(name, A[i][0]))
            return A[i][1];
    return name;
}

/* a list's selected row: a THEME bar ((Skiff S1) NEON: a sunset, as the MENU's) */
static void list_bar(int32_t y)
{
    if (ux.neon) {
        uint16_t st[3] = {T_THEME, NE_VIOLET, ux_mix(NE_VIOLET, T_ACCENT, 50)};
        ne_hgrad(6, y, 228, 16, st, 3);
    } else
        cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
}
/* a list row (17 px): the selected one a THEME bar with INK text; tag at x 14, free text from x 54 to x1 */
#define LIST_Y(k) (3 + 17 * (int32_t)(k))
static void list_row(int32_t y, int sel, const char *tag, uint16_t tc, const char *name, uint16_t nc, int32_t x1)
{
    uint16_t bg = sel ? T_THEME : T_SURF;
    if (sel)
        list_bar(y);
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_text_on(14, y + CAP_IN(S, 16), &AF_S, tag, sel ? T_INK : tc, bg);
    if (name[0])
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_free_text(54, y + CAP_IN(S, 16), &AF_S, name, sel ? T_INK : nc, bg, x1 - 54);
}
#if FELUCCA_CAFA
static void cafa_nv_browser_rows(uint32_t selected,uint32_t first)
{
    uint32_t k;
    (void)selected; (void)first;
    for(k=0;k<7u;k++) {
        const uint8_t *r=cafa_nv_row(k);char tag[9],name[17],value[17];
        int sel;
        if(!r)continue;
        sel=(r[40]&1u)!=0u;
        cafa_native_text(tag,r+32,8);cafa_native_text(name,r,16);cafa_native_text(value,r+16,16);
        list_row(LIST_Y(k),sel,tag,T_DIM,name,(r[40]&16u)?T_DIM:T_TEXT,value[0]?190:232);
        if(value[0])cv_text_r(228,LIST_Y(k)+CAP_IN(S,16),&AF_S,value,sel?T_INK:T_MID,sel?T_THEME:T_SURF);
        if(r[40]&2u)cv_icon_in(214,LIST_Y(k),0,16,12,ICON_X_STAR,sel?T_INK:T_ACCENT,sel?T_THEME:T_SURF);
    }
}
#endif
/* an empty list: a title and a hint, centred */
static void note_line(int32_t y, const char *s, uint16_t fg)   /* centred S; "[K2] ADD PATTERN": a key hint */
{
    uint32_t n;
    int32_t id = kc_tag(s, &n);
    if (id < 0) {
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
        cv_text_in(0, y, 240, &AF_S, s, fg, T_SURF);
        return;
    }
    s += n;
    while (*s == ' ')
        s++;
    {                                                  /* (Skiff) "KEY: WHAT" (gfx.c kh_plain) */
        char b[48];
        kh_plain(b, sizeof b, (uint32_t)id, s);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
        cv_text_in(0, y, 240, &AF_S, b, fg, T_SURF);
    }
}
static void panel_note(const char *a, const char *b, const char *c)
{
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "panel note centred");
    cv_text_in(0, 30, 240, &AF_M, a, T_TEXT, T_SURF);
    if (b) note_line(60, b, T_MID);
    if (c) note_line(82, c, T_DIM);
}
/* the same title led by an icon (16 px, the accent's: the header message's, ui_draw.c draw_head) in the middle of
 * the panel: the title's capitals centred up and down in it, the icon's ink on them; across, the pair centred by
 * its ink */
#define PA_GAP 6                                        /* icon ink -> title ink (setup_title's) */
static void panel_alert(uint32_t id, const char *a)
{
    int32_t bi[4], bt[4], x, y = CAP_IN(M, (int32_t)graph_h());
    icon_ink(16, id, bi);
    text_ink(&AF_M, a, bt);
    x = HALF_UP(240 - (bi[2] - bi[0] + PA_GAP + bt[2] - bt[0])) - bi[0];
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "panel alert centred");
    GFX_HOOK_ALIGN(0, y + AF_M_CAP_Y, 0, y + AF_M_CAP_Y + AF_M_CAP_H, AL_V | AL_PASS, "panel alert icon on its title's line");
    cv_icon_in(x, y + AF_M_CAP_Y, 0, AF_M_CAP_H, 16, id, T_ACCENT, T_SURF);
    GFX_HOOK_ALIGN(0, 0, 0, (int32_t)graph_h(), AL_V | AL_PASS, "panel alert title in the panel's middle");
    cv_text_on(x + bi[2] + PA_GAP - bt[0], y, &AF_M, a, T_TEXT, T_SURF);
}

/* preset browser: the global list (every engine), the current one selected; tag DIM, name TEXT,
 * favourites starred (the accent), the selected row's suggested pattern at its right */
static void graph_browse(void)
{
#if FELUCCA_CAFA
    if(cafa_native_profile_view){cafa_nv_browser_rows(cafa_native_u14(cafa_native_profile_view+24),
                                                     cafa_native_u14(cafa_native_profile_view+26));return;}
#endif
    uint32_t total, cur = preset_pos(&total), e, k;
    int32_t row;
    if (!total) {
        panel_note(ui_ptag ? "NO SOUNDS" : "NO FAVORITES", ui_ptag ? "list ALL" : "list ALL to add sounds", 0);
        return;
    }
    for (row = -3; row <= 3; row++) {
        int32_t y = LIST_Y(row + 3);
        char tag[6], nm[13];
        uint32_t index = preset_visible(cur, total, (uint32_t)(row + 3));
        int sel = index == cur;
        if (index >= total) continue;
        e = preset_at(index, &k);
        if (e == NENGINES) {                             /* user preset: "U07" and its name */
            up_slot_label(tag, k);
            up_name(k, nm);
        } else {
            str_cpy(tag, eng_abbr(ENGINES[e]->name), sizeof tag);
            str_cpy(nm, ENGINES[e]->presets[k].name, sizeof nm);
        }
        list_row(y, sel, tag, T_DIM, nm, T_TEXT, 212);
        if (favorite_has(e, k)) {
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row star centred up/down");
            cv_icon_in(214, y, 0, 16, 12, ICON_X_STAR, sel ? T_INK : T_ACCENT, sel ? T_THEME : T_SURF);
        }
    }
}
/* the EDIT layer (ui_layer.c): the sound loaded, as the browser's selected row: "03" (its place in KNOB 2's list,
 * the engine's sounds) or "U07", the name, the star of a favourite */
static void engine_sound_row(int32_t y)
{
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    uint32_t total, cur = eng_list_pos(&total), u = user_of(TSEL);
    char tag[6], nm[13];
    int fav = preset_favorite();
    if (u < UP_SLOTS) {
        up_slot_label(tag, u);
        up_name(u, nm);
    } else {
        tag[0] = (char)('0' + (cur + 1u) / 10u % 10u);
        tag[1] = (char)('0' + (cur + 1u) % 10u);
        tag[2] = 0;
        str_cpy(nm, e->npresets ? e->presets[TSEL->preset % e->npresets].name : "", sizeof nm);
    }
    list_row(y, 1, tag, T_DIM, nm, T_TEXT, fav ? 212 : 232);
    if (fav) {
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row star centred up/down");
        cv_icon_in(214, y, 0, 16, 12, ICON_X_STAR, T_INK, T_THEME);
    }
    (void)total;
}
/* user preset slots around the selected one: "U07  NAME" / EMPTY */
static void graph_user(void)
{
#if FELUCCA_CAFA
    if(cafa_native_profile_view){cafa_nv_browser_rows(cafa_native_u14(cafa_native_profile_view+24),
                                                     cafa_native_u14(cafa_native_profile_view+26));return;}
#endif
    int32_t row, first = clamp((int32_t)ui.uslot - 3, 0, UP_SLOTS - 7);
    for (row = 0; row < 7; row++) {
        uint32_t k = (uint32_t)(first + row);
        char tag[4], nm[13];
        int used = up_used(k);
        up_slot_label(tag, k);
        if (used)
            up_name(k, nm);
        else
            str_cpy(nm, "--", sizeof nm);            /* ((2.0) an OLD FORMAT one too: up_name's label in here grew
                                                      * draw_graph by about 300 B; LOAD and SAVE's question name it) */
        list_row(LIST_Y(row), k == ui.uslot, tag, T_MID, nm, used ? T_TEXT : T_DIM, 232);
    }
}
/* (Skiff phase 5) SAVE > PATTERNS, the pool view, as the chosen mockup draws it (design/mock_p5.py pool): the 64
 * patterns, 8 x 8, a kept one its number in a tile (BLOCKS: SURF on the black ground; FLAT, LINE: RAISE on the
 * panel): TEXT while something uses it, MID and outlined DIM while nothing does (it can be cleared: bank.c pool_refs);
 * the one KNOB 1 picked TEXT, INK on it (outlined TEXT when unused); an empty id a dot */
#define PV_W 28                                      /* a tile 28 wide, 30 apart from x 1: 1 .. 238 */
static void graph_bank(void)
{
    int32_t th = 15, py = 17;
    uint16_t tile = T_SURF;
    uint32_t id, n;
#if FELUCCA_CAFA
    if(cafa_native_profile_view) {
        uint32_t selected=cafa_native_u14(cafa_native_profile_view+24);
        cv_rect(0,0,240,(int32_t)cv_h,T_BG);cv_bg=T_BG;
        for(id=0;id<64u;id++) {
            const uint8_t *cell=cafa_nv_cell(id);uint32_t flags=cell?cell[2]:0u;
            int32_t x=1+30*(int32_t)(id%8u),y=2+py*(int32_t)(id/8u);
            char s[5];uint16_t fill=id==selected?T_TEXT:tile;
            if(!(flags&1u)){cv_rect(x+13,y+th/2,2,2,T_DIM);if(id==selected)cv_frame(x,y,PV_W,th,T_TEXT);continue;}
            if(ux.neon && id==selected)ne_vgrad(x,y,PV_W,th,NE_HOT,3);
            else cv_rrect(x,y,PV_W,th,0,fill,T_BG);
            uint32_t refs=cafa_native_u14(cell);
            if(!refs)cv_frame(x,y,PV_W,th,id==selected?T_TEXT:T_DIM);
            cafa_native_text(s,cell+3,4);
            cv_text_in(x,y+CAP_IN(S,th),PV_W,&AF_S,s,id==selected?T_INK:refs?T_TEXT:T_MID,fill);
        }
        return;
    }
#endif
    cv_rect(0, 0, 240, (int32_t)cv_h, T_BG);         /* (over the panel: the tiles are on the ground) */
    cv_bg = T_BG;
    for (id = 0; id < NPAT; id++) {
        int32_t x = 1 + 30 * (int32_t)(id % 8u), y = 2 + py * (int32_t)(id / 8u);
        int pick = id == ui.bslot % NPAT;
        uint16_t fill = pick ? T_TEXT : tile;
        char s[3];
        if (!pool_used(id)) {
            cv_rect(x + 13, y + th / 2, 2, 2, T_DIM);
            if (pick)
                cv_frame(x, y, PV_W, th, T_TEXT);
            continue;
        }
        n = pool_refs(id);
        if (ux.neon && pick)                         /* ((Skiff S1) NEON: the pick a dusk; the others glass, CRYPT's */
            ne_vgrad(x, y, PV_W, th, NE_HOT, 3);     /* stone (cv_rrect)) */
        else
            cv_rrect(x, y, PV_W, th, 0, fill, T_BG);
        if (!n)                                      /* nothing uses it: it can be cleared ((2.0, #46) lit while KNOB 4 */
            cv_frame(x, y, PV_W, th, pick || ui.bact == BA_UNUSED ? T_TEXT : T_DIM);   /* is CLEAR UNUSED) */
        pat_num(s, id);
        GFX_HOOK_ALIGN(x, y, x + PV_W, y + th, AL_HV, "pool tile number centred");
        cv_text_in(x, y + CAP_IN(S, th), PV_W, &AF_S, s, pick ? T_INK : n || ui.bact == BA_UNUSED ? T_TEXT : T_MID, fill);
    }
}
/* (Skiff phase 5) SEQ > SONG, as the chosen mockup draws it (design/mock_p5.py): a row per track, as the PERFORM map's
 * (ui_layer.c layer_perf): its number in its colour, then its list's 16 entries, one naming a pattern its number in a
 * tile (the entry the track is on: the track's colour; on the selected track's band INK, the number the band's), an
 * empty one a dot. The selected track's entry KNOB 1 picked: TEXT (empty: outlined TEXT); the entry staged to come in
 * (a launch, the song's next): outlined; repeats past 1: a tick each under the tile (three at most). The selected
 * track's row a band of its colour. BLOCKS: on the black ground; FLAT and LINE: on the panel, the band RAISE. (Phase 6)
 * With six tracks the rows of the four shown (ui.c trk_top), the tiles 13.4 apart and the band to x 236: the scroll
 * bar at 238 (design/mock_p6.py song). (#28) A REST: a tile as a pattern's, a dash in it for the number (lit while
 * the track is on it); the rests between patterns stay a bare dash (design/mock_rest.py, B) */
#define SG_X(p) (22 + ((int32_t)(p) * 134 + 5) / 10)   /* entry p's tile, 13 x 18, 13.4 apart: x 22 .. 235 */
#define SG_BAND_W 237
#define SG_W 13
#define SG_SIX (NTRK > 4)                             /* ((Skiff U3, N1) every track: 20 px bands, 22 apart, 15 px tiles;
                                                       * four tracks: 24, 27, 18 as before) */
#define SG_TH (SG_SIX ? 15 : 18)
#define SG_Y(r) (4 + (SG_SIX ? 22 : 27) * (int32_t)(r))   /* row r's band; the tile 2 px into it */
#define SG_H (SG_SIX ? 20 : 24)
static void graph_song(void)
{
#if FELUCCA_CAFA
    if(cafa_native_profile_view) {
        uint32_t k,p,count=cafa_native_u14(cafa_native_profile_view+15),selected=cafa_native_u14(cafa_native_profile_view+13);
        const uint8_t *rows=cafa_nv_section(CAFA_NATIVE_ROWS,0);
        if(rows)count=cafa_nv_track_lanes();
        else if(count>CAFA_TRACK_LANES)count=CAFA_TRACK_LANES;
        cv_rect(0,0,240,(int32_t)cv_h,T_BG);cv_bg=T_BG;
        for(k=0;k<count;k++) {
            const uint8_t *row=rows?cafa_nv_row(k):0;
            uint32_t track=row?cafa_native_u14(row+41):k,sel=track==selected;
            uint32_t copy_at=row && sel?cafa_native_u14(row+43):0u;
            uint32_t copy_count=row && sel?cafa_native_u14(row+45):0u;
            uint32_t chains=0u,starts=0u;
            int32_t y=SG_Y(k),ty=y+2;uint16_t col=T_TRK(track),g=sel?col:T_BG;
            uint16_t band[2]={col,ux_mix(col,NE_VIOLET,70)},chain_ground=ux_mix(T_BG,col,35);
            char number[6];
            if(copy_at>=NENT || copy_count>NENT-copy_at)copy_count=0u;
            if(sel) {
                if(ux.neon)ne_hgrad(0,y,SG_BAND_W,SG_H,band,2);
                else cv_rect(0,y,SG_BAND_W,SG_H,col);
            }
            fmt_int(number,(int32_t)track+1);
            if(track<9u)cv_text_on(5,y+CAP_IN(M,SG_H),&AF_M,number,sel?T_INK:col,g);
            else cv_text_fit(3,y+CAP_IN(S,SG_H),&AF_S,number,sel?T_INK:col,g,SG_X(0)-4);
            for(p=0;p<NENT;p++) {
                const uint8_t *cell=cafa_nv_cell(k*NENT+p);
                if(cell && (cell[2]&32u)) {
                    chains|=1u<<p;
                    if(cell[2]&64u)starts|=1u<<p;
                }
            }
            for(p=0;p<NENT;p++)if(chains&(1u<<p)) {
                int32_t left=!(starts&(1u<<p))?-1:p && (chains&(1u<<(p-1u)))?2:-2;
                int32_t right=(chains&(2u<<p))?(starts&(2u<<p))?10:13:14;
                cv_rect(SG_X(p)+left,y,right-left+1,SG_H,chain_ground);
            }
            for(p=0;p<NENT;p++) {
                const uint8_t *cell=cafa_nv_cell(k*NENT+p);uint32_t flags=cell?cell[2]:0u;
                int32_t x=SG_X(p);char value[5];
                int preview=p>=copy_at && p<copy_at+copy_count && (flags&1u);
                int chain=flags&32u;
                uint16_t ground=chain?chain_ground:g, mark=sel && !chain?T_INK:col;
                unsigned repeats=cell?cafa_native_u14(cell):0u;
                if(cell)cafa_native_text(value,cell+3,4);else value[0]=0;
                int rest=str_eq(value,"-") || str_eq(value,"REST");
                if(flags&1u) {
                    uint16_t fill=flags&2u?T_TEXT:flags&4u?mark:sel?ux_mix(col,T_INK,35):T_SURF;
                    uint16_t ink=flags&2u?T_INK:flags&4u?(mark==col?T_INK:col):sel?T_INK:T_MID;
                    cv_rrect(x,ty,SG_W,SG_TH,preview?0:2,fill,ground);
                    if(rest)cv_rect(x+3,ty+SG_TH/2-1,SG_W-6,2,ink);
                    else cv_text_in(x,ty+CAP_IN(S,SG_TH),SG_W,&AF_S,value,ink,fill);
                    if(flags&8u)for(uint32_t corner=0;corner<4u;corner++)
                        cv_rect(x+9+(int32_t)corner,ty,1,(int32_t)corner+1,
                                preview && ux.neon && !chain?
                                ne_ramp(band,2,(x+9+(int32_t)corner)*1000/(SG_BAND_W-1)):ground);
                } else if(rest)cv_rect(x+3,ty+SG_TH/2-1,SG_W-6,2,flags&4u?mark:T_DIM);
                else cv_rect(x+5,ty+SG_TH/2,2,2,sel?T_INK:T_DIM);
                if(preview || (flags&16u))cv_frame(x-1,ty-1,SG_W+2,SG_TH+2,preview?T_TEXT:mark);
                if((flags&2u) && !(flags&1u))cv_frame(x,ty,SG_W,SG_TH,T_TEXT);
                if((flags&1u) || rest)
                    for(unsigned tick=0;tick+1u<repeats && tick<3u;tick++)
                        cv_rect(x+1+3*(int32_t)tick,ty+SG_TH+1,2,1,mark);
            }
        }
        return;
    }
#endif
    uint16_t g0 = T_BG;
    uint32_t k, p, r, pos;
    cv_rect(0, 0, 240, (int32_t)cv_h, T_BG);         /* (over the panel, as HOME: the rows are on the ground) */
    cv_bg = T_BG;
    for (pos = 0, k = 0; k < NTRK; pos++, k++) {        /* ((Skiff U3, N1) every track) */
        int32_t y = SG_Y(pos), ty = y + 2;
        int band = k == song.sel % NTRK, cb = band;
        uint16_t col = T_TRK(k), g = cb ? col : g0;
        uint32_t cur = bank_cur(k), nx = bank_next(k), first, last, rest = trk_pid[k] ? NENT : lists.cur[k] % NENT;
        uint32_t ce = sg.entry % NENT, cp = band && song_pick() == SPICK_COPYP ? ent_free_after(k, ce) : NENT, cn = 0,
                 b0 = 0, b1 = 0, cm = 0, cs = 0, c0, c1;
        uint8_t ci[NENT];                            /* ((2.0, #33) COPY+: where its copies go ((#44) a chain's, part by
                                                      * part), their numbers */
        uint16_t st[2] = {col, ux_mix(col, NE_VIOLET, 70)}, bx = ux_mix(g0, col, 35);
        char d[2] = {(char)('1' + k), 0};
        if (cp < NENT) {
            cn = copy_ids(k, ce, ci);
            ent_block(k, ce, &b0, &b1);
            for (p = 0; p < cn; p++)
                if (ci[p] > NPAT)                    /* (no number for one: nothing shown) */
                    cp = NENT;
        }
        for (first = 0; first < NENT && !ent_on(k, first); first++)   /* ((1.2.1, #12) the empty entries between */
            ;                                                      /* these two are rests; (#28) a REST is one too */
        for (last = first < NENT ? NENT - 1u : 0u; last > first && !ent_on(k, last); last--)
            ;
        if (cb && ux.neon)                           /* ((Skiff S1) NEON: the band its colour into violet) */
            ne_hgrad(0, y, SG_BAND_W, SG_H, st, 2);
        else if (cb)
            cv_rect(0, y, SG_BAND_W, SG_H, col);
        else if (band)
            cv_rrect(3, y, 234, SG_H, 3, T_RAISE, g0);
        GFX_HOOK_ALIGN(0, y, 0, y + SG_H, AL_V, "song track number on its row's middle");
        cv_text_on(5, y + CAP_IN(M, SG_H), &AF_M, d, cb ? T_INK : col, g);
        for (p = 0; p < NENT; p = c1 + 1u)           /* ((2.0, #44, the chain sheet's A) a chain: its tiles on a box of
                                                      * their own, the band's height and 2 px round them, the track's
                                                      * colour dimmed (on the band too); COPY+'s copies of one too */
            if (ent_block(k, p, &c0, &c1))
                cm |= (2u << c1) - (1u << c0), cs |= 1u << c0;
        if (cp < NENT && cn > 1u)
            cm |= ((1u << cn) - 1u) << cp, cs |= 1u << cp;
        for (p = 0; p < NENT; p++)                   /* (each part's piece of it; where two meet, 2 px inside their tiles:
                                                      * a notch between them) */
            if ((cm >> p) & 1u) {
                int32_t a = !((cs >> p) & 1u) ? -1 : p && ((cm >> (p - 1u)) & 1u) ? 2 : -2,
                        z = p + 1u < NENT && ((cm >> (p + 1u)) & 1u) ? ((cs >> (p + 1u)) & 1u ? 10 : 13) : 14;
                cv_rect(SG_X(p) + a, y, z - a + 1, SG_H, bx);
            }
        for (p = 0; p < NENT; p++) {
            int32_t x = SG_X(p);
            int cpy = p >= cp && p < cp + cn,        /* ((2.0) COPY+'s copy here: of its chain's part p - cp) */
                in = (int)((cm >> p) & 1u);          /* (on a chain's box: it the ground, the marks the track's colour) */
            uint16_t gi = in ? bx : g, mk = cb && !in ? T_INK : col;
            uint32_t src = cpy ? b0 + p - cp : p, id = !cpy ? ent_pat(k, p) : ci[p - cp] ? ci[p - cp] - 1u : PAT_NONE,
                     reps = ent_reps(k, src);        /* ((2.0, #44) on CHAIN: once, no tick) */
            int pick = band && p == ce, rt = cpy ? !ci[p - cp] : ent_rest(k, p), on;
            uint16_t fill, ink;
            char s[3];
            if (id == PAT_NONE && !rt) {             /* empty: a dot; ((1.2.1, #12) a rest: a dash, lit while it plays, */
                uint16_t dc = cb ? ux_mix(col, T_INK, 55) : T_DIM;   /* framed while staged, its repeats under it) */
                if (p > first && p < last) {
                    cv_rect(x + 3, ty + SG_TH / 2 - 1, SG_W - 6, 2, p == rest ? (cb ? T_INK : col) : dc);
                    if (p == nx)
                        cv_frame(x - 1, ty - 1, SG_W + 2, SG_TH + 2, cb ? T_INK : col);
                    for (r = 0; r + 1u < reps && r < 3u; r++)
                        cv_rect(x + 1 + 3 * (int32_t)r, ty + SG_TH + 1, 2, 1, cb ? T_INK : col);
                } else
                    cv_rect(x + 5, ty + SG_TH / 2 - 1, 2, 2, dc);
                if (pick)
                    cv_frame(x, ty, SG_W, SG_TH, T_TEXT);
                continue;
            }
            on = !cpy && (p == cur || (rt && p == rest));
            if (pick)
                fill = T_TEXT, ink = T_INK;
            else if (on)
                fill = cb ? T_INK : col, ink = cb ? col : T_INK;
            else if (cb)
                fill = ux_mix(col, T_INK, 35), ink = T_INK;
            else
                fill = T_SURF, ink = T_MID;
            if (ux.neon && !cb && on)                /* ((Skiff S1) NEON: the entry playing a lit tube; SURF ones glass, */
                ne_tube(x, ty, SG_W, SG_TH, fill, gi);   /* CRYPT's stone (cv_rrect)) */
            else
                cv_rrect(x, ty, SG_W, SG_TH, 0, fill, gi);
            if (rt)
                cv_rect(x + 3, ty + SG_TH / 2 - 1, SG_W - 6, 2, ink);
            else {
                pat_num(s, id);
                GFX_HOOK_ALIGN(x, ty, x + SG_W, ty + SG_TH, AL_HV, "song entry number centred");
                cv_text_in(x, ty + CAP_IN(S, SG_TH), SG_W, &AF_S, s, ink, fill);
            }
            if (lists.snd[k][src][0])                /* ((2.0, #30, the user's pick 2A) an entry with a sound: its tile's
                                                      * top right corner cut, a 4 px step, to the ground) */
                for (r = 0; r < 4u; r++)
                    cv_rect(x + 9 + (int32_t)r, ty, 1, (int32_t)r + 1,
                            cb && ux.neon && !in ? ne_ramp(st, 2, (x + 9 + (int32_t)r) * 1000 / (SG_BAND_W - 1)) : gi);
            if (p == nx || cpy)                      /* staged: comes in at its launch point; ((2.0) COPY+'s: TEXT) */
                cv_frame(x - 1, ty - 1, SG_W + 2, SG_TH + 2, cpy ? T_TEXT : mk);
            for (r = 0; r + 1u < reps && r < 3u; r++)
                cv_rect(x + 1 + 3 * (int32_t)r, ty + SG_TH + 1, 2, 1, mk);
        }
    }
}
/* SEQ > AUTOMATION (ui_events.c; 1.1.5's AUTO LIST): the track's records around the one selected, a row each: the kind
 * (LOCK in the accent, AUTO), the step, the parameter's name, the value at the right; the last row + ADD LOCK. (Skiff
 * U3, R5) The first row is the page's own: PLAY ON / OFF and how many records */
static void graph_events(void)
{
#if FELUCCA_CAFA
    if(cafa_native_profile_view) {
        uint32_t k,selected=cafa_native_u14(cafa_native_profile_view+24),first=cafa_native_u14(cafa_native_profile_view+26);
        for(k=0;k<7u;k++) {
            const uint8_t *r=cafa_nv_row(k);char kind[9],step[9],name[17],value[17];
            int32_t y=LIST_Y(k);int sel=first+k==selected;uint16_t bg=sel?T_THEME:T_SURF;
            if(!r)continue;
            cafa_native_text(kind,r+32,8);cafa_native_text(step,r,8);
            cafa_native_text(name,r+8,8);cafa_native_text(value,r+16,16);
            if(sel)list_bar(y);
            cv_text_on(14,y+CAP_IN(S,16),&AF_S,kind,sel?T_INK:(r[40]&64u)?T_ACCENT:T_MID,bg);
            cv_text_on(54,y+CAP_IN(S,16),&AF_S,step,sel?T_INK:T_MID,bg);
            cv_free_text(80,y+CAP_IN(S,16),&AF_S,name,sel?T_INK:T_TEXT,bg,100);
            cv_text_r(226,y+CAP_IN(S,16),&AF_S,value,sel?T_INK:T_THEME,bg);
        }
        return;
    }
#endif
    const track_t *t = TSEL;
    uint8_t idx[MOTION_MAX];
    uint32_t n = ev_rows(idx), k;
    int32_t cur = ui.ev_top ? -1 : (int32_t)ev_row(n), row, first = clamp(cur - 3, -1, (int32_t)n + 2 > 7 ? (int32_t)n - 6 : -1);
    for (row = 0; row < 7 && first + row <= (int32_t)n; row++) {
        int32_t y = LIST_Y(row);
        int sel;
        uint16_t bg;
        sel = first + row == cur;
        bg = sel ? T_THEME : T_SURF;
        if (sel)
            list_bar(y);
        if (first + row < 0) {                       /* the page's row */
            char b[16];
            fmt_int(b, (int32_t)n);
            str_cpy(b + str_len(b), n == 1u ? " RECORD" : " RECORDS", sizeof b - str_len(b));
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + CAP_IN(S, 16), &AF_S, motion_enabled(t) ? "PLAY ON" : "PLAY OFF", sel ? T_INK : T_TEXT, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_r(226, y + CAP_IN(S, 16), &AF_S, b, sel ? T_INK : T_MID, bg);
            continue;
        }
        k = (uint32_t)(first + row);
        if (k == n) {
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + CAP_IN(S, 16), &AF_S, "+ ADD LOCK", sel ? T_INK : T_DIM, bg);
            break;
        }
        {
            const motion_event_t *e = &motion.event[idx[k]];
            uint32_t id = MOTION_ID(e), lock = e->lock;
            char b[16], nm[12];
            const char *unit;
            const param_desc_t *d = track_desc(t, id);
            b[0] = (char)('0' + ((e->place & 63u) + 1u) / 10u);
            b[1] = (char)('0' + ((e->place & 63u) + 1u) % 10u);
            b[2] = 0;
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(14, y + CAP_IN(S, 16), &AF_S, lock ? "LOCK" : "AUTO", sel ? T_INK : lock ? T_ACCENT : T_MID, bg);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            cv_text_on(54, y + CAP_IN(S, 16), &AF_S, b, sel ? T_INK : T_MID, bg);
            param_format(d, e->value, b, &unit);
            if (unit && unit[0] && str_len(b) + str_len(unit) < sizeof b)
                str_cpy(b + str_len(b), unit, sizeof b - str_len(b));
            ev_name(t, id, nm);
            GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
            {
                int32_t nx = cv_free_text(80, y + CAP_IN(S, 16), &AF_S, nm, sel ? T_INK : T_TEXT, bg,
                                          226 - text_w(&AF_S, b) - 8 - 80);
                GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
                cv_text_r(226, y + CAP_IN(S, 16), &AF_S, b, sel ? T_INK : ux.neon ? T_ACCENT : T_TEXT, bg);
                if ((ux.crypt || ux.neon || ux.quark) && !sel)   /* ((Skiff S1) a dotted leader to the value) */
                    cr_leader(nx + 4, 226 - text_w(&AF_S, b) - 4, y + 11);
            }
        }
    }
}
/* project slots: the name (none: USED) / EMPTY, the selected one filled */
static void graph_slots(void)
{
#if FELUCCA_CAFA
    if(cafa_native_profile_view) {
        uint32_t i,selected=cafa_native_u14(cafa_native_profile_view+24);
        for(i=0;i<4u;i++) {
            const uint8_t *r=cafa_nv_row(i);char tag[9],name[17];
            if(!r)continue;
            cafa_native_text(tag,r+32,8);cafa_native_text(name,r,16);
            list_row(12+(int32_t)i*26,i==selected,tag,T_MID,name,(r[40]&16u)?T_DIM:T_TEXT,232);
        }
        return;
    }
#endif
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int32_t y = 12 + (int32_t)i * 26;
        char b[4];
        int sel = (int32_t)i + 1 == song.g[G_SLOT], used = graph_project_used(i);
        const char *n = graph_project_name(i);
        b[0] = (char)('A' + i);
        b[1] = 0;
        list_row(y, sel, b, T_MID, !used ? "--" : n[0] ? n : "USED", used ? T_TEXT : T_DIM, 232);
    }
}
/* MIXER page: four SURF columns, one under each card: the track number on its cushion (in the accent:
 * the selected track) with a REC / ARM / MUTE badge (P_MUTE, KNOB 1), the sound's short name (a MUTE badge
 * when armed and muted), the LEVEL knob (dB inside) with the output meter beside it, then the PAN and REV
 * knobs with their values. Knobs: a 270 degree ring, the track RAISE, the value arc THEME (the knob just
 * turned: ACCENT; muted: DIM); PAN from 12 o'clock. Each column is its own canvas with its own signature:
 * while the transport runs only the meters move. */
#define TS_MY 39                                     /* meter slot: column rows 39 .. 70 */
#define TS_MH 32
#define KB_X 4                                       /* LEVEL knob: box 4..37 x 37..70, centre (21, 54) */
#define KB_Y 37
#define KS_Y 86                                      /* PAN / REV knobs: boxes 86..105, centres x 15 and 42 */
static struct {
    uint32_t col[NTRK];
    uint8_t meter[NTRK];
    uint8_t top;                                     /* ((Skiff phase 6) the window the scroll bar shows, + 1; 0 none) */
} ts;

static int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 60 dB over the slot */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);             /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB) */
    return clamp((v - 48) * (TS_MH - 2) / 80, 0, TS_MH - 2);
}

/* a knob's ring: 270 degrees (7:30 .. 4:30 o'clock), 2 px, anti-aliased, from the quadrant mask of its size
 * (tools/gen_aa_keycaps.py: coverage and angle; the other three quadrants mirror it, no trigonometry here).
 * Box top-left (x, y), outer radius r. Angles: 1/1024 turn from 12 o'clock, clockwise, -384 .. 384; the value
 * arc is lo .. hi (in vc), the rest of the ring the track (tr); bg lies under the ring */
#define KA_END 384
static void knob_arc(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t lo, int32_t hi,
                     uint16_t tr, uint16_t vc, uint16_t bg)
{
    const uint16_t *rt = ramp(tr, bg), *rv = ramp(vc, bg);
    int32_t i, j, m;
    for (j = 0; j < r; j++)
        for (i = 0; i < r; i++) {
            uint32_t k = (uint32_t)(j * r + i), a = (cov[k >> 1] >> ((k & 1u) ? 0 : 4)) & 15u;
            int32_t q = ang[k];
            if (!a)
                continue;
            for (m = 0; m < 4; m++) {                /* top right, bottom right, bottom left, top left */
                int32_t s = m == 0 ? q : m == 1 ? 512 - q : m == 2 ? q - 512 : -q;
                int32_t px = m < 2 ? x + r + i : x + r - 1 - i, py = (m == 0 || m == 3 ? y + r - 1 - j : y + r + j) + cv_oy;
                if (s < -KA_END || s > KA_END || (uint32_t)px >= cv_w || (uint32_t)py >= cv_h)
                    continue;
                cv_px[(uint32_t)py * cv_w + (uint32_t)px] = (s >= lo && s <= hi ? rv : rt)[a];
            }
        }
}
/* value v of lo..hi as an arc: from the start, or (bipolar, lo < 0) from 12 o'clock with a 1 px nub at 0 */
static void knob(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t v, int32_t lo,
                 int32_t hi, uint16_t vc)
{
    int32_t a0, a1;
    if (lo < 0) {
        int32_t s = v * KA_END / (v < 0 ? -lo : hi);
        a0 = (s < 0 ? s : 0) - 8;
        a1 = (s > 0 ? s : 0) + 8;
    } else {
        a0 = -KA_END;
        a1 = v > lo ? -KA_END + (v - lo) * 2 * KA_END / (hi - lo) : -KA_END - 1;
    }
    knob_arc(x, y, r, cov, ang, a0, a1, T_RAISE, vc, T_SURF);
}

static uint32_t trk_level(uint32_t c) { return (uint32_t)trk[c].p[P_LEVEL] & 127u; }   /* LEVEL 0..127 */

static void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, e->presets[t->preset % e->npresets].name, 13);
    else
        str_cpy(b, e->name, 13);
}

/* MENU > LARGE: the four tracks in the strip under the tall cards (57 x LG_H_GRAPH each): the cushion (the selected
 * one the accent), REC / ARM / MUTE as on the full mixer, the sound's name, LEVEL as a gauge (THEME: the cards'),
 * the output meter under it (MID); a muted track DIM */
#define TSS_GY 40                                    /* the LEVEL gauge, 3 px */
#define TSS_MY 48                                    /* the meter, 3 px */
static void track_strip(uint32_t c, uint32_t pos, uint32_t sel, uint32_t st, uint32_t mute, uint32_t arm, uint32_t hot,
                        uint32_t lvl, const char *b)
{
    int32_t gw = CARD_W - 10, m = ts.meter[c] * gw / (TS_MH - 2), fx = (int32_t)lvl * gw / 127;
    uint16_t vc = mute ? T_DIM : T_THEME;
    cv_begin(CARD_W, graph_h(), T_BG);              /* ((Skiff) BLOCKS: LG_H_GRAPH and the footer's line) */
    cv_rrect(0, 0, CARD_W, graph_h(), 5, T_SURF, T_BG);
    cv_icon_on(4, 4, 16, trk_icon(c, sel), sel ? T_ACCENT : T_MID, T_SURF);
    if (st)
        GFX_HOOK_ALIGN(0, 4, 0, 20, AL_V, "mixer badge on the cushion's line");
    if (st == 1u || st == 2u)
        cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 5, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                  T_INK, T_SURF);
    else if (st)
        cv_keycap(53 - kc_w(KC_MUTE), 5, KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
    if (mute && arm)                                 /* armed and muted: MUTE in place of the name */
        GFX_HOOK_ALIGN(0, 22 + AF_S_CAP_Y, 0, 22 + AF_S_CAP_Y + AF_S_CAP_H, AL_V, "mixer MUTE badge on the name's line");
    if (mute && arm)
        cv_keycap(5, 22 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
    else
        cv_free_text(5, 22, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
    cv_rrect(5, TSS_GY, gw, 3, 1, T_BG, T_SURF);
    cv_rrect(5, TSS_GY, fx < 3 ? 3 : fx, 3, 1, hot == 1u ? T_ACCENT : vc, T_BG);
    cv_rrect(5, TSS_MY, gw, 3, 1, T_RAISE, T_SURF);
    if (m)
        cv_rrect(5, TSS_MY, m < 3 ? 3 : m, 3, 1, T_MID, T_RAISE);
    cv_blit((uint32_t)CARD_X(pos), LG_Y_GRAPH);
}
/* (Skiff U3, N1; design/mock_u3.py, the user's pick) six strips of MS_W at the layer maps' columns, every track at
 * once: the number (V; DIM muted), its state at the right (REC recording: a square in REC; armed, stopped: its outline
 * in the accent; muted: M, the accent when KNOB 4 just turned it), the sound's first word, LEVEL a fader (0 dB at its
 * top, the cards' THEME; just turned: the accent) with the output meter beside it, its dB under it, the PAN (L31, C,
 * R37). REV is the cards' (the selected track's K3). The selected track's strip a band of its colour, all on it INK,
 * as HOME's lanes. MENU > LARGE (the strip under the tall cards): the number, its state, the name, LEVEL as a gauge,
 * the meter under it */
#define MS_W 36
#define MS_X(c) (4 + 39 * (int32_t)(c))
#define MS_FY0 52                                    /* the fader: y 52 .. 101 */
#define MS_FY1 102
#if FELUCCA_CAFA
static uint32_t cafa_mix_level(const uint8_t *r)
{
    return (cafa_native_u14(r+41u)*127u+8191u)/16383u;
}
static int32_t cafa_mix_pan(const uint8_t *r)
{
    return (int32_t)((cafa_native_u14(r+43u)*127u+8191u)/16383u)-64;
}
static int32_t cafa_mix_meter(const uint8_t *cell)
{
    return cell ? (int32_t)((cafa_native_u14(cell)*(TS_MH-2)+8191u)/16383u) : 0;
}
static uint32_t cafa_mix_status(const uint8_t *r, const uint8_t *cell)
{
    return cell && (cell[2]&4u) ? 1u : cell && (cell[2]&1u) ? 2u : r[40]&4u ? 3u : 0u;
}
#endif
static void mix_strip(uint32_t c, uint32_t track, uint32_t sel, uint32_t st, uint32_t mute, uint32_t hot, uint32_t lvl, int32_t pan,
                      const char *b, int large)
{
    int ib = sel && !ux.quark;                       /* (the selected strip a band, INK on it; (Skiff Q3) QUARK: a tint) */
    uint16_t col = T_TRK(track), g = sel ? (ux.quark ? ux_mix(col, T_BG, 82) : col) : T_SURF, line = ib ? ux_mix(col, T_INK, 55) : T_RAISE;
    uint16_t vc = ib ? T_INK : hot == 1u ? T_ACCENT : mute ? T_DIM : T_THEME, nc = ib ? T_INK : mute ? T_DIM : col;
    int32_t h = (int32_t)graph_h(), m = 0, i;
#if FELUCCA_CAFA
    if(cafa_native_profile_view) {
        m=cafa_mix_meter(cafa_nv_cell(c));
    } else
#endif
    m=ts.meter[c];
    char d[6], nm[8], v[8];
    fmt_int(d,(int32_t)track+1);
    cv_begin(MS_W, (uint32_t)h, T_BG);
    if (ux.crypt && !sel)                            /* (Skiff C3) CRYPT: the strips stone */
        cr_stone(0, 0, MS_W, h, g);
    else if (ux.neon && sel) {                       /* (Skiff N3) NEON: the selected one its colour into violet, the */
        uint16_t st[2] = {col, ux_mix(col, NE_VIOLET, 75)};   /* others glass */
        ne_vgrad(0, 0, MS_W, h, st, 2);
    } else if (ux.neon)
        ne_glass(0, 0, MS_W, h, g);
    else if (ux.quark) {                             /* (Skiff Q3) QUARK: the selected one in its colour's corners */
        cv_rect(0, 0, MS_W, h, g);
        if (sel)
            qk_corners(0, 0, MS_W, h, col, 6);
    } else
        cv_rect(0, 0, MS_W, h, g);
    GFX_HOOK_ALIGN(0, 3, 0, 27, AL_V, "mixer strip number on its line");
    if (track < 9u)
        cv_text_on(4, 3 + CAP_IN(V, 24), &AF_V, d, nc, g);
    else
        cv_text_fit(4, 3 + CAP_IN(M, 24), &AF_M, d, nc, g, MS_W - 15);
    if (st == 1u)                                    /* REC (recording) / armed, stopped / muted */
        cv_rect(MS_W - 11, 10, 7, 7, ib ? T_INK : T_REC);
    else if (st == 2u)
        cv_frame(MS_W - 11, 10, 7, 7, ib ? T_INK : T_ACCENT);
    else if (st == 3u && ux.crypt)                   /* (Skiff C3) CRYPT: muted, a crossed skull */
        cr_skull(MS_W - 12, 10, ib ? T_INK : hot == 4u ? T_ACCENT : T_MID, g, 1);
    else if (st == 3u) {
        GFX_HOOK_ALIGN(0, 3, 0, 27, AL_V, "mixer strip number on its line");
        cv_text_r(MS_W - 4, 3 + CAP_IN(S, 24), &AF_S, "M", ib ? T_INK : hot == 4u ? T_ACCENT : T_MID, g);
    }
    for (i = 0; b[i] && b[i] != ' ' && i < 7; i++)    /* the sound's first word */
        nm[i] = b[i];
    nm[i] = 0;
    cv_free_text(4, 29, &AF_S, nm, ib ? T_INK : mute ? T_DIM : T_TEXT, g, MS_W - 6);
    if (large) {                                     /* LARGE: LEVEL a gauge, the meter under it */
        int32_t gw = MS_W - 8, fx = (int32_t)lvl * gw / 127;
        cv_rect(4, 46, gw, 3, line);
        cv_rect(4, 46, fx < 2 ? 2 : fx, 3, vc);
        cv_rect(4, 53, gw, 3, line);
        if (m)
            cv_rect(4, 53, m * gw / (TS_MH - 2), 3, ib ? T_INK : T_MID);
        cv_blit((uint32_t)MS_X(c), LG_Y_GRAPH);
        return;
    }
    {   /* LEVEL: the fader from its foot, its cap; the meter beside it */
        int32_t fh = (int32_t)lvl * (MS_FY1 - MS_FY0) / 127, mh = m * (MS_FY1 - MS_FY0) / (TS_MH - 2);
        if (ux.quark && mute) {                       /* (Skiff Q3) QUARK: muted, dark matter: there, nothing shows */
            qk_dotframe(10, MS_FY0, 16, MS_FY1 - MS_FY0, T_DIM);
            GFX_HOOK_ALIGN(0, 0, MS_W, 0, AL_H, "mixer strip value centred across");
            cv_text_in(0, MS_FY0 + 20, MS_W, &AF_S, "DARK", T_DIM, g);
        } else if (ux.quark) {                        /* (Skiff Q3) QUARK: a calorimeter tower, its cells the energy
                                                       * deposited (the level), a gold cap; the meter beside it */
            int32_t n = 12, lit = (fh * n + 25) / (MS_FY1 - MS_FY0), s;
            for (s = 0; s < n; s++)
                cv_rect(10, MS_FY1 - (s + 1) * 4, 14, 3, s < lit ? ux_mix(col, T_TEXT, s * 50 / n) : ux_mix(T_SURF, T_TEXT, 10));
            cv_rect(8, MS_FY1 - lit * 4 - 2, 18, 1, hot == 1u ? T_TEXT : T_ACCENT);
            if (mh)
                cv_rect(27, MS_FY1 - mh, 2, mh, T_THEME);
        } else if (ux.neon && mute) {                        /* (Skiff N3) NEON: muted, no signal (selected: dark snow on its
                                                       * band) */
            ne_snow(3, MS_FY0, MS_W - 6, MS_FY1 - MS_FY0, track * 7u + 3u, sel ? col : T_BG, ib ? T_INK : T_MID);
            GFX_HOOK_ALIGN(0, 0, MS_W, 0, AL_H, "mixer strip value centred across");
            cv_text_in(0, MS_FY0 + 22, MS_W, &AF_S, "MUTE", ib ? T_INK : T_THEME, g);
        } else if (ux.neon) {                         /* (Skiff N3) NEON: VU columns, LEVEL (its cap white; turned: the
                                                       * accent) and the meter */
            ne_vu(8, MS_FY1, 9, MS_FY1 - MS_FY0, fh, ib ? T_INK : 0, sel ? line : ux_mix(T_SURF, T_TEXT, 10),
                  sel ? 0 : hot == 1u ? T_ACCENT : T_TEXT);
            ne_vu(19, MS_FY1, 9, MS_FY1 - MS_FY0, mh, ib ? T_INK : 0, sel ? line : ux_mix(T_SURF, T_TEXT, 10), 0);
        } else
        cv_rect(13, MS_FY0, 6, MS_FY1 - MS_FY0, line);
        if (ux.neon || ux.quark)
            ;
        else if (ux.crypt && !sel) {                  /* (Skiff) CRYPT: a torch as tall as the level (muted: out) */
            cr_meter(13, MS_FY1, fh, mute);
        } else {
        if (fh)
            cv_rect(13, MS_FY1 - fh, 6, fh, vc);
        cv_rect(10, MS_FY1 - fh - 1, 12, 2, ib ? T_INK : T_TEXT);
        }
        if (!ux.neon && !ux.quark) {
        cv_rect(23, MS_FY0, 2, MS_FY1 - MS_FY0, line);
        if (mh)
            cv_rect(23, MS_FY1 - mh, 2, mh, ib ? T_INK : T_MID);
        }
    }
    if (lvl) {                                       /* its dB (OFF at 0), the PAN */
        int32_t dd = LEVEL_DB_X10(lvl);
        fmt_int(v, (dd + (dd < 0 ? -5 : 5)) / 10);
    } else {
        str_cpy(v, "OFF", sizeof v);
    }
    GFX_HOOK_ALIGN(0, 0, MS_W, 0, AL_H, "mixer strip value centred across");
    cv_text_in(0, 106, MS_W, &AF_M, v, ib ? T_INK : hot == 1u ? T_ACCENT : lvl && !mute ? T_TEXT : T_DIM, g);
    pan_txt(v, pan);
    GFX_HOOK_ALIGN(0, 0, MS_W, 0, AL_H, "mixer strip value centred across");
    cv_text_in(0, 123, MS_W, &AF_S, v, ib ? T_INK : hot == 2u ? T_ACCENT : T_MID, g);
    cv_blit((uint32_t)MS_X(c), Y_GRAPH);
}
static void draw_tracks(void)
{
    uint32_t c, pos, sk;
#if FELUCCA_CAFA
    if(cafa_native_profile_view) {
        uint32_t first=cafa_native_u14(cafa_native_profile_view+26),selected=cafa_native_u14(cafa_native_profile_view+24);
        uint32_t count=cafa_nv_track_lanes();
        lcd_fill(0,graph_y(),240,graph_h(),T_BG);
        for(c=0;c<count;c++) {
            const uint8_t *r=cafa_nv_row(c);char name[17];
            uint32_t flags=r?r[40]:0u,lvl=r?cafa_mix_level(r):0u;
            int32_t pan=r?cafa_mix_pan(r):0;
            if(r)cafa_native_text(name,r,16);else name[0]=0;
            mix_strip(c,first+c,first+c==selected,r?cafa_mix_status(r,cafa_nv_cell(c)):0u,
                      flags&4u,0u,lvl,pan,name,large_kind()==LK_TALL);
        }
        return;
    }
#endif
    sk=strip_kind()==SK_TRK;
    if (ui.force) {
        if (NTRK > 4u)                               /* the six strips are the panel's full height: only the gaps */
            for (c = 0, pos = 0; c <= NTRK; c++) {   /* between them, so a track change doesn't flash them (#15) */
                int32_t r = c < NTRK ? MS_X(c) : 240;
                lcd_fill(pos, graph_y(), (uint32_t)r - pos, graph_h(), T_BG);
                pos = (uint32_t)(r + MS_W);
            }
        else
            lcd_fill(0, graph_y(), 240, graph_h(), T_BG);
        for (c = 0; c < NTRK; c++)
            ts.meter[c] = 0;
    }
    for (c = 0; c < NTRK; c++) {                     /* every track's meter, shown or not ((Skiff phase 6) one scrolled */
        int32_t m = trk[c].p[P_MUTE] ? 0 : meter_px(trk[c].peak);   /* in shows no old peak) */
        trk[c].peak = 0;
        if (m < ts.meter[c] - 1)
            m = ts.meter[c] - 1;                     /* falls ~2 dB a frame */
        ts.meter[c] = (uint8_t)(m < 0 ? 0 : m);
    }
    for (pos = 0, c = NTRK > 4u ? 0u : trk_top(); pos < (NTRK > 4u ? NTRK : 4u) && c < NTRK; pos++, c++) {   /* ((U3, N1)
                                                                            * every track; four: as phase 6) */
        track_t *t = &trk[c];
        uint32_t sel = c == song.sel, lvl = trk_level(c), mute = t->p[P_MUTE] != 0;
        uint32_t arm = (song.rec >> c) & 1u, st = arm ? (song.playing ? 1u : 2u) : mute ? 3u : 0u, sig;
        uint32_t hot = sel && ui.hot_t ? ui.hot_col + 1u : 0u;   /* the knob just turned (hot_col + 1): 1 LEVEL, 2 PAN,
                                                                * 3 REV, 4 MUTE (its badge) */
        int32_t pan = clamp(t->p[P_PAN], -64, 63), rv = clamp(t->p[P_REV], 0, 127);
        uint16_t vc = mute ? T_DIM : T_THEME;
        char b[16];
        trk_short_name(c, b);
        sig = str_hash(1u + sel + st * 2u + (mute && arm) * 16u + hot * 32u, b) + lvl * 7919u +
              ts.meter[c] * 131u + (uint32_t)(pan + 128) * 104729u + (uint32_t)rv * 1299709u + mute * 3u + sk * 0x9E37u +
              c * 2654435761u;
        if (!ui.force && sig == ts.col[pos])
            continue;
        ts.col[pos] = sig;
        if (NTRK > 4u) {                             /* ((Skiff U3, N1) six strips) */
            mix_strip(c, c, sel, st, mute, hot, lvl, pan, b, (int)sk);
            continue;
        }
        if (sk) {
            track_strip(c, pos, sel, st, mute, arm, hot, lvl, b);
            continue;
        }
        cv_begin(CARD_W, graph_h(), T_BG);
        cv_rrect(0, 0, CARD_W, graph_h(), 5, T_SURF, T_BG);
        cv_oy = graph_pad();                         /* ((Skiff) BLOCKS: the taller panel, the strip centred in it) */
        cv_icon_on(4, 5, 16, trk_icon(c, sel), sel ? T_ACCENT : T_MID, T_SURF);
        if (st)
            GFX_HOOK_ALIGN(0, 5, 0, 21, AL_V, "mixer badge on the cushion's line");
        if (st == 1u || st == 2u)                    /* REC (recording) / ARM (armed, stopped) */
            cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 6, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                      T_INK, T_SURF);
        else if (st)                                 /* muted (K4 just turned: lit) */
            cv_keycap(53 - kc_w(KC_MUTE), 6, KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
        if (mute && arm)                             /* armed and muted: MUTE in place of the name */
            GFX_HOOK_ALIGN(0, 21 + AF_S_CAP_Y, 0, 21 + AF_S_CAP_Y + AF_S_CAP_H, AL_V,
                           "mixer MUTE badge on the name's line");
        if (mute && arm)                             /* (on the name's capitals) */
            cv_keycap(5, 21 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, hot == 4u ? T_ACCENT : T_KEY, T_INK, T_SURF);
        else
            cv_free_text(5, 21, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
        {   /* LEVEL: the knob, its dB inside (OFF at 0), and the meter of the output */
            char v[8];
            knob(KB_X, KB_Y, KNOB_BIG_R, KNOB_BIG_COV, KNOB_BIG_ANG, (int32_t)lvl, 0, 127, hot == 1u ? T_ACCENT : vc);
            if (lvl) {
                int32_t d = LEVEL_DB_X10(lvl);
                fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
                GFX_HOOK_ALIGN(KB_X, 0, KB_X + 2 * KNOB_BIG_R, 0, AL_H, "mixer level text centred across");
                cv_text_in(KB_X, KB_Y + KNOB_BIG_R + 5, 2 * KNOB_BIG_R, &AF_S, "dB", T_DIM, T_SURF);
            } else {
                str_cpy(v, "OFF", sizeof v);
            }
            GFX_HOOK_ALIGN(KB_X, KB_Y, KB_X + 2 * KNOB_BIG_R, KB_Y + 2 * KNOB_BIG_R, AL_HV, "mixer level value in its knob");
            cv_text_in(KB_X, KB_Y + CAP_IN(S, 2 * KNOB_BIG_R), 2 * KNOB_BIG_R, &AF_S, v, hot == 1u ? T_ACCENT : lvl ? vc : T_DIM, T_SURF);
            cv_rrect(45, TS_MY, 4, TS_MH, 2, T_RAISE, T_SURF);
            if (ts.meter[c])
                cv_rrect(45, TS_MY + TS_MH - 1 - ts.meter[c], 4, ts.meter[c] + 1, ts.meter[c] >= 4 ? 2 : 0, T_MID, T_RAISE);
        }
        {   /* PAN (from the centre) and the REV send: captions, knobs, values */
            char v[8];
            uint16_t pc = hot == 2u ? T_ACCENT : vc, rc = hot == 3u ? T_ACCENT : vc;
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(15 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "PAN", T_DIM, T_SURF);   /* (by their ink) */
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(42 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "REV", T_DIM, T_SURF);
            knob(15 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, pan, -64, 63, pc);
            knob(42 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, rv, 0, 127, rc);
            v[0] = '+';                              /* the cards' numbers, without the % */
            fmt_int(pan > 0 ? v + 1 : v, pan * 100 / (pan > 0 ? 63 : 64));
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(15 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, pc, T_SURF);
            fmt_int(v, (rv * 100 + 63) / 127);
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(42 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, rc, T_SURF);
        }
        cv_oy = 0;
        cv_blit((uint32_t)CARD_X(pos), Y_GRAPH);
    }
    if (NTRK <= 4u && (ui.force || ts.top != trk_top() + 1u)) {   /* ((Skiff phase 6) the scroll bar: 1 px under the strips, x 3 .. 236
                                                      * (BLOCKS y 215: design/mock_p6.py mixer); between them and the
                                                      * footer, on no one's canvas */
        ts.top = (uint8_t)(trk_top() + 1u);
        cv_begin(240, 2, T_BG);
        trk_bar(3, 0, 236, 1);
        cv_blit(0, graph_y() + graph_h() + 1u);
    }
}
/* the scope's own state (.pool: not among the globals the audio ISR reaches): the tracks' levels, 0..1000, falling
 * (QUARK's towers, CRYPT's creatures, SKIFF's sea); (2.5, QUARK) each voice's note as last seen (its age's low bits)
 * and when (fm1_ms's) */
static struct {
    uint32_t last;                                  /* the last draw's fm1_ms */
    int16_t lv[NTRK];
    uint16_t id[NTRK][NVOICE], t0[NTRK][NVOICE];
} scd __attribute__((section(".pool")));
static void scope_levels(void)
{
    uint32_t now = fm1_ms, k;
    int32_t fall = (int32_t)(now - scd.last > 500u ? 500u : now - scd.last) * 2;   /* (all the way down in 0.5 s) */
    scd.last = now;
    for (k = 0; k < NTRK; k++) {
#if FELUCCA_CAFA
        if(cafa_native_profile_view) {
            const uint8_t *r=cafa_nv_row(k);
            scd.lv[k]=r?(int16_t)(cafa_native_u14(r+41)*1000u/16383u):0;
            continue;
        }
#endif
        track_t *t = &trk[k];
        int32_t lv = t->p[P_MUTE] ? 0 : meter_px(t->peak) * 1000 / (TS_MH - 2);
        t->peak = 0;                                 /* (a max-hold, as the MIXER reads it) */
        scd.lv[k] = (int16_t)(lv > scd.lv[k] - fall ? lv : scd.lv[k] - fall);
    }
}

/* (Skiff Q6) QUARK: the scope as the 1.2 tour video's event display, live: each note a track plays is a collision at
 * the vertex, three particles flying out of it in the track's colour and curling in the field (ANIM OFF: they appear
 * whole); (2.5) they live as long as the note sounds: lit while it plays, fading with its envelope as it is released
 * (a hit's as its track's level falls), gone with its voice. Round the beam pipe's rings the calorimeter, eight towers
 * a track, as loud as it is (muted: dark); the vertex gold, brighter just after a hit */
static void qk_detector(int32_t cx, int32_t cy)
{
    int32_t R = cy - 3, k, j;
    uint32_t now = fm1_ms, newest = 1000u, first = 0u;
#if FELUCCA_CAFA
    if(cafa_native_profile_view)first=cafa_native_u14(cafa_native_profile_view+26u);
#endif
    qk_disc(cx, cy, R * 58 / 100, ux_mix(T_THEME, T_SURF, 93));   /* (the glow round the beam) */
    qk_ellipse(cx, cy, R * 19 / 100, R * 19 / 100, 0, ux_mix(T_THEME, T_SURF, 78), 24);
    qk_dotring(cx, cy, R * 37 / 100, 40, ux_mix(T_THEME, T_SURF, 66));
    qk_ellipse(cx, cy, R * 58 / 100, R * 58 / 100, 0, ux_mix(T_THEME, T_SURF, 70), 48);
    qk_dotring(cx, cy, R * 76 / 100, 72, ux_mix(T_THEME, T_SURF, 50));
    for (k = 0; k < 48; k++) {                       /* the calorimeter: the six tracks' towers in turn */
        uint32_t ph = (uint32_t)k * (0xFFFFFFFFu / 48u), r = (uint32_t)k * 2654435761u >> 24;   /* (r: its own reach) */
        int32_t lv = scd.lv[k % NTRK], r0 = R * 78 / 100, h = 1 + lv * (R * 22 / 100) * (600 + (int32_t)r * 400 / 255) / 1000000;
        int32_t co = QK_COS(ph), si = QK_SIN(ph);
        cv_line_t(cx + r0 * co / 32767, cy + r0 * si / 32767, cx + (r0 + h) * co / 32767, cy + (r0 + h) * si / 32767,
                  ux_mix(T_TRK(first + k % NTRK), T_SURF, 65 - lv * 65 / 1000), 2);
    }
#if FELUCCA_CAFA
    if(cafa_native_profile_view) {
        for(k=0;k<(int32_t)cafa_nv_track_lanes();k++){const uint8_t *r=cafa_nv_row(k);
            if(r && (r[40]&2u)){
                uint32_t track=first+(uint32_t)k;
                uint32_t pitch=cafa_native_u14(r+43),seed=(pitch*2654435761u)^(track*0x9E3779B9u);
                uint32_t part;newest=0u;
                for(part=0;part<3u;part++) {
                    uint32_t dir=(seed+part*0x55555555u)*2654435761u;
                    int32_t reach=R*(34+(int32_t)(pitch+part*13u)%46)/100;
                    uint32_t point=qk_track(cx,cy,dir,R/2,reach,part&1u?1:-1,T_TRK(track),T_SURF,reach);
                    cv_rect((int16_t)(point>>16),(int16_t)point-1,2,2,T_TRK(track));
                }
            }}
        goto qk_vertex;
    }
#endif
    for (k = 0; k < NTRK; k++)
        for (j = 0; j < (int32_t)NVOICE; j++) {      /* a sounding voice an event (UNISON's, MONO's: its first) */
            const track_t *t = &trk[k];
            const voice_t *v = &t->v[j];
            uint32_t age, seed = (v->age * 2654435761u ^ (uint32_t)k * 0x9E3779B9u) | 1u, u, i;
            int32_t g, fade = 100 - scd.lv[k] / 8;  /* (the track's level: a hit fades as it rings out; the voice's */
            uint16_t col;                           /* envelope, 0 dB .. -54 dB: a note as it is released) */
            if (!v->active || v->stage == 4u || (j && trk_vmode(t) != V_POLY))
                continue;
            if (scd.id[k][j] != (uint16_t)v->age) {  /* a new note on it: flying out from now */
                scd.id[k][j] = (uint16_t)v->age;
                scd.t0[k][j] = (uint16_t)now;
            }
            age = (uint16_t)(now - scd.t0[k][j]);
            if (age > 250u)                          /* (out: kept there, a long note's never wraps round) */
                scd.t0[k][j] = (uint16_t)(now - 250u);
            newest = age < newest ? age : newest;
            g = 100 - meter_px(v->env >> 9) * 15 / 4;
            if ((fade = g > fade ? g : fade) >= 95)
                continue;
            u = age >= 250u ? 0u : 1000u - age * 4u;       /* (flying out over 250 ms, slowing) */
            g = ui_prefs & PREF_ANIM_OFF ? 1000 : 1000 - (int32_t)(u * u / 1000u);
            col = ux_mix(T_TRK(k), T_SURF, fade < 0 ? 0 : fade);
            for (i = 0; i < 3u; i++) {               /* three particles, each its own way */
                uint32_t dir, e;
                int32_t len, rc, curl, n;
                seed = seed * 1103515245u + 12345u;
                dir = seed * 2654435761u;
                len = R * (34 + (int32_t)(seed >> 16) % 46) / 100;
                rc = R * (15 + (int32_t)(seed >> 24) % 136) / 100;
                curl = seed >> 3 & 1u ? 1 : -1;
                n = len * g / 1000;
                qk_track(cx + 1, cy, dir, rc, len, curl, ux_mix(col, T_SURF, 60), T_SURF, n);   /* (its glow) */
                e = qk_track(cx, cy, dir, rc, len, curl, col, T_SURF, n);
                if (n)
                    cv_rect((int16_t)(e >> 16), (int16_t)e - 1, 2, 2, col);   /* (the particle) */
            }
        }
qk_vertex:
    qk_disc(cx, cy, newest < 150u ? 4 : 3, ux_mix(T_ACCENT, T_SURF, 60));
    qk_disc(cx, cy, newest < 150u ? 3 : 2, T_ACCENT);
}

/* (2.5) CRYPT: the scope as six creatures in a row, a track's each, always the same (ui_crypt.c cr_creature), on the
 * floor, its number under it */
static void cr_creatures(void)
{
    static const uint8_t *const ART[NTRK] = {SPR_CR_SKULL, SPR_CR_GHOST, SPR_CR_CANDLE, SPR_CR_WITCH, SPR_CR_MONK,
                                             SPR_CR_DEMON};
    uint32_t k, count = NTRK, first = 0u;
#if FELUCCA_CAFA
    if(cafa_native_profile_view){count=cafa_nv_track_lanes();first=cafa_native_u14(cafa_native_profile_view+26u);}
#endif
    for (k = 0; k < count; k++) {
        int32_t x = 9 + (int32_t)k * 37, muted;
        uint32_t track = first + k;
#if FELUCCA_CAFA
        if(cafa_native_profile_view){const uint8_t *r=cafa_nv_row(k);muted=r && (r[40]&4u);}
        else
#endif
        muted=trk[k].p[P_MUTE]!=0;
        char d[6];
        fmt_int(d, (int32_t)track + 1);
        cr_creature(x, (int32_t)cv_h - 22 - ART[k][1] * 6, ART[k], scd.lv[k], T_TRK(track), muted,
                    (ui.frame / 2u + track) * 2654435761u);
        GFX_HOOK_ALIGN(x, (int32_t)cv_h - 19, x + 35, (int32_t)cv_h - 7, AL_HV, "scope creature's number under it");
        cv_text_in(x, (int32_t)cv_h - 19 + CAP_IN(S, 12), 35, &AF_S, d, muted ? T_DIM : T_TRK(track), T_SURF);
    }
}

/* (2.5) SKIFF: the scope as the logo (design/logo/make_logo.py): a sea of steps, each column lit to the swell, the
 * tracks' colours in bands (track 1 at the left), the swell as high as the tracks there are loud, rolling on (a muted
 * or silent track's band no colour, calm); the skiff riding the crest, across and round again */
#define SEA_N 18                                      /* columns (three a track), every SEA_P px, a cell SEA_P - 2 */
#define SEA_P 12
#define SEA_Z 3                                       /* the boat's pixel */
static void scope_sea(void)
{
    static const uint16_t BOAT[4] = {0x0001, 0x7FFE, 0x3FFC, 0x1FF8};   /* (the logo's: 15 x 4, the bow at the right) */
    int32_t x0 = 120 - SEA_N * SEA_P / 2 + 1, foot = (int32_t)cv_h - 8, rows = 8, i, r, hb = 1;
    int32_t bx = x0 - 15 * SEA_Z + (int32_t)(fm1_ms / 24u % (uint32_t)(SEA_N * SEA_P + 15 * SEA_Z));   /* (its stern) */
    uint32_t ph = fm1_ms * 2863311u;                 /* (the swell: 8 columns a wave, one wave on every 1.5 s) */
    uint32_t first = 0u;
#if FELUCCA_CAFA
    if(cafa_native_profile_view)first=cafa_native_u14(cafa_native_profile_view+26u);
#endif
    uint16_t grey = ux_mix(T_SURF, T_TEXT, 30), off = ux_mix(T_SURF, T_TEXT, 8);
    for (i = 0; i < SEA_N; i++) {
        uint32_t k = (uint32_t)i / 3u, n = k + (uint32_t)i % 3u - 1u;   /* (the band beside, on the column's side) */
        int32_t lv = scd.lv[k], x = x0 + i * SEA_P, h;
        uint16_t on = lv > 30 ? T_TRK(first + k) : grey;
        lv = (2 * lv + scd.lv[n < NTRK ? n : k]) / 3;   /* (between the bands' middles: a third of the way and two) */
        h = 1 + (lv * rows * (32767 + QK_SIN(ph - (uint32_t)i * 0x20000000u)) >> 26);   /* (~ / 1000 / 65534) */
        for (r = 0; r < rows; r++) {                 /* (a cell its corners cut, as the logo's rounded ones) */
            int32_t y = foot - (r + 1) * SEA_P + 2;
            uint16_t c = r < h ? on : off;
            cv_rect(x + 1, y, SEA_P - 4, SEA_P - 2, c);
            cv_rect(x, y + 1, SEA_P - 2, SEA_P - 4, c);
        }
        if (x + SEA_P - 2 > bx && x < bx + 15 * SEA_Z && h > hb)   /* (the boat on the highest crest under it) */
            hb = h;
    }
    for (r = 0; r < 4; r++)
        for (i = 0; i < 15; i++)
            if (BOAT[r] >> (14 - i) & 1u && bx + i * SEA_Z >= x0 && bx + i * SEA_Z < x0 + SEA_N * SEA_P - 2)
                cv_rect(bx + i * SEA_Z, foot - hb * SEA_P - 4 * SEA_Z + 1 + r * SEA_Z, SEA_Z, SEA_Z, T_TEXT);
}

/* oscilloscope of the output, triggered on a rising zero crossing: a RAISE centre line, the trace 2 px (in silence
 * a flat line on it; MENU > SCREEN OFF keeps it from staying on the panel for hours) */
#include "ui_native_curves.c"

#include "ui_pgraph.c"                               /* (Skiff U5, V1) the pages' own graphs */

static void draw_graph(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint16_t c = ux.quark ? T_THEME : T_TRK(song.sel);   /* Skiff: BLOCKS draws in the track's colour ((Skiff Q5) QUARK:
                                                      * cyan, an instrument's readouts, as the mockup) */
    uint32_t sig;
    int32_t pad = graph_pad();                       /* (Skiff) BLOCKS: the panel 16 px taller (ui.c foot_less): ADSR and
                                                      * LFO grow into it, the fixed layouts sit centred, pad lower */
    if (!ui.home && pg->graph == GR_TRK && !fr.on) {
        draw_tracks();
        return;
    }
    if (!ui.home && (pg->scope == SC_ENGINE || pg->graph == GR_SMIX) && ENGINES[t->eng_req % NENGINES] == &ENG_SAMPLE)
        sample_wave_tick(t);
    sig = fr.on ? steps_hash(t) + (uint32_t)t->p[P_SLEN] * 7u + (song.playing ? t->seq_idx + 1u : 0u) * 977u + 0xF7EEu
                : graph_signature() + (ui.take_t ? take_box.gen * 2654435761u + 0x7A6Bu : 0u);   /* (Skiff: the TAKE box) */
    if (!ui.force && sig == ui.graph_sig)
        return;
    ui.graph_sig = sig;
    cv_begin(240, graph_h(), T_BG);
    if (ux.crypt) {                                  /* (Skiff C3) CRYPT: stone in a double frame */
        cr_stone(3, 0, 234, (int32_t)graph_h(), T_SURF);
        cr_frame2(3, 0, 234, (int32_t)graph_h(), T_LINE);
    } else
        cv_rrect(3, 0, 234, graph_h(), 5, T_SURF, T_BG);   /* the panel (MENU > LARGE: the strip) */
    if (ux.quark)                                    /* ((Skiff Q2) QUARK: an instrument's bracket corners) */
        qk_corners(3, 0, 234, (int32_t)graph_h(), T_DIM, 7);
    cv_bg = T_SURF;                                  /* (text drawn with cv_text lands on it) */
    cv_oy = GOY + pad;                               /* graphs on a 100 px scale */
    if (fr.on && strip_kind() == SK_NONE) {
        graph_steps(t, c);                           /* ((2.5) a FREE TAKE's picker: the track's steps, as PATTERN's) */
    } else if (strip_kind() != SK_NONE) {            /* MENU > LARGE: the strip */
        uint32_t k = fr.on ? SK_STEPS : strip_kind();
        cv_oy = k == SK_ADSR || k == SK_LFO ? 3 : k == SK_SCOPE ? 0 : pad;
        graph_ht = (int32_t)graph_h() - 6;
        if (k == SK_SCOPE && !ui.home_scope) graph_names();   /* ((2.5, #97, 8C) HOME: the six sounds) */
        else if (k == SK_SCOPE) graph_scope(c);
        else if (k == SK_ADSR) graph_adsr(t, c);
        else if (k == SK_LFO) graph_lfo(t, c);
        else if (k == SK_STEPS) graph_steps(t, c);
        else graph_title();
        graph_ht = 100;
    } else if (ui.home) {
        cv_oy = 0;
        if (ui.home_scope)
            graph_scope(c);
        else
            graph_tracks();                          /* Skiff: the four tracks (ui_home.c) */
    } else {
        switch (pg->graph) {
        case GR_ADSR:
            cv_oy = GOY;
            graph_ht = 100 + 2 * pad;
            graph_adsr(t, c);
            graph_ht = 100;
            break;
        case GR_LFO:
            cv_oy = GOY;
            graph_ht = 100 + 2 * pad;
            graph_lfo(t, c);
            graph_ht = 100;
            break;
        case GR_STEPS:
            graph_steps(t, c);
            break;
        case GR_FEEL:                                /* (Skiff 2.0) FEEL: the tiles, each step's value (a drum track: */
            cv_oy = drum_track(t) ? pad : 0;         /* the grid (#10); the tiles centre themselves) */
            if (drum_track(t))
                graph_grid(t, c);
            else
                graph_tiles(t, c);
            break;
        case GR_ROLL:                                /* (Skiff 2.0) NOTES: the roll (a drum track: the grid) */
            cv_oy = pad;
            if (drum_track(t))
                graph_grid(t, c);
            else
                graph_roll(t, c);
            break;
        case GR_SCALE:
            graph_scale(t, c);
            break;
        case GR_CHORD:
            cv_oy = pad;
            graph_chord(t, c);
            break;
        case GR_FOLLOW:                              /* ((2.1) as CHORD's) */
            cv_oy = pad;
            graph_follow(t);
            break;
        case GR_FX:
            graph_fx(t, c);
            break;
        case GR_SLCR:
            graph_slicer(t, c);
            break;
        case GR_DUCK:                                /* (Skiff) */
            graph_duck(t, c);
            break;
        case GR_MOD:                                 /* ((2.0, D 1A) from the panel's top, as the mockup) */
            cv_oy = 0;
            graph_mod(t, c);
            break;
        case GR_BROWSE:
            cv_oy = pad;
            graph_browse();
            break;
        case GR_SLOTS:
            cv_oy = pad;
            graph_slots();
            break;
        case GR_USER:
            cv_oy = pad;
            graph_user();
            break;
        case GR_BANK:                                /* (Skiff phase 5) the pool view, from the panel's top (the mockup's) */
            cv_oy = 0;
            graph_bank();
            break;
        case GR_DXSND:                               /* (Skiff phase 7, 8; FM6 LAB) a sheet, from the panel's top */
        case GR_FMOP:
        case GR_FMALL:
            cv_oy = 0;
            sheet_of(pg->graph)->draw(t, c);
            break;
        case GR_SONG:                                /* (Skiff phase 5) the rows from the panel's top (the mockup's) */
            cv_oy = 0;
            graph_song();
            break;
        case GR_EVENTS:
            cv_oy = pad;
            graph_events();
            break;
        case GR_LANES:                               /* (Skiff U3, R6) */
            graph_lanes(t, c);
            break;
        case GR_LMIX:                                /* ((2.0, #36) LANE MIX) */
            graph_lmix(t, c);
            break;
        case GR_DRIVE:                               /* (Skiff phase 9; U5, V1: ui_pgraph.c, from the panel's top) */
            cv_oy = 0;
            graph_drive(t, c);
            break;
        case GR_INS:                                 /* (Skiff 1.3) */
            cv_oy = 0;
            graph_insert(t, c);
            break;
        case GR_DELAY:
        case GR_FXDLY:                               /* ((2.5) FX > DELAY: GLO's, the track's send in it) */
            cv_oy = 0;
            graph_delay(c);
            break;
        case GR_REVERB:
        case GR_FXREV:
            cv_oy = 0;
            graph_reverb(c);
            break;
        case GR_CHORUS:
            cv_oy = 0;
            graph_chorus(c);
            break;
        case GR_GLOBAL:
            cv_oy = 0;
            graph_global(c);
            break;
        case GR_VOICE:
            cv_oy = 0;
            graph_voice(t, c);
            break;
        case GR_VOICE2:
            cv_oy = 0;
            graph_voice2(t, c);
            break;
        case GR_ARP:
        case GR_ARP2:
            cv_oy = 0;
            graph_arp(t, c);
            break;
#if FELUCCA_SLICE
        case GR_SLICES:
            cv_oy = pad;
            graph_slices();
            break;
#endif
        default:
            if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_WHEEL) graph_wheel(t, c);
            else if ((pg->scope == SC_ENGINE || pg->graph == GR_SMIX) && ENGINES[t->eng_req % NENGINES] == &ENG_SAMPLE &&
                     sample_wave.ready) graph_sample(c);   /* ((#21) SAMPLES: the sample's) */
            else if (pg->scope == SC_ENGINE && t->eng_req % NENGINES == ENGI_FM6) graph_fm6(t, c);   /* EDIT 1 and 2 */
            else if (analog_page(pg, t)) {           /* (Skiff U5) ANALOG's OSC and FLT */
                cv_oy = 0;
                if (pg->id[0] == P_E0) graph_osc(t, c);
                else graph_flt(t, c);
            }
#if FELUCCA_FM4
            else if ((pg->scope == SC_ENGINE || pg->id[0] == P_FM1_LEVEL) && t->eng_req % NENGINES == ENGI_DIGITAL)
                graph_fm(t, c);                      /* (OP LEVEL too: the levels on the chart) */
#endif
            else { cv_oy = 0; graph_scope(c); }   /* (it centres itself in the canvas) */
            break;
        }
    }
    cv_oy = 0;
    if (ui.take_t && (int32_t)graph_h() >= TB_Y + TB_H + 1)   /* (Skiff: the TAKE box, ui_take.c) */
        take_box_draw();
    cv_blit(0, graph_y());
}

/* Published companion sections feed the same canvas, palettes and Skiff panel
 * geometry as the standalone graph painters above.  No local sequencer data is
 * consulted while cafa_native_profile_view is set. */
#if FELUCCA_CAFA
static uint32_t cafa_nv_count(uint8_t tag,const uint8_t **body,uint32_t *bytes)
{
    const uint8_t *p=cafa_nv_section(tag,bytes);
    if(!p) { *body=0;return 0; }
    *body=p+(tag==1u || tag==4u || tag==7u ? 1u : 2u);
    return tag==1u || tag==4u || tag==7u ? p[0] : cafa_native_u14(p);
}
static void cafa_nv_curve(uint16_t col,uint32_t screen)
{
    const uint8_t *p,*m;uint32_t n,b,k,mb,mn;
    int32_t x0=12,x1=228,top=7,bot=graph_h()-10,px=0,py=0;
    n=cafa_nv_count(3,&p,&b);
    if(screen==0u || screen==1u || (screen>=24u && screen<=27u)) {
        cv_rect(x0,top,x1-x0,1,T_LINE);
        cv_rect(x0,(top+bot)/2,x1-x0,1,T_LINE);
        cv_rect(x0,bot,x1-x0,1,T_LINE);
    } else if(screen==2u || screen==3u) cv_rect(x0,(top+bot)/2,x1-x0,1,T_RAISE);
    else {cv_rect(x0,bot,x1-x0,1,T_LINE);cv_rect(x0,top,1,bot-top,T_LINE);}
    for(k=0;k<n;k++) {
        int32_t x=x0+(int32_t)cafa_native_u14(p+4u*k)*(x1-x0)/16383;
        int32_t y=bot-(int32_t)cafa_native_u14(p+4u*k+2u)*(bot-top)/16383;
        if(k)cv_line_t(px,py,x,y,col,2);
        else if(ux.quark)qk_disc(x,y,2,col);
        px=x;py=y;
    }
    mn=cafa_nv_count(4,&m,&mb);
    for(k=0;k<mn;k++) {
        int32_t x=x0+(int32_t)cafa_native_u14(m+13u*k)*(x1-x0)/16383;
        int32_t y=bot-(int32_t)cafa_native_u14(m+13u*k+2u)*(bot-top)/16383;
        uint16_t c=(m[13u*k+4u]&1u)?T_ACCENT:col;
        if(ux.quark)qk_nucleus(x,y,c,7);
        else if(ux.neon)ne_spark(x,y,c,1);
        else cv_rect(x-2,y-2,5,5,c);
    }
    if(!n)cv_text_in(8,graph_h()/2-5,224,&AF_S,"NO CURVE",T_DIM,T_SURF);
}

static void cafa_nv_notes(uint16_t col)
{
    const uint8_t *p;uint32_t n,b,k,first=cafa_native_u14(cafa_native_profile_view+17);
    uint32_t cursor=cafa_native_u14(cafa_native_profile_view+20),play=cafa_native_u14(cafa_native_profile_view+22);
    const uint8_t *keyboard;uint32_t kb_len,lo=48u,hi=48u;
    const uint8_t *roll=cafa_nv_row(0);
    char format[17];
    if(roll){cafa_native_text(format,roll,16);if(!str_eq(format,"ROLL VIEW V1"))roll=0;}
    uint32_t cols=roll?cafa_native_profile_view[19]:16u;
    uint32_t start=roll?cafa_native_u14(roll+41):cursor;
    uint32_t end=roll?cafa_native_u14(roll+43):cursor;
    n=cafa_nv_count(2,&p,&b);
    if(n) {
        lo=hi=p[0];
        for(k=1;k<n;k++){if(p[7u*k]<lo)lo=p[7u*k];if(p[7u*k]>hi)hi=p[7u*k];}
        if(hi-lo<PR_ROWS){uint32_t pad=(PR_ROWS-(hi-lo))/2u;lo=lo>pad?lo-pad:0u;}
        if(lo+PR_ROWS>128u)lo=128u-PR_ROWS;
    }
    keyboard=cafa_nv_section(6,&kb_len);
    for(k=0;k<PR_ROWS;k++) {
        uint32_t pitch=lo+PR_ROWS-1u-k,pc=pitch%12u;
        int32_t y=PR_Y0+(int32_t)k*PR_RH;char label[8];
        int in=keyboard && (keyboard[pitch/7u]&(1u<<(pitch%7u)));
        if(in)cv_rect(PR_X0,y,(int32_t)cols*PR_CW,PR_RH,T_LANE);
        cv_rect(PR_KX,y,KEY_BLACK[pc]?5:9,PR_RH-1,KEY_BLACK[pc]?T_DIM:T_RAISE);
        if(pc==0u){
            int32_t ink[4];
            cv_rect(PR_X0,y+PR_RH-1,(int32_t)cols*PR_CW,1,T_RAISE);
            note_name(label,pitch);text_ink(&AF_S,label,ink);
            cv_text_on(23-ink[2],y-5,&AF_S,label,T_DIM,T_SURF);
        }
    }
    for(k=0;k<=cols;k++)cv_rect(PR_X0+(int32_t)k*PR_CW,PR_Y0,1,PR_ROWS*PR_RH,k%4u?T_GRID:T_RAISE);
    if(cols && start<=end && start<first+cols && end>=first) {
        uint32_t left=start<first?0u:start-first,right=end>=first+cols?cols-1u:end-first;
        int32_t x=PR_X0+(int32_t)left*PR_CW,width=(int32_t)(right-left+1u)*PR_CW+1;
        if(ux.neon)ne_laser(x,PR_Y0-2,width,PR_ROWS*PR_RH+4,T_SURF);
        else if(ux.quark)qk_corners(x,PR_Y0-2,width,PR_ROWS*PR_RH+4,T_ACCENT,4);
        else cv_frame(x,PR_Y0-2,width,PR_ROWS*PR_RH+4,ux.crypt?CR_GOLD:T_TEXT);
    }
    if(play>=first && play<first+cols)cv_rect(PR_X0+(int32_t)(play-first)*PR_CW+6,PR_Y0,1,PR_ROWS*PR_RH,
                                             ux.crypt?CR_GOLD:T_ACCENT);
    if(roll){
        char scale[17];cafa_native_text(scale,roll+16,16);
        cv_text_r(PR_X0+16*PR_CW,PR_Y0+PR_ROWS*PR_RH+2,&AF_S,scale,T_DIM,T_SURF);
        for(k=0;k<cols;k++){
            const uint8_t *cell=cafa_nv_cell(k);
            if(cell && (cell[2]&8u))cv_rect(PR_X0+3+(int32_t)k*PR_CW,PR_Y0+PR_ROWS*PR_RH+3,7,2,T_ACCENT);
        }
    }
    for(k=0;k<n;k++) {
        uint32_t start=cafa_native_u14(p+7u*k+1u),length=cafa_native_u14(p+7u*k+3u);
        uint32_t flags=p[7u*k+5u];
        int32_t x=PR_X0+(int32_t)start*PR_CW/128,y=PR_Y0+((int32_t)(lo+PR_ROWS-1u)-(int32_t)p[7u*k])*PR_RH;
        int32_t w=(int32_t)length*PR_CW/128;if(w<2)w=2;
        if(x<PR_X0)x=PR_X0;if(x+w>PR_X0+(int32_t)cols*PR_CW)w=PR_X0+(int32_t)cols*PR_CW-x;
        if(w>0) {
            uint16_t ink=(flags&16u)?T_DIM:(flags&1u)?T_ACCENT:(flags&32u)?T_TEXT:col;
            pr_bar(x,y,w,ink,(flags&32u)!=0u,1u);
            if(flags&2u)cv_rect(x,y,1,4,T_TEXT);
            if(flags&4u)cv_rect(x+w-1,y,1,4,T_TEXT);
            if(flags&8u)cv_rect(x,y+PR_RH,7,2,T_ACCENT);
        }
    }
}
static void cafa_native_profile_graph(const uint8_t *view,uint32_t len)
{
    uint32_t screen=view[6];uint16_t col=T_THEME;
    (void)len;
    if(screen==31u){draw_tracks();return;}
    cv_begin(240,graph_h(),T_BG);
    if(ux.crypt) {cr_stone(3,0,234,(int32_t)graph_h(),T_SURF);cr_frame2(3,0,234,(int32_t)graph_h(),T_LINE);}
    else cv_rrect(3,0,234,graph_h(),5,T_SURF,T_BG);
    if(ux.quark)qk_corners(3,0,234,(int32_t)graph_h(),T_DIM,7);
    cv_bg=T_SURF;cv_oy=0;
    switch(screen) {
    case 0:case 1:case 24:case 25:case 26:case 27:
        cv_oy=GOY;graph_ht=100+2*graph_pad();graph_adsr(0,col);graph_ht=100;break;
    case 2:case 3:
        cv_oy=GOY;graph_ht=100+2*graph_pad();graph_lfo(0,col);graph_ht=100;break;
    case 11:cv_oy=GOY+graph_pad();graph_duck(0,col);break;
    case 48:graph_scope(col);break;
    case 5:graph_fx(0,col);break;
    case 6:case 32:graph_delay(col);break;
    case 7:case 33:graph_reverb(col);break;
    case 8:graph_drive(0,col);break;
    case 9:graph_insert(0,col);break;
    case 28:
#if FELUCCA_FM4
        graph_fm(0,col);
#else
        cafa_nv_fm_network(col);
#endif
        break;
    case 29:graph_voice(0,col);break;
    case 30:graph_voice2(0,col);break;
    case 34:graph_chorus(col);break;
    case 35:graph_global(col);break;
    case 40:case 41:graph_arp(0,col);break;
    case 10:graph_slicer(0,col);break;
    case 43:
        if(view[7]==CAFA_NATIVE_VARIANT_DRUM && cafa_nv_cell(16))graph_grid(0,col);
        else graph_tiles(0,col);
        break;
    case 44:case 67:graph_steps(0,col);break;
    case 12:graph_scale(0,col);break;
    case 13:graph_chord(0,col);break;
    case 14:graph_follow(0);break;
    case 19:graph_lanes(0,col);break;
    case 21:graph_lmix(0,col);break;
    case 22:
#if FELUCCA_SLICE
        graph_slices();
#else
        cafa_nv_slices();
#endif
        break;
    case 42:if(view[7]==CAFA_NATIVE_VARIANT_DRUM)graph_grid(0,col);else graph_roll(0,col);break;
    case 47:if(large_kind()==LK_TALL)graph_names();else graph_tracks();break;
    case 4:graph_mod(0,col);break;
    case 36:graph_browse();break;
    case 37:graph_user();break;
    case 38:graph_bank();break;
    case 39:graph_slots();break;
    case 45:graph_events();break;
    case 46:graph_song();break;
    case 15:case 16:
        if(view[7]==CAFA_NATIVE_VARIANT_NORMAL && cafa_nv_row(8) &&
           cafa_nv_row(0) && memcmp(cafa_nv_row(0)+32u,"DRAWBARS",8u)==0)graph_wheel(0,col);
        else if(view[7]==CAFA_NATIVE_VARIANT_SAMPLE_WAVE)graph_sample(col);
        else if(view[7]==CAFA_NATIVE_VARIANT_FM_OPERATOR)graph_fm6(0,col);
        else if(view[7]==CAFA_NATIVE_VARIANT_ANALOG) {
            if(screen==15u)graph_osc(0,col);else graph_flt(0,col);
        } else if(view[7]==CAFA_NATIVE_VARIANT_DRUM_SOUND)graph_dx(0,col);
        else if(view[7]==CAFA_NATIVE_VARIANT_DRUM_LANES)graph_lanes(0,col);
        else graph_scope(col);
        break;
    case 17:graph_fm_op(0,col);break;
    case 18:graph_fm_all(0,col);break;
    case 20:graph_dx(0,col);break;
    case 23:graph_sample(col);break;
    default:break;
    }
    cv_oy=0;cafa_native_transient_draw();cv_blit(0,graph_y());
}
#endif
