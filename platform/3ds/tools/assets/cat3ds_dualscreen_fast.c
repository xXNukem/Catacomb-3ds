/* Catacomb 3DS - native second-screen HUD and explored automap.
 * Copyright (C) 2026 xXNukem. SPDX-License-Identifier: GPL-2.0-or-later
 * Retains original Catacomb 3-D status pixels; no reimplemented HUD logic.
 */
#include "cat3ds_dualscreen.h"
#include <3ds.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL_timer.h>

#define BOT_W 320
#define BOT_H 240
#define MAP_W 264  /* Exact original 3D-view rectangle, excluding right HUD */
#define MAP_H 144
#define MAP_MAX 64
#define MAP_CELL 6

extern const uint8_t g_cga_8x8TextFont[256 * 8 * 8];
static uint8_t bottomIndexed[BOT_W * BOT_H];
static uint8_t explored[MAP_MAX * MAP_MAX];
static uint8_t walls[MAP_MAX * MAP_MAX];
static int currentLevel = -1, levelWidth, levelHeight;
static int playerTileX, playerTileY, playerFineX, playerFineY, playerAngle;
static int gameplay, haveFrame, bottomConfigured, topConfigured;
static uint16_t topScaleX[400], topScaleY[240];
static int topScaleReady;

/* FPS is counted on actual gameplay presentation, not simulation ticks.
 * An approximately one-second window keeps the number readable. */
static uint64_t fpsWindowStart;
static unsigned fpsFrames;
static unsigned fpsTimes10;
static int fpsHasSample;

/* On New 3DS the automap rasterizer can run on core 1 while core 0
 * prepares the original HUD pixels for the bottom framebuffer. It never
 * calls SDL, gfx, or game logic from the worker. */
static int perfInitialized, isNew3DS, workerEnabled, workerStopping;
static LightEvent workerStart, workerDone;
static Thread workerThread;

static void updateFps(void)
{
    const uint64_t now = SDL_GetTicks();
    if (!fpsWindowStart) fpsWindowStart = now;
    ++fpsFrames;
    const uint64_t elapsed = now - fpsWindowStart;
    if (elapsed >= 1000) {
        fpsTimes10 = (unsigned)((uint64_t)fpsFrames * 10000 / elapsed);
        fpsHasSample = 1;
        fpsWindowStart = now;
        fpsFrames = 0;
    }
}


static void put(int x, int y, uint8_t color)
{
    if ((unsigned)x < BOT_W && (unsigned)y < BOT_H)
        bottomIndexed[y * BOT_W + x] = color;
}
static void hline(int x, int y, int w, uint8_t col)
{
    if ((unsigned)y >= BOT_H || w <= 0)
        return;
    if (x < 0) { w += x; x = 0; }
    if (x >= BOT_W) return;
    if (x + w > BOT_W) w = BOT_W - x;
    if (w > 0) memset(bottomIndexed + y * BOT_W + x, col, (size_t)w);
}
static void text8(int x, int y, const char *s, uint8_t col)
{
    for (; *s && x <= BOT_W - 8; ++s, x += 8) {
        const uint8_t *glyph = g_cga_8x8TextFont + (uint8_t)*s * 64;
        for (int yy = 0; yy < 8; ++yy)
            for (int xx = 0; xx < 8; ++xx)
                if (glyph[yy * 8 + xx]) put(x + xx, y + yy, col);
    }
}
static void line(int x0, int y0, int x1, int y1, uint8_t col)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        if ((unsigned)x0 < MAP_W && (unsigned)y0 < MAP_H) put(x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e = 2 * err;
        if (e > -dy) { err -= dy; x0 += sx; }
        if (e < dx) { err += dx; y0 += sy; }
    }
}
static void drawMap(void)
{
    /* Fixed north-up view, following the player. Each tile is 6x6 pixels. */
    for (int y = 0; y < MAP_H; ++y)
        hline(0, y, MAP_W, 0);
    if (!haveFrame) return;

    const int baseX = playerTileX - MAP_W / (MAP_CELL * 2);
    const int baseY = playerTileY - MAP_H / (MAP_CELL * 2);
    for (int sy = 0; sy < MAP_H / MAP_CELL + 1; ++sy) {
        int my = baseY + sy;
        if ((unsigned)my >= (unsigned)levelHeight) continue;
        for (int sx = 0; sx < MAP_W / MAP_CELL + 1; ++sx) {
            int mx = baseX + sx;
            if ((unsigned)mx >= (unsigned)levelWidth || !explored[mx * MAP_MAX + my])
                continue;
            int px = sx * MAP_CELL, py = sy * MAP_CELL;
            uint8_t color = walls[mx * MAP_MAX + my] ? 9 : 8;
            for (int yy = py + 1; yy < py + MAP_CELL - 1 && yy < MAP_H; ++yy)
                hline(px + 1, yy, MAP_CELL - 2, color);
        }
    }
    /* Draw the position at sub-tile precision. Angles use Catacomb's
     * 360-degree convention: +X east, positive angles turn north. */
    const int cx = (playerTileX - baseX) * MAP_CELL + playerFineX * MAP_CELL / 65536;
    const int cy = (playerTileY - baseY) * MAP_CELL + playerFineY * MAP_CELL / 65536;
    for (int y = cy - 2; y <= cy + 2; ++y)
        hline(cx - 2, y, 5, 14);
    /* 8-way directional arrow, using integer arithmetic (no per-frame trig). */
    static const int dx[8] = {9, 6, 0,-6,-9,-6, 0, 6};
    static const int dy[8] = {0,-6,-9,-6, 0, 6, 9, 6};
    int dir = ((playerAngle % 360) + 360) % 360;
    dir = ((dir + 22) / 45) % 8;
    line(cx, cy, cx + dx[dir], cy + dy[dir], 15);
}

