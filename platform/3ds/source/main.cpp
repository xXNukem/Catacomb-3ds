/* Catacomb 3DS - native launcher / original Catacomb 3-D title preview.
 * Game file requirements are generated from ReflectionHLE definitions.
 * The actual game entry points are not linked yet.
 */
#include <3ds.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cat3ds_game_files.h"
#include "cat3ds_title_preview.h"
#include "cat3ds_resource_probe.h"
#include "cat3ds_level_map.h"
#include "cat3ds_walk_preview.h"
#include "cat3ds_wall_textures.h"
#include "cat3ds_original_trace.h"

extern "C" {
#include "../../../src/backend/video/be_video.h"
#include "../../../src/backend/video/be_video_emu.h"
/* ROM CGA 8x8 font, public-domain collection included in ReflectionHLE. */
extern const uint8_t g_cga_8x8TextFont[256 * 8 * 8];
void BE_ST_SetScreenMode(int mode);
void BE_ST_SetScreenStartAddress(uint16_t crtc);
void BE_ST_EGASetPelPanning(uint8_t panning);
void BE_ST_EGASetSplitScreen(int16_t linenum);
void BE_ST_EGASetPaletteAndBorder(const uint8_t *signals);
void BE_ST_EGAUpdateGFXBufferFrom4bitsPixel(uint16_t off, uint8_t color, uint16_t count);
void BE_ST_EGAUpdateGFXBitsFrom4bitsPixel(uint16_t off, uint8_t color, uint8_t mask);
}

void cat3dsRunVideoTest(void);

static const uint8_t kEgaSignals[17] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    16, 17, 18, 19, 20, 21, 22, 23, 0
};

/* x and width are multiples of 8, like the original EGA memory layout. */
static void fillRect(int x, int y, int width, int height, uint8_t color)
{
    for (int row = y; row < y + height; ++row)
        BE_ST_EGAUpdateGFXBufferFrom4bitsPixel(
            (uint16_t)(row * 40 + x / 8), color, (uint16_t)(width / 8));
}

/* Draw actual glyphs using EGA bitplane writes, not SDL, C2D or a pixel copy.
 * The preexisting ROM font is a 256 x 8 x 8 array of 0/1 pixels. */
static void drawText(int x, int y, const char *s, uint8_t color)
{
    for (; *s && x <= 312; ++s, x += 8)
    {
        const unsigned char ch = (unsigned char)*s;
        for (int row = 0; row < 8; ++row)
        {
            const uint8_t *glyph = g_cga_8x8TextFont + ch * 64 + row * 8;
            uint8_t mask = 0;
            for (int col = 0; col < 8; ++col)
                if (glyph[col])
                    mask |= (uint8_t)(0x80 >> col);
            if (mask)
                BE_ST_EGAUpdateGFXBitsFrom4bitsPixel(
                    (uint16_t)((y + row) * 40 + x / 8), color, mask);
        }
    }
}

/* Tile-sized EGA rectangles, including sub-byte widths. The EGA backend
 * already supports masked updates into its original emulated video RAM. */
static void drawPixelRect(int x, int y, int width, int height, uint8_t color)
{
    if (x < 0 || y < 0 || x + width > 320 || y + height > 200) return;
    for (int py = y; py < y + height; ++py)
        for (int b = x / 8; b <= (x + width - 1) / 8; ++b)
        {
            uint8_t mask = 0;
            for (int bit = 0; bit < 8; ++bit)
            {
                const int px = 8 * b + bit;
                if (px >= x && px < x + width) mask |= uint8_t(0x80 >> bit);
            }
            BE_ST_EGAUpdateGFXBitsFrom4bitsPixel(uint16_t(py * 40 + b), color, mask);
        }
}

static void recenterOnPlayer(const Cat3DSLevelMap &map, unsigned &x, unsigned &y)
{
    x = map.width / 2; y = map.height / 2;
    for (unsigned yy = 0; yy < map.height; ++yy)
        for (unsigned xx = 0; xx < map.width; ++xx)
        {
            const unsigned code = map.objects[yy * map.width + xx];
            if (code >= 1 && code <= 4) { x = xx; y = yy; return; }
        }
}

