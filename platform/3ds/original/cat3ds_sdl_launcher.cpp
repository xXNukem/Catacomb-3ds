/* Catacomb 3DS -- four-game launcher for the ORIGINAL ReflectionHLE engine.
 * The layout and lettering are transferred from platform/3ds/source/main.cpp.
 * This file does NOT render any game scenes; it only selects an installation.
 */
#include <3ds.h>
#include <sys/iosupport.h>
#include <SDL3/SDL.h>

#include "cat3ds_game_files.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

extern const uint8_t g_cga_8x8TextFont[256 * 8 * 8];

namespace {
constexpr int kWidth = 320;
constexpr int kHeight = 200;
static uint32_t pixels[kWidth * kHeight];
/* IBM EGA 16-color approximation, no SDL color mixing. */
static const uint32_t kEga[16] = {
    0xff000000, 0xff0000aa, 0xff00aa00, 0xff00aaaa,
    0xffaa0000, 0xffaa00aa, 0xffaa5500, 0xffaaaaaa,
    0xff555555, 0xff5555ff, 0xff55ff55, 0xff55ffff,
    0xffff5555, 0xffff55ff, 0xffffff55, 0xffffffff
};

static void rect(int x, int y, int w, int h, uint8_t color)
{
    if (x < 0 || y < 0 || w < 1 || h < 1 || x + w > kWidth || y + h > kHeight)
        return;
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i)
            pixels[j * kWidth + i] = kEga[color & 15];
}

static void text8(int x, int y, const char *text, uint8_t color)
{
    for (; text && *text && x <= 312; ++text, x += 8)
    {
        unsigned char ch = static_cast<unsigned char>(*text);
        for (int py = 0; py < 8; ++py)
            for (int px = 0; px < 8; ++px)
                if (g_cga_8x8TextFont[ch * 64 + py * 8 + px] &&
                    x + px < kWidth && y + py < kHeight)
                    pixels[(y + py) * kWidth + x + px] = kEga[color & 15];
    }
}

static bool playableCat3d(const Cat3DSInstallStatus &s)
{
    return s.coreFileCount == 4 && s.coreValidCount == 4;
}

/* Exact layout of the existing libctru selector, now drawn before handing
 * control to the unmodified ReflectionHLE game-state initialization. */
static void drawTop(const Cat3DSInstallStatus states[CAT3DS_GAME_COUNT], int selected)
{
    rect(0, 0, 320, 200, 0);
    rect(8, 8, 304, 184, 1);
    rect(16, 16, 288, 168, 0);
    rect(16, 16, 288, 30, 1);
    text8(48, 21, "C A T A C O M B   3 D S", 15);
    text8(56, 34, "SELECT YOUR ADVENTURE", 14);
    for (int i = 0; i < CAT3DS_GAME_COUNT; ++i)
    {
        const int y = 52 + 31 * i;
        const bool active = i == selected;
        rect(24, y, 272, 27, active ? 3 : 8);
        rect(32, y + 2, 256, 23, active ? 1 : 0);
        text8(40, y + 5, cat3dsGameName(i), active ? 15 : 7);
        const Cat3DSInstallStatus &s = states[i];
        const bool core = i == 3 ? cat3dsCanLaunchApocalypse(s) : playableCat3d(s);
        const bool full = s.fileCount && s.validCount == s.fileCount;
        const bool structural = s.resourceGraphicsValid && s.resourceAudioMapsPresent;
        const char *label = !s.directoryFound ? "GAME FOLDER UNAVAILABLE" :
            full ? "FULL EDITION VERIFIED" :
            core ? "MAIN DATA VERIFIED" :
            structural ? "EGA ARCHIVE COMPATIBLE" :
            s.resourceGraphicsValid ? "EGA OK / CHECK OTHER FILES" :
            s.presentCount ? "CHECK GAME DATA" : "AWAITING GAME DATA";
        text8(40, y + 15, label, (full || core || structural) ? 10 : 14);
        if (active)
            text8(272, y + 8, ">", 15);
    }
    /* As in the original menu: A opens the selected game's details. */
    text8(56, 183, "A: DETAILS     Y: RESCAN", 15);
}