static void mapWorkerMain(void *unused)
{
    (void)unused;
    for (;;) {
        LightEvent_Wait(&workerStart);
        if (workerStopping) break;
        drawMap();
        LightEvent_Signal(&workerDone);
    }
}

static void stopMapWorker(void)
{
    if (!workerEnabled) return;
    workerStopping = 1;
    LightEvent_Signal(&workerStart);
    threadJoin(workerThread, U64_MAX);
    threadFree(workerThread);
    workerThread = NULL;
    workerEnabled = 0;
    /* Releasing the reserved system-core budget when the game exits. */
    APT_SetAppCpuTimeLimit(0);
}

static void initPerformance(void)
{
    if (perfInitialized) return;
    perfInitialized = 1;
    bool detected = false;
    if (R_FAILED(APT_CheckNew3DS(&detected)) || !detected) return;
    isNew3DS = 1;
    /* The clock boost is also requested from the main entry, before menu. */
    osSetSpeedupEnable(true);
    /* Core 1 (system core) permits a small app worker CPU quota. This is
     * NOT a claim that the game engine itself runs on two cores. */
    if (R_FAILED(APT_SetAppCpuTimeLimit(20))) return;
    LightEvent_Init(&workerStart, RESET_ONESHOT);
    LightEvent_Init(&workerDone, RESET_ONESHOT);
    workerStopping = 0;
    workerThread = threadCreate(mapWorkerMain, NULL, 16384, 0x30, 1, false);
    if (!workerThread) {
        APT_SetAppCpuTimeLimit(0);
        return;
    }
    workerEnabled = 1;
    atexit(stopMapWorker);
}

static void convertRange(uint8_t *fb, int fbStride,
                         const uint32_t *palette, int xBegin, int xEnd,
                         int yBegin, int yEnd)
{
    for (int x = xBegin; x < xEnd; ++x) {
        uint8_t *dst = fb + 3 * x * fbStride;
        for (int y = yBegin; y < yEnd; ++y) {
            uint32_t rgb = palette[bottomIndexed[y * BOT_W + x]];
            uint8_t *pix = dst + 3 * (fbStride - y - 1);
            pix[0] = (uint8_t)rgb;
            pix[1] = (uint8_t)(rgb >> 8);
            pix[2] = (uint8_t)(rgb >> 16);
        }
    }
}