static void drawOriginalMap(const Cat3DSLevelMap &map, unsigned cursorX, unsigned cursorY)
{
    BE_ST_SetScreenMode(0xD);
    BE_ST_EGASetPaletteAndBorder(kEgaSignals);
    BE_ST_SetScreenStartAddress(0);
    BE_ST_EGASetPelPanning(0);
    BE_ST_EGASetSplitScreen(-2);
    fillRect(0, 0, 320, 200, 0);
    fillRect(8, 8, 304, 184, 1);
    fillRect(16, 16, 288, 168, 0);
    drawText(40, 21, "CATACOMB 3-D   /   MAP DATA", 15);

    const int tileSize = ((304 / int(map.width)) < (144 / int(map.height))) ?
                           (304 / int(map.width)) : (144 / int(map.height));
    if (tileSize < 1) return;
    const int left = (320 - int(map.width) * tileSize) / 2;
    const int top = 34 + (144 - int(map.height) * tileSize) / 2;
    for (unsigned y = 0; y < map.height; ++y)
        for (unsigned x = 0; x < map.width; ++x)
        {
            const size_t i = y * map.width + x;
            const unsigned wall = map.walls[i];
            const unsigned object = map.objects[i];
            const uint8_t color = object >= 1 && object <= 4 ? 14 :
                                  object ? 11 :
                                  wall > 0 && wall < 36 ? 5 :
                                  wall >= 36 ? 1 : 0;
            drawPixelRect(left + int(x) * tileSize, top + int(y) * tileSize,
                          tileSize, tileSize, color);
        }
    /* The white outline is a read-only tile inspector, not the player. */
    const int cx = left + int(cursorX) * tileSize;
    const int cy = top + int(cursorY) * tileSize;
    drawPixelRect(cx, cy, tileSize, 1, 15);
    drawPixelRect(cx, cy + tileSize - 1, tileSize, 1, 15);
    drawPixelRect(cx, cy, 1, tileSize, 15);
    drawPixelRect(cx + tileSize - 1, cy, 1, tileSize, 15);
    drawText(32, 182, "ORIGINAL DOS LEVEL - DATA TEST", 10);
}

static void drawOriginalMapInfo(const Cat3DSLevelMap &map, unsigned x, unsigned y)
{
    consoleClear();
    printf("CATACOMB 3DS - MAP LOADER\n");
    printf("-------------------------------\n\n");
    printf("LEVEL %02u / 20\n", map.index + 1);
    printf("Name: %.16s\n", map.name);
    printf("Dimensions: %u x %u tiles\n", map.width, map.height);
    const unsigned offset = y * map.width + x;
    printf("\nINSPECTOR: X=%u Y=%u\n", x, y);
    printf("Wall/floor code: %u\n", unsigned(map.walls[offset]));
    printf("Object/spawn code: %u\n", unsigned(map.objects[offset]));
    printf("\nYellow: player starting tile\n");
    printf("Cyan: objects / actors\n");
    printf("Purple: solid wall codes\n");
    printf("Blue: floor codes\n");
    printf("White: inspector cursor\n");
    printf("\nD-PAD: Move inspector\n");
    printf("L / R: Previous / next level\n");
    printf("A: Jump to player spawn\n");
    printf("X: First-person walk test\n");
    printf("B: Back   START: Exit\n");
    printf("\nData test only - not gameplay.\n");
}

/* The 3DS Circle Pad reports raw values (-156..156). Small offsets around
 * its resting point must not turn the camera every frame. */
static float circleAxisWithDeadzone(int raw)
{
    constexpr int deadzone = 35;
    constexpr int maxAxis = 156;
    if (raw > maxAxis) raw = maxAxis;
    if (raw < -maxAxis) raw = -maxAxis;
    if (raw > deadzone) return float(raw - deadzone) / float(maxAxis - deadzone);
    if (raw < -deadzone) return float(raw + deadzone) / float(maxAxis - deadzone);
    return 0.f;
}

