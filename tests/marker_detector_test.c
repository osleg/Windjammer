/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include "../firmware/src/windjammer_marker_detector.h"

static int feed_pattern(windjammer_marker_detector_t *d, uint32_t first,
                        int length, int amplitude, int *hit, uint32_t *found)
{
    int i;
    for (i = 0; i < 4; i++) {
        int frame;
        for (frame = 0; frame < length; frame++)
            *hit += windjammer_marker_feed(d, first++, (int16_t)((i & 1) ? -amplitude : amplitude), found);
    }
    return (int)first;
}

int main(void)
{
    windjammer_marker_detector_t d;
    uint32_t found = 0, index;
    int i, hits = 0;
    /* Captured from a windowed-sinc 44.1 -> 48 -> 44.1 kHz conversion of
     * the reserved right-channel pulse at 0.316 gain. The first sustained
     * positive run begins at frame 65, after short alternating pre-ringing. */
    static const int16_t filtered[] = {
        0, -1, 1, -2, 3, -5, 5, -2, -6, 22, -50,
        91, -144, 207, -269, 320, -348, 339,
        7482, 7946, 7733, 7621, 8097, 7268, 8385, 7093,
        -7121, -8300, -7414, -7885, -7895, -7410, -8296, -7142,
        7142, 8296, 7409, 7895, 7885, 7415, 8300, 7121,
        -7093, -8385, -7268, -8097, -7621, -7733, -7947, -7482
    };
    windjammer_marker_reset(&d);
    for (i = 0; i < 8; i++) assert(!windjammer_marker_feed(&d, (uint32_t)i, 0, &found));
    feed_pattern(&d, 8, 8, 24576, &hits, &found);
    assert(hits == 1 && found == 8);

    /* Phone volume may attenuate the right channel and its sample-rate
     * conversion may shift each sign transition by a frame. */
    windjammer_marker_reset(&d); hits = 0;
    for (i = 0; i < 4; i++) windjammer_marker_feed(&d, (uint32_t)i, 0, &found);
    index = 4;
    for (i = 0; i < 4; i++) {
        int j, run = (i & 1) ? 9 : 7;
        for (j = 0; j < run; j++)
            hits += windjammer_marker_feed(&d, index++, (int16_t)((i & 1) ? -120 : 120), &found);
    }
    assert(hits == 1 && found == 4);

    /* A volume mixer may round one or two interpolated transition samples
     * below the threshold. Only a completed run followed by reversal admits
     * that gap. */
    windjammer_marker_reset(&d); hits = 0; index = 0;
    for (i = 0; i < 4; i++) windjammer_marker_feed(&d, index++, 0, &found);
    for (i = 0; i < 4; i++) {
        int j;
        for (j = 0; j < 8; j++)
            hits += windjammer_marker_feed(&d, index++, (int16_t)((i & 1) ? -120 : 120), &found);
        if (i < 3) for (j = 0; j < 2; j++)
            hits += windjammer_marker_feed(&d, index++, 0, &found);
    }
    assert(hits == 1 && found == 4);

    windjammer_marker_reset(&d); hits = 0; index = 0;
    for (i = 0; i < 4; i++) windjammer_marker_feed(&d, index++, 0, &found);
    for (i = 0; i < 8; i++) windjammer_marker_feed(&d, index++, 120, &found);
    for (i = 0; i < 3; i++) windjammer_marker_feed(&d, index++, 0, &found);
    for (i = 1; i < 4; i++) {
        int j;
        for (j = 0; j < 8; j++)
            hits += windjammer_marker_feed(&d, index++, (int16_t)((i & 1) ? -120 : 120), &found);
    }
    assert(hits == 0);

    /* A short filter blip followed by four quiet frames arms a new cue. */
    windjammer_marker_reset(&d); hits = 0; index = 0;
    for (i = 0; i < 4; i++) windjammer_marker_feed(&d, index++, 0, &found);
    windjammer_marker_feed(&d, index++, 120, &found);
    windjammer_marker_feed(&d, index++, -120, &found);
    for (i = 0; i < 4; i++) windjammer_marker_feed(&d, index++, 0, &found);
    feed_pattern(&d, index, 8, 120, &hits, &found);
    assert(hits == 1 && found == index);

    windjammer_marker_reset(&d); hits = 0; found = 0;
    for (i = 0; i < 47; i++) windjammer_marker_feed(&d, (uint32_t)i, 0, &found);
    for (i = 0; i < (int)(sizeof filtered / sizeof filtered[0]); i++)
        hits += windjammer_marker_feed(&d, (uint32_t)(47 + i), filtered[i], &found);
    assert(hits == 1 && found >= 64 && found <= 65);

    /* The same waveform without leading quiet must not arm the detector. */
    windjammer_marker_reset(&d); hits = 0;
    for (i = 11; i < (int)(sizeof filtered / sizeof filtered[0]); i++)
        hits += windjammer_marker_feed(&d, (uint32_t)i, filtered[i], &found);
    assert(hits == 0);

    /* A discontinuity in the leading guard cannot borrow quiet frames. */
    windjammer_marker_reset(&d); hits = 0; found = 0;
    for (i = 0; i < 3; i++) windjammer_marker_feed(&d, (uint32_t)i, 0, &found);
    windjammer_marker_feed(&d, 4, 0, &found);
    feed_pattern(&d, 5, 8, 120, &hits, &found);
    assert(hits == 0);

    /* A too-fast pulse still fails the qualified-run length. */
    windjammer_marker_reset(&d); hits = 0;
    for (i = 0; i < 4; i++) windjammer_marker_feed(&d, (uint32_t)i, 0, &found);
    feed_pattern(&d, 4, 5, 120, &hits, &found);
    assert(hits == 0);

    /* Ordinary sustained tones have lobes much longer than this cue. */
    windjammer_marker_reset(&d); hits = 0; index = 0;
    for (i = 0; i < 4; i++) windjammer_marker_feed(&d, index++, 0, &found);
    for (i = 0; i < 128; i++)
        hits += windjammer_marker_feed(&d, index++, (int16_t)(((i / 16) & 1) ? -120 : 120), &found);
    assert(hits == 0);

    /* A long foreign oscillation cannot serve as an unlimited preamble. */
    windjammer_marker_reset(&d); hits = 0; index = 0;
    for (i = 0; i < 4; i++) windjammer_marker_feed(&d, index++, 0, &found);
    for (i = 0; i < 16; i++)
        hits += windjammer_marker_feed(&d, index++, (int16_t)((i & 1) ? -120 : 120), &found);
    feed_pattern(&d, index, 8, 120, &hits, &found);
    assert(hits == 0);

    /* A complete-looking pulse split by a missing source frame is invalid. */
    windjammer_marker_reset(&d); hits = 0; index = 0;
    for (i = 0; i < 4; i++) windjammer_marker_feed(&d, index++, 0, &found);
    for (i = 0; i < 8; i++) windjammer_marker_feed(&d, index++, 120, &found);
    index++;
    for (i = 1; i < 4; i++) {
        int j;
        for (j = 0; j < 8; j++)
            hits += windjammer_marker_feed(&d, index++, (int16_t)((i & 1) ? -120 : 120), &found);
    }
    assert(hits == 0);

    /* A loud single-channel edge or a broken packet cannot be called a
     * physical note onset. */
    windjammer_marker_reset(&d); hits = 0;
    for (i = 0; i < 4; i++) windjammer_marker_feed(&d, (uint32_t)i, 0, &found);
    for (i = 4; i < 20; i++) hits += windjammer_marker_feed(&d, (uint32_t)i, 3000, &found);
    assert(hits == 0);
    for (i = 20; i < 24; i++) windjammer_marker_feed(&d, (uint32_t)i, 0, &found);
    hits += windjammer_marker_feed(&d, 24, 3000, &found);
    hits += windjammer_marker_feed(&d, 26, 3000, &found);
    assert(hits == 0);
    puts("Marker detector: attenuated and resampled source, false-event rejection passed");
    return 0;
}
