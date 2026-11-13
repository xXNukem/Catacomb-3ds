/* Catacomb 3DS: native ARM-friendly wall scaler from ReflectionHLE scale tables.
 * Copyright (C) 2026 xXNukem. SPDX-License-Identifier: GPL-2.0-or-later.
 *
 * Only the 3DS wall drawing path uses this. It leaves the original DOS
 * algorithm, wall layout, perspective, clip, palettes and game state intact.
 */
#ifndef CAT3DS_NATIVE_WALL_SCALER_H
#define CAT3DS_NATIVE_WALL_SCALER_H

#include <stdint.h>
#include <string.h>

typedef struct Cat3DS_WallRowMap {
    uint8_t source[144]; /* 0..63 = texture y; 255 = outside wall */
    uint8_t first;
    uint8_t end; /* exclusive */
} Cat3DS_WallRowMap;

/* ReflectionHLE's BuildCompScale generates an x86-style instruction stream
 * using the same start/width arrays. Compile that geometry to a tiny row
 * table ONCE, instead of interpreting the instructions on every frame. */
static inline void Cat3DS_BuildWallRows(Cat3DS_WallRowMap *out,
                                      const uint16_t *start,
                                      const uint16_t *width,
                                      int textureHeight, int screenHeight)
{
    const int top = (screenHeight - textureHeight) / 2;
    int first = screenHeight, last = 0;
    memset(out->source, 255, sizeof out->source);
    for (int src = 0; src < 64; ++src) {
        int y0 = top + start[src];
        int y1 = y0 + width[src];
        if (y0 < 0) y0 = 0;
        if (y1 > screenHeight) y1 = screenHeight;
        if (y0 >= y1) continue;
        if (y0 < first) first = y0;
        if (y1 > last) last = y1;
        for (int y = y0; y < y1; ++y) out->source[y] = (uint8_t)src;
    }
    out->first = (uint8_t)(first < last ? first : 0);
    out->end = (uint8_t)last;
}

/* EGA emulation already stores one 64-bit word for eight planar pixels.
 * Draw the exact same bitmasked writes without calling the interpreted
 * 'mov / and' instruction interpreter once per output row and column. */
static inline void Cat3DS_DrawWallRows(const Cat3DS_WallRowMap *rows,
                                      uint64_t *ega,
                                      uint32_t destOffset, unsigned stride,
                                      const uint8_t *tex, uint8_t loMask, uint8_t hiMask,
                                      const uint64_t *repeatColor,
                                      const uint64_t *expandMask)
{
    const uint64_t lo = expandMask[loMask];
    const uint64_t hi = expandMask[hiMask];
    const uint64_t invLo = ~lo, invHi = ~hi;
    uint32_t offset = destOffset + (unsigned)rows->first * stride;
    for (int y = rows->first; y < rows->end; ++y, offset += stride) {
        uint8_t src = rows->source[y];
        if (src == 255) continue;
        uint64_t color = repeatColor[tex[src] & 15];
        if (loMask) ega[offset] = (ega[offset] & invLo) | (color & lo);
        if (hiMask) ega[offset+1] = (ega[offset+1] & invHi) | (color & hi);
    }
}

#endif