static void drawWalkInfo(const Cat3DSLevelMap &map, const Cat3DSWalkPreview &walk,
                         bool analogEnabled, int circleX, int circleY,
                         bool texturesReady, bool highQuality)
{
    consoleClear();
    printf("CATACOMB 3DS - FIRST-PERSON TEST\n");
    printf("-------------------------------\n\n");
    printf("ORIGINAL DOS LEVEL %02u / 20\n", map.index + 1);
    printf("Name: %.16s\n", map.name);
    printf("Map: %u x %u\n", map.width, map.height);
    printf("Position: %.2f / %.2f\n", walk.x, walk.y);
    printf("Tile: %u / %u\n", unsigned(walk.x), unsigned(walk.y));
    printf("Heading: %.0f degrees\n", walk.angle * (180.f / 3.1415926536f));
    printf("\nD-PAD UP/DOWN: Walk\n");
    printf("D-PAD LEFT/RIGHT: Turn\n");
    printf("CIRCLE PAD: Walk and turn\n");
    printf("Y: Circle Pad %s (toggle)\n", analogEnabled ? "ON" : "OFF");
    printf("SELECT: View quality %s\n", highQuality ? "320 columns" : "160 columns");
    printf("Raw Pad X/Y: %d / %d\n", circleX, circleY);
    printf("L / R: Strafe left / right\n");
    printf("A: Restart at spawn\n");
    printf("X or B: Return to top-down map\n");
    printf("START: Exit\n");
    printf("\nWall art: %s\n", texturesReady ? "ORIGINAL EGA" : "FLAT PREVIEW");
    printf("Ceiling: BLACK   Floor: DARK GREY\n");
    Cat3DSOriginalTrace originalTrace = {};
    if (cat3dsTraceOriginalViewEdges(map, walk, &originalTrace))
        printf("Orig ray: L(%d,%d) R(%d,%d)\n",
               originalTrace.leftX, originalTrace.leftY,
               originalTrace.rightX, originalTrace.rightY);
    else
        printf("Orig ray: no valid wall edges\n");
    printf("Game engine: integration in progress\n");
}

static void drawLauncher(int selected, const Cat3DSInstallStatus states[CAT3DS_GAME_COUNT])
{
    BE_ST_SetScreenMode(0xD);
    BE_ST_EGASetPaletteAndBorder(kEgaSignals);
    BE_ST_SetScreenStartAddress(0);
    BE_ST_EGASetPelPanning(0);
    BE_ST_EGASetSplitScreen(-2);
    fillRect(0, 0, 320, 200, 0);

    fillRect(8, 8, 304, 184, 1);
    fillRect(16, 16, 288, 168, 0);
    fillRect(16, 16, 288, 30, 1);
    drawText(48, 21, "C A T A C O M B   3 D S", 15);
    drawText(56, 34, "SELECT YOUR ADVENTURE", 14);

    for (int i = 0; i < CAT3DS_GAME_COUNT; ++i)
    {
        const int y = 52 + 31 * i;
        const bool active = i == selected;
        fillRect(24, y, 272, 27, active ? 3 : 8);
        fillRect(32, y + 2, 256, 23, active ? 1 : 0);
        drawText(40, y + 5, cat3dsGameName(i), active ? 15 : 7);
        const Cat3DSInstallStatus *s = &states[i];
        const bool baseVerified = s->coreFileCount == 4 && s->coreValidCount == 4;
        const bool fullVerified = s->fileCount && s->validCount == s->fileCount;
        const bool structural = s->resourceGraphicsValid && s->resourceAudioMapsPresent;
        const char *label = !s->directoryFound ? "GAME FOLDER UNAVAILABLE" :
                            fullVerified ? "FULL EDITION VERIFIED" :
                            baseVerified ? "MAIN DATA VERIFIED" :
                            structural ? "EGA ARCHIVE COMPATIBLE" :
                            s->resourceGraphicsValid ? "EGA OK / CHECK OTHER FILES" :
                            s->presentCount ? "CHECK GAME DATA" : "AWAITING GAME DATA";
        drawText(40, y + 15, label, (fullVerified || baseVerified || structural) ? 10 : 14);
        if (active)
            drawText(272, y + 8, ">", 15);
    }
    drawText(56, 183, "A: DETAILS    X: EGA TEST", 15);
}

static void drawMenu(const Cat3DSInstallStatus states[CAT3DS_GAME_COUNT],
                     int selected, const char *mkdirFailed)
{
    consoleClear();
    printf("CATACOMB 3DS\n");
    printf("-------------------------------\n\n");
    printf("Selected: %s\n\n", cat3dsGameName(selected));
    const Cat3DSInstallStatus *s = &states[selected];
    if (!s->directoryFound)
        printf("Game folder unavailable\n");
    else if (s->fileCount && s->validCount == s->fileCount)
        printf("Full original edition verified\n");
    else if (s->coreFileCount == 4 && s->coreValidCount == 4)
        printf("Main data verified (4/4)\nExtras %d/%d (may be needed)\n",
               s->validCount - s->coreValidCount,
               s->fileCount - s->coreFileCount);
    else if (s->resourceGraphicsValid && s->resourceAudioMapsPresent)
        printf("Original EGA archive readable!\nGame EXE: %s\n\nGame engine not linked yet.\n",
               s->resourceExeName);
    else if (!s->presentCount)
        printf("Folder ready - transfer files\nvia FTP\n");
    else
        printf("Main data: %d/%d verified\nTotal files: %d/%d\n",
               s->coreValidCount, s->coreFileCount,
               s->validCount, s->fileCount);
    printf("\nD-PAD: Choose   A: Details\n");
    printf("Y: Rescan       X: EGA test\n");
    printf("START: Exit\n");
    if (mkdirFailed)
        printf("\nSD mkdir FAILED:\n%s\n", mkdirFailed);
}