void Cat3DS_DualScreenSetGameplay(int enabled)
{
    /* SDL3 still renders menus and game-over/selection screens. Restore the
     * video format it initialized before returning control to its renderer. */
    if (!enabled && topConfigured) {
        gfxSetScreenFormat(GFX_TOP, GSP_RGBA8_OES);
        topConfigured = 0;
    }
    gameplay = !!enabled;
    if (!gameplay) haveFrame = 0;
}
void Cat3DS_DualScreenBeginLevel(int level, int mapWidth, int mapHeight)
{
    currentLevel = level;
    levelWidth = mapWidth > 0 && mapWidth <= MAP_MAX ? mapWidth : 0;
    levelHeight = mapHeight > 0 && mapHeight <= MAP_MAX ? mapHeight : 0;
    memset(explored, 0, sizeof explored);
    haveFrame = 0;
}
void Cat3DS_DualScreenObserve(int level, int mapWidth, int mapHeight,
                               const uint8_t *tilemapXMajor,
                               const uint8_t *visibleXMajor,
                               int32_t playerX, int32_t playerY, int angle)
{
    if (!gameplay || !tilemapXMajor || !visibleXMajor ||
        mapWidth <= 0 || mapWidth > MAP_MAX ||
        mapHeight <= 0 || mapHeight > MAP_MAX)
        return;
    if (level != currentLevel || mapWidth != levelWidth || mapHeight != levelHeight)
        Cat3DS_DualScreenBeginLevel(level, mapWidth, mapHeight);
    /* The originals lay out tilemap and spotvis as [x][y], not [y][x]. */
    memcpy(walls, tilemapXMajor, sizeof walls);
    for (int x = 0; x < levelWidth; ++x)
        for (int y = 0; y < levelHeight; ++y)
            if (visibleXMajor[x * MAP_MAX + y]) explored[x * MAP_MAX + y] = 1;
    /* The original visibility tracer marks only the wall cells hit by its
     * rays. Reveal the immediate solid neighbors of visible OPEN tiles,
     * so adjacent wall segments do not appear randomly missing/blue.
     * Never reveal beyond one wall or past the explored open area. */
    for (int x = 0; x < levelWidth; ++x)
        for (int y = 0; y < levelHeight; ++y) {
            const int pos = x * MAP_MAX + y;
            if (!visibleXMajor[pos] || walls[pos]) continue;
            if (x > 0 && walls[(x - 1) * MAP_MAX + y]) explored[(x - 1) * MAP_MAX + y] = 1;
            if (x + 1 < levelWidth && walls[(x + 1) * MAP_MAX + y]) explored[(x + 1) * MAP_MAX + y] = 1;
            if (y > 0 && walls[x * MAP_MAX + y - 1]) explored[x * MAP_MAX + y - 1] = 1;
            if (y + 1 < levelHeight && walls[x * MAP_MAX + y + 1]) explored[x * MAP_MAX + y + 1] = 1;
        }
    playerTileX = playerX >> 16;
    playerTileY = playerY >> 16;
    if ((unsigned)playerTileX >= (unsigned)levelWidth ||
        (unsigned)playerTileY >= (unsigned)levelHeight)
        return;
    explored[playerTileX * MAP_MAX + playerTileY] = 1;
    playerFineX = playerX & 65535;
    playerFineY = playerY & 65535;
    playerAngle = angle;
    haveFrame = 1;
}
int Cat3DS_DualScreenHasGameplayFrame(void) { return gameplay && haveFrame; }

/* CAT3DS_NATIVE_EGA_TOP_FAST_V1
 * The original 264x144 view is already present in the EGA framebuffer.
 * Read it directly instead of uploading a 320x200 ARGB texture and running
 * SDL's software renderer at 400x240 on every gameplay frame.
 *
 * This changes presentation only, NOT Catacomb's ray tracing, FOV, or
 * game logic. No game data or gameplay state is modified. */
