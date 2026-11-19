/* SPDX-License-Identifier: GPL-3.0-only
 * The diagnostic phone reserves the right USB channel for four eight-frame
 * runs: positive, negative, positive, negative. The USB host may resample it,
 * so each run may gain or lose up to two source frames. */
#ifndef WINDJAMMER_MARKER_DETECTOR_H
#define WINDJAMMER_MARKER_DETECTOR_H

#include <stdint.h>

typedef struct {
    uint32_t start, previous_index;
    uint8_t stage, run, gap, quiet, have_previous, preamble, pre_run;
    int8_t pre_sign;
} windjammer_marker_detector_t;

static void windjammer_marker_reset(windjammer_marker_detector_t *d)
{
    *d = (windjammer_marker_detector_t){0};
}

static void windjammer_marker_disarm(windjammer_marker_detector_t *d)
{
    d->stage = d->run = d->gap = d->quiet = 0;
    d->preamble = d->pre_run = 0;
    d->pre_sign = 0;
}

/* Returns the first source frame's index only after the complete guarded
 * pattern is recognized. Call with the right PCM sample from every accepted
 * USB packet in source-index order. */
static int windjammer_marker_feed(windjammer_marker_detector_t *d, uint32_t index,
                             int16_t sample, uint32_t *start_index)
{
    int sign = sample >= 64 ? 1 : sample <= -64 ? -1 : 0;
    int expected;
    if (d->have_previous && index != d->previous_index + 1u)
        windjammer_marker_disarm(d);
    d->have_previous = 1;
    d->previous_index = index;
    if (!d->stage) {
        if (d->quiet < 4u) {
            if (!sign) d->quiet++;
            else d->quiet = 0;
            return 0;
        }
        if (!sign && !d->preamble) return 0;
        /* A band-limited conversion can ring for a few alternating frames
         * before the first full positive lobe. Only short preliminary runs
         * are admitted, within twelve frames ahead of that lobe. */
        if (sign != d->pre_sign) {
            if (d->pre_run > (d->pre_sign ? 2u : 3u)) goto reject;
            d->pre_sign = (int8_t)sign;
            d->pre_run = 0;
            if (sign > 0) d->start = index;
        }
        d->pre_run++;
        if (!sign && d->pre_run == 4u) {
            windjammer_marker_disarm(d);
            d->quiet = 4u;
            return 0;
        }
        if (++d->preamble > 18u || (sign < 0 && d->pre_run > 2u))
            goto reject;
        if (sign > 0 && d->pre_run >= 6u) {
            d->stage = 1;
            d->run = d->pre_run;
            d->preamble = d->pre_run = 0;
            d->pre_sign = 0;
        }
        return 0;
    }
    expected = (d->stage & 1u) ? 1 : -1;
    /* A mixer can interpolate two low samples at a polarity change. The
     * preceding run must qualify and the next strong sample must reverse. */
    if (!sign && d->stage < 4u && d->run >= 6u && d->gap < 2u) {
        d->gap++;
        return 0;
    }
    if (sign == expected && !d->gap) {
        if (++d->run > 10u) goto reject;
        if (d->stage == 4u && d->run >= 6u) {
            *start_index = d->start;
            windjammer_marker_disarm(d);
            return 1;
        }
        return 0;
    }
    if (sign == -expected && d->stage < 4u && d->run >= 6u) {
        d->stage++;
        d->run = 1;
        d->gap = 0;
        return 0;
    }
reject:
    if (!sign) {
        uint8_t quiet = d->gap ? (uint8_t)(d->gap + 1u) : 1u;
        windjammer_marker_disarm(d);
        d->quiet = quiet;
        return 0;
    }
    windjammer_marker_disarm(d);
    return 0;
}

#endif