static void drawDetails(int selected, const Cat3DSInstallStatus *s,
                        const char *mkdirFailed)
{
    consoleClear();
    printf("%s\n", cat3dsGameName(selected));
    printf("-------------------------------\n\n");
    printf("Closest edition: %s\n", s->presentCount && s->versionName ?
           s->versionName : "Not detected");
    printf("\nSD path:\n%s\n\n", cat3dsGameFolder(selected));
    printf("Main data:  %d/%d verified\n", s->coreValidCount, s->coreFileCount);
    printf("Other files:%3d/%d verified\n",
           s->validCount - s->coreValidCount,
           s->fileCount - s->coreFileCount);
    printf("Full set:   %d/%d verified\n", s->validCount, s->fileCount);
    printf("Missing: %d  Size: %d  CRC: %d\n",
           s->missingCount, s->wrongSizeCount, s->wrongCrcCount);
    const bool baseVerified = s->coreFileCount == 4 && s->coreValidCount == 4;
    if (s->expectedExeName)
    {
        printf("\nExpected EXE: %s\n", s->expectedExeName);
        if (s->matchedExeName[0])
            printf("Verified EXE: %s\n", s->matchedExeName);
        else if (s->foundExeName[0])
            printf("Found EXE: %s (not matched)\n", s->foundExeName);
    }
    if (baseVerified && selected == 0)
        printf("\nA: Preview ORIGINAL title\n");
    else if (s->resourceGraphicsValid && selected > 0)
    {
        printf("\nEGA graphics: VALID FORMAT\n");
        printf("Game EXE: %s\n", s->resourceExeName);
        printf("Audio/maps: %s\n", s->resourceAudioMapsPresent ? "present" : "missing");
        printf("A: Preview STATUSPIC\n");
        printf("(Not the original intro)\n");
        printf("Gameplay still not implemented.\n");
    }
    else if (baseVerified)
        printf("\nMain data recognized. Further\nassets may still be required.\n");
    else if (!s->presentCount)
        printf("\nFTP: transfer the original\nDOS game files above.\n");
    else if (s->coreProblemFile)
        printf("\nMain file issue: %s\n%s\n", s->coreProblemFile,
               s->coreProblemReason ? s->coreProblemReason : "Invalid");
    if (selected == 0 && baseVerified)
        printf("X: Inspect ORIGINAL DOS levels\n");
    if (selected > 0 && !s->resourceGraphicsValid && s->resourceIssue[0])
        printf("\nEGA probe: %s\n", s->resourceIssue);
    if (baseVerified && s->validCount != s->fileCount && s->firstProblemFile)
        printf("\nOther file issue: %s\n%s\n", s->firstProblemFile,
               s->firstProblemReason ? s->firstProblemReason : "Invalid");
    if (mkdirFailed)
        printf("\nCannot create: %s\n", mkdirFailed);
    printf("\nB: Back   Y: Rescan\nSTART: Exit\n");
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);

    const char *mkdirFailed = NULL;
    cat3dsEnsureGameFolders(&mkdirFailed);
    Cat3DSInstallStatus installs[CAT3DS_GAME_COUNT] = {};
    cat3dsScanInstallations(installs);

    int selected = 0;
    bool details = false;
    bool titleVisible = false;
    bool mapVisible = false;
    bool walkVisible = false;
    bool analogEnabled = true;
    bool highQuality = true;
    int circleX = 0, circleY = 0;
    Cat3DSWalkPreview walk = {};
    uint64_t previousMs = osGetTime();
    uint64_t lastWalkHudMs = previousMs;
    static Cat3DSLevelMap levelMap = {};
    static Cat3DSWallTextures wallTextures = {};
    unsigned cursorX = 0, cursorY = 0;
    bool redraw = true;
    bool redrawTop = true;

    while (aptMainLoop())
    {
        hidScanInput();
        const u32 keys = hidKeysDown();
        const u32 held = hidKeysHeld();
        const uint64_t nowMs = osGetTime();
        const float frameSeconds = nowMs >= previousMs ? float(nowMs - previousMs) / 1000.f : 0.f;
        previousMs = nowMs;
        if (keys & KEY_START)
            break;

        if (walkVisible)
        {
            if (keys & (KEY_B | KEY_X))
            {
                walkVisible = false;
                redrawTop = redraw = true;
            }
            else
            {
                if (keys & KEY_A)
                {
                    cat3dsStartWalkPreview(levelMap, &walk);
                    redrawTop = redraw = true;
                }
                if (keys & KEY_Y)
                {
                    analogEnabled = !analogEnabled;
                    redraw = true;
                }
                if (keys & KEY_SELECT)
                {
                    highQuality = !highQuality;
                    redrawTop = redraw = true;
                }
                circlePosition circle = {};
                hidCircleRead(&circle);
                circleX = circle.dx;
                circleY = circle.dy;
                const float analogForward = analogEnabled ? circleAxisWithDeadzone(circleY) : 0.f;
                const float analogTurn = analogEnabled ? circleAxisWithDeadzone(circleX) : 0.f;
                const float forward = float(((held & KEY_DUP) ? 1 : 0) -
                                            ((held & KEY_DDOWN) ? 1 : 0)) + analogForward;
                const float turn = float(((held & KEY_DRIGHT) ? 1 : 0) -
                                         ((held & KEY_DLEFT) ? 1 : 0)) + analogTurn;
                const float strafe = float(((held & KEY_R) ? 1 : 0) -
                                           ((held & KEY_L) ? 1 : 0));
                if (cat3dsUpdateWalkPreview(levelMap, &walk, forward, strafe, turn,
                                            frameSeconds))
                    redrawTop = true;
                /* Periodically display raw stick input even while stationary. */
                if (nowMs - lastWalkHudMs >= 250)
                {
                    redraw = true;
                    lastWalkHudMs = nowMs;
                }
            }
        }
        else if (mapVisible)
        {
            if (keys & KEY_B)
            {
                mapVisible = false;
                redraw = redrawTop = true;
            }
            else
            {
                if (keys & KEY_X)
                {
                    if (cat3dsStartWalkPreview(levelMap, &walk))
                    {
                        char textureError[128] = {};
                        if (!cat3dsLoadCat3dWallTextures(installs[0].versionName,
                                                         installs[0].matchedExeName,
                                                         cat3dsGameFolder(0),
                                                         &wallTextures,
                                                         textureError, sizeof(textureError)))
                        {
                            /* A failed texture load must be visible and
                             * never silently misrepresent graphics as genuine. */
                            drawOriginalMapInfo(levelMap, cursorX, cursorY);
                            printf("\nWALL ART ERROR: %s\n", textureError);
                            redraw = false;
                        }
                        else
                        {
                        walkVisible = true;
                        BE_ST_SetScreenMode(0xD);
                        BE_ST_EGASetPaletteAndBorder(kEgaSignals);
                        BE_ST_SetScreenStartAddress(0);
                        BE_ST_EGASetPelPanning(0);
                        BE_ST_EGASetSplitScreen(-2);
                        lastWalkHudMs = nowMs;
                        redrawTop = redraw = true;
                        }
                    }
                    else
                    {
                        drawOriginalMapInfo(levelMap, cursorX, cursorY);
                        printf("\nNO PLAYER SPAWN / INVALID TILE\n");
                        redraw = false;
                    }
                }
                if (keys & (KEY_L | KEY_R))
                {
                    const unsigned next = (levelMap.index + ((keys & KEY_R) ? 1 : 19)) % 20;
                    char error[128] = {};
                    if (cat3dsLoadCat3dMap(installs[0].versionName,
                                            installs[0].matchedExeName,
                                            cat3dsGameFolder(0), next,
                                            &levelMap, error, sizeof(error)))
                    {
                        recenterOnPlayer(levelMap, cursorX, cursorY);
                        redrawTop = redraw = true;
                    }
                    else
                    {
                        drawOriginalMapInfo(levelMap, cursorX, cursorY);
                        printf("\nMAP ERROR: %s\n", error);
                        redraw = false;
                    }
                }
                if (keys & KEY_A) { recenterOnPlayer(levelMap, cursorX, cursorY); redrawTop = redraw = true; }
                if ((keys & KEY_DLEFT) && cursorX) { --cursorX; redrawTop = redraw = true; }
                if ((keys & KEY_DRIGHT) && cursorX + 1 < levelMap.width) { ++cursorX; redrawTop = redraw = true; }
                if ((keys & KEY_DUP) && cursorY) { --cursorY; redrawTop = redraw = true; }
                if ((keys & KEY_DDOWN) && cursorY + 1 < levelMap.height) { ++cursorY; redrawTop = redraw = true; }
            }
        }
        else if (titleVisible)
        {
            if (keys & KEY_B)
            {
                titleVisible = false;
                redrawTop = redraw = true;
            }
        }
        else if (!details)
        {
            if (keys & KEY_DUP)
            {
                selected = (selected + CAT3DS_GAME_COUNT - 1) % CAT3DS_GAME_COUNT;
                redrawTop = redraw = true;
            }
            if (keys & KEY_DDOWN)
            {
                selected = (selected + 1) % CAT3DS_GAME_COUNT;
                redrawTop = redraw = true;
            }
            if (keys & KEY_A)
            {
                details = true;
                redraw = true;
            }
            if (keys & KEY_X)
            {
                cat3dsRunVideoTest();
                redrawTop = redraw = true;
            }
        }
        else if (keys & KEY_B)
        {
            details = false;
            redraw = true;
        }
        else if ((keys & KEY_X) && selected == 0 &&
                 installs[0].coreFileCount == 4 && installs[0].coreValidCount == 4)
        {
            char error[128] = {};
            if (cat3dsLoadCat3dMap(installs[0].versionName,
                                    installs[0].matchedExeName,
                                    cat3dsGameFolder(0), 0,
                                    &levelMap, error, sizeof(error)))
            {
                mapVisible = true;
                recenterOnPlayer(levelMap, cursorX, cursorY);
                redraw = redrawTop = true;
            }
            else
            {
                drawDetails(selected, &installs[selected], mkdirFailed);
                printf("\nMAP ERROR: %s\n", error);
                redraw = false;
            }
        }
        else if ((keys & KEY_A) &&
                 ((selected == 0 && installs[0].coreFileCount == 4 &&
                   installs[0].coreValidCount == 4) ||
                  (selected > 0 && installs[selected].resourceGraphicsValid)))
        {
            char error[112] = {};
            consoleClear();
            printf("Loading original EGA art...\n");
            bool success = selected == 0 ?
                cat3dsLoadCat3dTitle(installs[0].versionName,
                                     installs[0].matchedExeName, error, sizeof(error)) :
                cat3dsShowOriginalStatusGraphic(selected, cat3dsGameFolder(selected),
                                               error, sizeof(error));
            if (success)
            {
                titleVisible = true;
                consoleClear();
                printf("%s\n", cat3dsGameName(selected));
                printf("\nAuthentic game EGA resource\n");
                printf(selected == 0 ? "TITLEPIC from DOS files.\n" :
                                       "STATUSPIC from DOS files.\n");
                printf("\nNot a running game yet.\n");
                printf("\nB: Back   START: Exit\n");
            }
            else
            {
                redraw = false;
                drawDetails(selected, &installs[selected], mkdirFailed);
                printf("\nGRAPHIC ERROR: %s\n", error);
            }
        }
        if (!titleVisible && !mapVisible && (keys & KEY_Y))
        {
            cat3dsEnsureGameFolders(&mkdirFailed);
            cat3dsScanInstallations(installs);
            redrawTop = redraw = true;
        }

        if (redrawTop && !titleVisible)
        {
            if (walkVisible)
            {
                cat3dsRenderWalkPreview(levelMap, walk, &wallTextures, highQuality);
            }
            else if (mapVisible)
                drawOriginalMap(levelMap, cursorX, cursorY);
            else
                drawLauncher(selected, installs);
            redrawTop = false;
        }
        if (redraw && !titleVisible)
        {
            if (walkVisible)
                drawWalkInfo(levelMap, walk, analogEnabled, circleX, circleY,
                             wallTextures.ready, highQuality);
            else if (mapVisible)
                drawOriginalMapInfo(levelMap, cursorX, cursorY);
            else if (details)
                drawDetails(selected, &installs[selected], mkdirFailed);
            else
                drawMenu(installs, selected, mkdirFailed);
            redraw = false;
        }
        BEL_ST_UpdateHostDisplay();
    }
    gfxExit();
    return 0;
}
