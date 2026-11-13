// EGA diagnostic preserved from the previously hardware-tested bridge.
#include <3ds.h>
#include <stdint.h>
#include <stdio.h>

extern "C" {
#include "../../../src/backend/video/be_video.h"
#include "../../../src/backend/video/be_video_emu.h"

/* These are the original ReflectionHLE entry points, implemented in C. */
void BE_ST_SetScreenMode(int mode);
void BE_ST_SetScreenStartAddress(uint16_t crtc);
void BE_ST_EGASetPaletteAndBorder(const uint8_t *palette);
void BE_ST_EGASetPelPanning(uint8_t panning);
void BE_ST_EGASetSplitScreen(int16_t linenum);
void BE_ST_EGAUpdateGFXBufferFrom4bitsPixel(uint16_t destOff,
                                            uint8_t color, uint16_t count);
}

/* Draw two EGA video pages using ReflectionHLE's original planar writes.
   The 3DS backend is responsible for all conversion and presentation. */
static void generateTestPages(void)
{
    const int width = g_sdlTexWidth;
    const int byteStride = width / 8;
    const int bytesPerPage = byteStride * GFX_TEX_HEIGHT;

    for (int page = 0; page < 2; ++page)
    {
        for (int y = 0; y < GFX_TEX_HEIGHT; ++y)
        {
            for (int x = 0; x < width; x += 8)
            {
                const uint16_t offset = (uint16_t)(
                    page * bytesPerPage + y * byteStride + x / 8);
                const uint8_t color = (uint8_t)(
                    ((x / 20) + (y / 20) + page * 5) & 15);
                BE_ST_EGAUpdateGFXBufferFrom4bitsPixel(offset, color, 1);
            }
        }
    }
}

static void setPaletteShift(int shift)
{
    uint8_t signals[17];
    for (int i = 0; i < 16; ++i)
    {
        const int color = (i + shift) & 15;
        signals[i] = (uint8_t)((color & 7) | ((color & 8) ? 16 : 0));
    }
    signals[16] = 0; // Black overscan border
    BE_ST_EGASetPaletteAndBorder(signals);
}

void cat3dsRunVideoTest(void)
{
    consoleClear();

    int paletteShift = 0;
    int page = 0;
    int panning = 0;
    bool split = false;

    BE_ST_SetScreenMode(0xD);
    generateTestPages();
    setPaletteShift(paletteShift);

    printf("Catacomb 3DS - EGA bridge\n\n");
    printf("A: 320/640 resolution\n");
    printf("B: Palette shift\n");
    printf("X: Video page flip\n");
    printf("Y: Split screen\n");
    printf("Left/Right: Pixel panning\n");
    printf("START: Exit\n");

    while (aptMainLoop())
    {
        hidScanInput();
        const u32 keys = hidKeysDown();

        if (keys & KEY_START)
            break;

        if (keys & KEY_A)
        {
            const int mode = (g_sdlScreenMode == 0xD) ? 0xE : 0xD;
            BE_ST_SetScreenMode(mode);
            paletteShift &= 15;
            page = 0;
            panning = 0;
            split = false;
            generateTestPages();
            setPaletteShift(paletteShift);
        }

        if (keys & KEY_B)
        {
            paletteShift = (paletteShift + 1) & 15;
            setPaletteShift(paletteShift);
        }

        if (keys & KEY_X)
        {
            page ^= 1;
            BE_ST_SetScreenStartAddress(
                (uint16_t)(page * (g_sdlTexWidth / 8) * GFX_TEX_HEIGHT));
        }

        if (keys & KEY_Y)
        {
            split = !split;
            BE_ST_EGASetSplitScreen(split ? 198 : -2); // Line 99 or disabled
        }

        if (keys & KEY_DLEFT)
            panning = (panning + 7) & 7;
        if (keys & KEY_DRIGHT)
            panning = (panning + 1) & 7;
        if (keys & (KEY_DLEFT | KEY_DRIGHT))
            BE_ST_EGASetPelPanning((uint8_t)panning);

        printf("\x1b[9;1H%dx200  Page %d  Split %s  Pan %d     ",
               g_sdlTexWidth, page, split ? "ON " : "OFF", panning);

        BEL_ST_UpdateHostDisplay();
    }

    consoleClear();
}