static void drawBottom(const Cat3DSInstallStatus states[CAT3DS_GAME_COUNT],
                       int selected, bool details, const char *mkdirFailed)
{
    std::printf("\x1b[2J\x1b[H");
    const Cat3DSInstallStatus &s = states[selected];
    std::printf("CATACOMB 3DS\n-------------------------------\n\n");
    std::printf("%s\n\n", cat3dsGameName(selected));
    if (!details)
    {
        if (!s.directoryFound)
            std::printf("Game folder unavailable\n");
        else if (s.fileCount && s.validCount == s.fileCount)
            std::printf("Full original edition verified\n");
        else if (playableCat3d(s))
            std::printf("Main data verified (4/4)\n");
        else if (s.resourceGraphicsValid && s.resourceAudioMapsPresent)
            std::printf("Original EGA archive readable\n");
        else if (!s.presentCount)
            std::printf("Folder ready - copy DOS data\n");
        else
            std::printf("Main data: %d/%d verified\n", s.coreValidCount, s.coreFileCount);
        std::printf("\nD-PAD: Choose  A: Details\nY: Rescan     START: Exit\n");
    }
    else
    {
        std::printf("Detected: %s\n", s.versionName ? s.versionName : "unknown");
        std::printf("Main data: %d/%d\n", s.coreValidCount, s.coreFileCount);
        std::printf("Other files: %d/%d\n", s.validCount - s.coreValidCount,
                    s.fileCount - s.coreFileCount);
        std::printf("EGA archive: %s\n", s.resourceGraphicsValid ? "readable" : "not validated");
        if (selected == 1)
        {
            // SHP01..SHP12 belong to the external DOS intro program,
            // not to the linked Catacomb Abyss gameplay engine.
            const bool abyssReady = s.resourceGraphicsValid &&
                                    s.resourceAudioMapsPresent &&
                                    s.resourceLayoutValid;
            std::printf("\nEGA: %s  AUDIO/MAPS: %s\n",
                        s.resourceGraphicsValid ? "OK" : "FAIL",
                        s.resourceAudioMapsPresent ? "OK" : "FAIL");
            std::printf("EXE TABLES: %s\n", s.resourceLayoutValid ? "OK" : "FAIL");
            if (abyssReady)
                std::printf("A: START CATACOMB ABYSS\n");
            else
                std::printf("Cannot start: check failed fields\n");
            if (s.resourceIssue[0] && !abyssReady)
                std::printf("Probe: %.72s\n", s.resourceIssue);
            if (!s.resourceShapesPresent)
                std::printf("Intro SHP: absent (not game blocker)\n");
        }
        else if (selected == 3)
        {
            std::printf(cat3dsCanLaunchApocalypse(s) ? "\nA: START CATACOMB APOCALYPSE\n" :
                                                       "\nRequires matching v1.01 game files.\n");
            std::printf("Intro and hint book files optional.\n");
        }
        else if (selected == 2)
            std::printf(cat3dsCanLaunchArmageddon(s) ? "\nA: START CATACOMB ARMAGEDDON\n" : "\nRequires matching v1.02 DOS files.\n");
        else if (selected == 0 && playableCat3d(s))
            std::printf("\nA: START ORIGINAL CATACOMB 3-D\n(ReflectionHLE engine)\n");
        else if (selected == 0)
            std::printf("\nCatacomb 3-D v1.22 requires\nmatching original game files.\n");
        else
            std::printf("\nOriginal game engine for this\nepisode is not integrated yet.\n");
        std::printf("\nB: Back   Y: Rescan\nSTART: Exit\n");
    }
    if (mkdirFailed)
        std::printf("\nCannot create folder:\n%s\n", mkdirFailed);
    std::fflush(stdout);
}
} // namespace

