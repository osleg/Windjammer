/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FM1_MODE_POOL_H
#define FM1_MODE_POOL_H

#include <stdint.h>

#ifndef MODE_POOL_START
extern uint32_t _pool_end[], _mode_pool_limit[];
#define MODE_POOL_START _pool_end
#define MODE_POOL_LIMIT _mode_pool_limit
#endif

/* Exclusive borrow of the unused pool tail. Main loop only. The owner prepares
 * its data before reading it and stops any outstanding DMA before releasing it. */
static const void *mode_pool_owner;

static void *mode_pool_claim(const void *owner, uint32_t bytes)
{
    uintptr_t start = (uintptr_t)MODE_POOL_START;
    uintptr_t limit = (uintptr_t)MODE_POOL_LIMIT;
    if (mode_pool_owner || limit < start || bytes > limit - start) return 0;
    mode_pool_owner = owner;
    return (void *)start;
}

static void mode_pool_release(const void *owner)
{
    if (mode_pool_owner == owner) mode_pool_owner = 0;
}

#endif