static void presentNativeTop(const uint8_t *egaPixels, int pitch,
                             const uint32_t *palette)
{
    if (!topConfigured) {
        gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
        topConfigured = 1;
    }
    if (!topScaleReady) {
        for (int x = 0; x < 400; ++x)
            topScaleX[x] = (uint16_t)(x * 264 / 400);
        for (int y = 0; y < 240; ++y)
            topScaleY[y] = (uint16_t)(y * 144 / 240);
        topScaleReady = 1;
    }

    u16 width, height;
    uint8_t *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width, &height);
    /* libctru uses column-major physical layout: 240 rows x 400 columns. */
    if (!fb || width < 240 || height < 400)
        return;
    for (int x = 0; x < 400; ++x) {
        uint8_t *col = fb + (size_t)x * width * 3;
        const int sampleX = topScaleX[x];
        for (int y = 0; y < 240; ++y) {
            const uint32_t color = palette[egaPixels[(size_t)topScaleY[y] * pitch + sampleX]];
            uint8_t *pixel = col + (size_t)(width - y - 1) * 3;
            pixel[0] = (uint8_t)color;
            pixel[1] = (uint8_t)(color >> 8);
            pixel[2] = (uint8_t)(color >> 16);
        }
    }
    GSPGPU_FlushDataCache(fb, (u32)width * height * 3);
    gfxScreenSwapBuffers(GFX_TOP, false);
}

void Cat3DS_DualScreenPresent(const uint8_t *egaPixels, int pitch,
                              const uint32_t *palette)
{
    if (!gameplay || !haveFrame || !egaPixels || pitch < BOT_W || !palette)
        return;
    initPerformance();
    updateFps();
    presentNativeTop(egaPixels, pitch, palette);

    /* SDL3 initializes both screens as RGBA8. Only the bottom screen's
     * pixel format is changed; SDL3 still owns the top renderer. */
    if (!bottomConfigured) {
        gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
        bottomConfigured = 1;
    }
    for (int y = 0; y < 200; ++y)
        memcpy(bottomIndexed + y * BOT_W, egaPixels + y * pitch, BOT_W);
    memset(bottomIndexed + 200 * BOT_W, 0, 40 * BOT_W);

    /* The worker may only write the map area: x=0..263, y=0..143.
     * The main thread remains responsible for all HUD/footer pixels,
     * framebuffer conversion and swapping. */
    if (workerEnabled) LightEvent_Signal(&workerStart);
    else drawMap();

    hline(0, 200, BOT_W, 1);
    hline(0, 221, BOT_W, 1);
    text8(8, 206, "CATACOMB 3DS | AUTOMAP", 11);
    char fpsText[24];
    if (fpsHasSample)
        snprintf(fpsText, sizeof fpsText, "FPS %u.%u",
                 fpsTimes10 / 10, fpsTimes10 % 10);
    else
        snprintf(fpsText, sizeof fpsText, "FPS --");
    text8(240, 206, fpsText, 14);
    char levelText[] = "LEVEL 00   * PLAYER # WALL";
    const int levelNum = currentLevel >= 0 ? (currentLevel + 1) % 100 : 0;
    levelText[6] = (char)('0' + levelNum / 10);
    levelText[7] = (char)('0' + levelNum % 10);
    text8(8, 228, levelText, 7);
    /* CORE1 is shown only when a real auxiliary thread was created. */
    text8(232, 228, workerEnabled ? "CORE1" :
          (isNew3DS ? "BOOST" : "CPU0"), 10);

    u16 fw, fh;
    uint8_t *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);
    const int valid = fb && fw >= BOT_H && fh >= BOT_W;
    if (workerEnabled) {
        /* Convert the original HUD while the other CPU draws the map.
         * Conversion touches NO pixels that the worker is updating. */
        if (valid) {
            convertRange(fb, fw, palette, 0, MAP_W, MAP_H, BOT_H);
            convertRange(fb, fw, palette, MAP_W, BOT_W, 0, BOT_H);
        }
        LightEvent_Wait(&workerDone);
        if (valid) convertRange(fb, fw, palette, 0, MAP_W, 0, MAP_H);
    } else if (valid) {
        convertRange(fb, fw, palette, 0, BOT_W, 0, BOT_H);
    }
    if (!valid) return;
    GSPGPU_FlushDataCache(fb, (u32)fw * (u32)fh * 3);
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
}