extern "C" int cat3dsSelectGame(void)
{
    /* SDL is started here only for menu presentation. Release all SDL
     * resources before ReflectionHLE's BE_ST_InitCommon starts its backend. */
    if (!SDL_Init(SDL_INIT_VIDEO))
        return -1;
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Catacomb 3DS", 400, 240, 0,
                                    &window, &renderer))
    {
        SDL_Quit();
        return -1;
    }
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             kWidth, kHeight);
    if (!texture)
    {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    const devoptab_t *oldOut = devoptab_list[STD_OUT];
    const devoptab_t *oldErr = devoptab_list[STD_ERR];
    consoleInit(GFX_BOTTOM, nullptr);

    const char *mkdirFailed = nullptr;
    cat3dsEnsureGameFolders(&mkdirFailed);
    Cat3DSInstallStatus states[CAT3DS_GAME_COUNT] = {};
    cat3dsScanInstallations(states);
    int selected = 0;
    bool details = false;
    bool confirmExit = false;
    bool redraw = true;
    int selectionResult = -1;
    u32 previousHeld = 0;

    while (aptMainLoop())
    {
        SDL_Event ev;
        bool quit = false;
        while (SDL_PollEvent(&ev))
            if (ev.type == SDL_EVENT_QUIT)
                quit = true;
        /* SDL3's Nintendo 3DS event pump already scans HID input.
         * Repeated scans can clear hidKeysDown() before it is read.
         * Held input persists; derive just-pressed transitions here. */
        hidScanInput();
        const u32 held = hidKeysHeld();
        const u32 keys = held & ~previousHeld;
        previousHeld = held;
        if (quit) break;
        if (confirmExit) {
            if (keys & KEY_A) break;
            if (keys & (KEY_B | KEY_START)) {
                confirmExit = false;
                redraw = true;
            } else {
                SDL_Delay(16);
                continue;
            }
        } else if (keys & KEY_START) {
            std::printf("\x1b[2J\x1b[HExit Catacomb 3DS?\n\nA: Exit    B: Back\n");
            confirmExit = true;
            continue;
        }
        else if ((keys & KEY_B) && details)
        {
            details = false;
            redraw = true;
        }
        else if (keys & KEY_Y)
        {
            cat3dsEnsureGameFolders(&mkdirFailed);
            cat3dsScanInstallations(states);
            redraw = true;
        }
        else if (!details)
        {
            if (keys & KEY_DUP)
            {
                selected = (selected + CAT3DS_GAME_COUNT - 1) % CAT3DS_GAME_COUNT;
                redraw = true;
            }
            if (keys & KEY_DDOWN)
            {
                selected = (selected + 1) % CAT3DS_GAME_COUNT;
                redraw = true;
            }
            if (keys & KEY_A)
            {
                details = true;
                redraw = true;
            }
        }
        else if (keys & KEY_A)
        {
            if (selected == 0 && playableCat3d(states[0]))
            {
                selectionResult = 0;
                break;
            }
            if (selected == 3 && cat3dsCanLaunchApocalypse(states[3]))
            {
                selectionResult = 3;
                break;
            }
            if (selected == 2 && cat3dsCanLaunchArmageddon(states[2]))
            {
                selectionResult = 2;
                break;
            }
            if (selected == 1 && states[1].resourceGraphicsValid &&
                states[1].resourceAudioMapsPresent && states[1].resourceLayoutValid)
            {
                selectionResult = 1;
                break;
            }
        }
        if (redraw)
        {
            drawTop(states, selected);
            SDL_UpdateTexture(texture, nullptr, pixels, kWidth * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
            drawBottom(states, selected, details, mkdirFailed);
            redraw = false;
        }
        SDL_Delay(16);
    }

    while (selectionResult >= 0 && aptMainLoop()) {
        hidScanInput();
        if (!hidKeysHeld()) break;
        SDL_Delay(16);
    }
    // The text console points into the selector framebuffer until gfxExit.
    std::fflush(stdout);
    std::fflush(stderr);
    devoptab_list[STD_OUT] = oldOut;
    devoptab_list[STD_ERR] = oldErr;
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return selectionResult;
}
