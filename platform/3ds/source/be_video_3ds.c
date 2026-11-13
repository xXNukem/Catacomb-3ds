/* Copyright (C) 2014-2026 NY00123
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Native New Nintendo 3DS indexed-video backend for ReflectionHLE.
 *
 * The memory-to-screen addressing follows the EGA/VGA path in
 * ReflectionHLE's src/backend/video/be_video.c, by NY00123
 * (Copyright (C) 2014-2026; original BSD-3-Clause notice retained
 * in the upstream sources). This file is an adaptation, not a replacement
 * for the emulated video-writing operations in be_video_emu.c.
 */

#include <3ds.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "be_st.h"
#include "backend/video/be_video.h"
#include "backend/video/be_video_emu.h"

bool g_sdlDoRefreshGfxOutput = true;

/* On the 3DS there is no SDL window, output rectangle or texture to create.
   The original BE_ST_SetScreenMode still calls these hooks. */
void BEL_ST_SetGfxOutputRects(bool allowResize)
{
    (void)allowResize;
}

void BEL_ST_RecreateMainTextures(void)
{
    g_sdlDoRefreshGfxOutput = true;
}

void BE_ST_MarkGfxForUpdate(void)
{
    g_sdlDoRefreshGfxOutput = true;
}

void BEL_ST_ForceHostDisplayUpdate(void)
{
    g_sdlDoRefreshGfxOutput = true;
}

void BE_ST_ExitWithErrorMsg(const char *msg)
{
    fprintf(stderr, "Catacomb 3DS: %s\n", msg ? msg : "unknown error");
    exit(EXIT_FAILURE);
}

/* Reconstruct the visible indexed frame from emulated EGA/VGA video RAM.
   Like the original converter, screen-start units are 8 pixels for EGA
   and 4 pixels for VGA mode 13h. Addresses wrap within the emulated segment;
   the split-screen reset takes effect AFTER its designated scanline. */
static void BEL_3DS_RefreshIndexedFrame(void)
{
    if (!g_sdlDoRefreshGfxOutput)
        return;

    if (g_sdlScreenMode != 0xD && g_sdlScreenMode != 0xE &&
        g_sdlScreenMode != 0x13)
        return;

    const int width = g_sdlTexWidth;
    if (width != GFX_TEX_WIDTH && width != 2 * GFX_TEX_WIDTH)
        return;

    const uint32_t wrap = (g_sdlScreenMode == 0x13) ? 0x40000u : 0x80000u;
    uint32_t srcOffset =
        ((uint32_t)(g_sdlScreenMode == 0x13 ? 4 : 8) *
         g_sdlScreenStartAddress + g_sdlPelPanning) % wrap;
    const uint8_t *src = (const uint8_t *)g_sdlVidMem.A000.egaGfx;

    for (int y = 0; y < GFX_TEX_HEIGHT; ++y)
    {
        uint8_t *dst = g_sdlHostScrMem.egaGfx + y * width;
        uint32_t pixelsBeforeWrap = wrap - srcOffset;

        if ((uint32_t)width <= pixelsBeforeWrap)
            memcpy(dst, src + srcOffset, width);
        else
        {
            memcpy(dst, src + srcOffset, pixelsBeforeWrap);
            memcpy(dst + pixelsBeforeWrap, src, width - pixelsBeforeWrap);
        }

        if (g_sdlSplitScreenLine == y)
            srcOffset = g_sdlPelPanning;
        else
            srcOffset = (srcOffset + (uint32_t)g_sdlPixLineWidth) % wrap;
    }

    memcpy(g_sdlHostScrMemCache.egaGfx, g_sdlHostScrMem.egaGfx,
           width * GFX_TEX_HEIGHT);
    g_sdlDoRefreshGfxOutput = false;
}

/* Presentation entry point used by the ReflectionHLE video subsystem.
   We always draw both framebuffers after conversion: the 3DS swaps its
   display buffers even when the emulated graphics are unchanged. */
void BEL_ST_UpdateHostDisplay(void)
{
    BEL_3DS_RefreshIndexedFrame();

    if (g_sdlScreenMode == 0xD || g_sdlScreenMode == 0xE ||
        g_sdlScreenMode == 0x13)
    {
        const int width = g_sdlTexWidth;
        const uint8_t *pixels = g_sdlHostScrMem.egaGfx;
        const uint32_t *palette = g_sdlEGACurrBGRAPalette;
        uint8_t *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

        for (int x = 0; x < 400; ++x)
        {
            const int sx = x * width / 400;
            for (int y = 0; y < 240; ++y)
            {
                const int sy = y * GFX_TEX_HEIGHT / 240;
                const uint32_t color = palette[pixels[sy * width + sx]];
                const int offset = 3 * (x * 240 + 239 - y);
                fb[offset] = (uint8_t)color;
                fb[offset + 1] = (uint8_t)(color >> 8);
                fb[offset + 2] = (uint8_t)(color >> 16);
            }
        }
    }

    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}
