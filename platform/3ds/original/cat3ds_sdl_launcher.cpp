/* Catacomb 3DS -- four-game launcher for the ORIGINAL ReflectionHLE engine.
 * This file does NOT render any game scenes; it only selects an installation.
 */
#include <3ds.h>
#include <cstdarg>
#include <SDL3/SDL.h>

#include "cat3ds_game_files.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

extern const uint8_t g_cga_8x8TextFont[256 * 8 * 8];

namespace {
constexpr int kWidth = 400;
constexpr int kHeight = 240;
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

static bool playableCat3d(const Cat3DSInstallStatus &s)
{
    return s.coreFileCount == 4 && s.coreValidCount == 4;
}

static bool canLaunch(int game, const Cat3DSInstallStatus &s)
{
    switch (game) {
    case 0: return playableCat3d(s);
    case 1: return s.resourceGraphicsValid && s.resourceAudioMapsPresent && s.resourceLayoutValid;
    case 2: return cat3dsCanLaunchArmageddon(s);
    case 3: return cat3dsCanLaunchApocalypse(s);
    default: return false;
    }
}

static void lettering(int y, const char *title, int scale, uint8_t color)
{
    int x = (kWidth - int(strlen(title))*8*scale)/2;
    for (; *title; ++title, x+=8*scale) {
        const auto *glyph = g_cga_8x8TextFont + (unsigned char)*title*64;
        for (int v=0; v<8; ++v) for (int u=0; u<8; ++u)
            if (glyph[v*8+u]) rect(x+u*scale,y+v*scale,scale,scale,color);
    }
}

static void drawTop(bool confirmExit)
{
    rect(0,0,kWidth,kHeight,0);
    for (int y=0; y<240; y+=16)
        for (int x=-(y%32); x<400; x+=32) {
            rect(x+1,y+1,30,14,1);
            rect(x+2,y+2,28,1,8);
        }
    rect(0,49,400,142,0);
    rect(0,50,400,2,6);
    rect(0,187,400,2,6);
    if (confirmExit) {
        lettering(92,"Exit Catacomb 3DS?",2,15);
        lettering(145,"A: Exit    B: Back",1,14);
        return;
    }
    lettering(69,"The",2,11);
    lettering(101,"Catacomb 3DS",4,6);
    lettering(98,"Catacomb 3DS",4,14);
    lettering(151,"by xXNukem",2,15);
}

// Compose a complete selector page before exposing it to the LCD. A libctru
// PrintConsole would install process-wide stdout/stderr handlers and swap both screens.
struct BottomPage {
    char cells[30][40] = {};
    int col = 0, row = 0;
    void print(const char *format, ...) {
        char buf[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof buf, format, args);
        va_end(args);
        for (const char *p=buf; *p; ++p) {
            if (*p=='\n') { col=0; ++row; continue; }
            if (col==40) { col=0; ++row; }
            while (row>=30) {
                memmove(cells[0],cells[1],sizeof cells-sizeof cells[0]);
                memset(cells[29],0,sizeof cells[29]);
                --row;
            }
            cells[row][col++]=*p;
        }
    }
    void present(int highlighted=-1) const {
        u16 stride, columns;
        auto *fb=(uint16_t*)gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,&stride,&columns);
        if (!fb || stride!=240 || columns!=320) return;
        for (int y=0; y<240; ++y)
            for (int x=0; x<320; ++x) {
                unsigned ch=(unsigned char)cells[y/8][x/8];
                bool active = highlighted>=0 && y>=24+32*highlighted && y<48+32*highlighted;
                bool ink = ch && g_cga_8x8TextFont[ch*64+(y%8)*8+x%8];
                fb[x*stride+239-y]=ink ? (active ? 0xffec : 0xffff) : (active ? 0x1951 : 0x000a);
            }
        GSPGPU_FlushDataCache(fb,stride*columns*2);
        gfxScreenSwapBuffers(GFX_BOTTOM,false);
    }
};

static void drawBottom(const Cat3DSInstallStatus states[CAT3DS_GAME_COUNT],
                       int selected, bool details, const char *mkdirFailed, const char *launchError)
{
    BottomPage page;
    const Cat3DSInstallStatus &s = states[selected];
    if (!details)
    {
        page.print("\n  SELECT YOUR ADVENTURE\n\n");
        for (int i=0; i<CAT3DS_GAME_COUNT; ++i) {
            const auto &entry = states[i];
            page.print(" %c %s\n   %s\n\n\n", i==selected ? '>' : ' ', cat3dsGameName(i),
                       canLaunch(i,entry) ? "Ready to play" :
                       !entry.directoryFound ? "Unavailable - missing folder" :
                       !entry.presentCount ? "Unavailable - copy DOS data" : "Unavailable - check files");
        }
        page.print(" D-pad: Choose   A: Start\n X: Details      Y: Rescan\n START: Exit");
    }
    else
    {
        page.print("%s\n\n", cat3dsGameName(selected));
        if (s.detectedEdition)
            page.print("Detected: %s\nEXE: %s\n", s.detectedEdition, s.resourceExeName);
        page.print("Reference edition: %s\n", s.versionName ? s.versionName : "unknown");
        page.print("Main data: %d/%d\n", s.coreValidCount, s.coreFileCount);
        page.print("Other files: %d/%d\n", s.validCount - s.coreValidCount,
                    s.fileCount - s.coreFileCount);
        page.print("EGA archive: %s\n", s.resourceGraphicsValid ? "readable" : "not validated");
        if (s.coreProblemFile && s.coreProblemReason && !s.compatibleGameTables && !s.detectedEdition)
            page.print("%s: %s\n", s.coreProblemFile, s.coreProblemReason);
        if (s.compatibleGameTables && !s.detectedEdition)
            page.print("Verified EXE tables: %s\n", s.resourceExeName);
        if (selected == 1)
        {
            // SHP01..SHP12 belong to the external DOS intro program,
            // not to the linked Catacomb Abyss gameplay engine.
            const bool abyssReady = s.resourceGraphicsValid &&
                                    s.resourceAudioMapsPresent &&
                                    s.resourceLayoutValid;
            page.print("\nEGA: %s  AUDIO/MAPS: %s\n",
                        s.resourceGraphicsValid ? "OK" : "FAIL",
                        s.resourceAudioMapsPresent ? "OK" : "FAIL");
            page.print("EXE TABLES: %s\n", s.resourceLayoutValid ? "OK" : "FAIL");
            if (abyssReady)
                page.print("A: START CATACOMB ABYSS\n");
            else
                page.print("Cannot start: check failed fields\n");
            if (s.resourceIssue[0] && !abyssReady)
                page.print("Probe: %s\n", s.resourceIssue);
            if (!s.resourceShapesPresent)
                page.print("Intro SHP: absent (not game blocker)\n");
        }
        else if (selected == 3)
        {
            page.print(cat3dsCanLaunchApocalypse(s) ? "\nA: START CATACOMB APOCALYPSE\n" :
                                                       "\nCannot verify game archives or EXE tables.\n");
            page.print("Intro and hint book files optional.\n");
        }
        else if (selected == 2)
            page.print(cat3dsCanLaunchArmageddon(s) ? "\nA: START CATACOMB ARMAGEDDON\n" : "\nCannot verify game archives or EXE tables.\n");
        else if (selected == 0 && playableCat3d(s))
            page.print("\nA: START ORIGINAL CATACOMB 3-D\n(ReflectionHLE engine)\n");
        else if (selected == 0)
            page.print("\nCannot verify original game files.\n");
        else
            page.print("\nOriginal game engine for this\nepisode is not integrated yet.\n");
        if (selected >= 2 && !canLaunch(selected,s) && s.resourceIssue[0])
            page.print("%s\n", s.resourceIssue);
        page.print("\nB: Back   Y: Rescan\nSTART: Exit\n");
    }
    if (mkdirFailed)
        page.print("\nCannot create folder:\n%s\n", mkdirFailed);
    if (launchError && details) page.print("%s\n", launchError);
    page.present(details ? -1 : selected);
}
} // namespace

extern "C" int cat3dsSelectGame(Cat3DSInstallStatus *chosen, int initialGame = 0, const char *launchError = nullptr)
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
    // Optional 400x240 title artwork, including the author credit.
    SDL_Surface *art = SDL_LoadBMP("sdmc:/3ds/catacomb-3ds/logo.bmp");
    SDL_Texture *logo = art ? SDL_CreateTextureFromSurface(renderer, art) : nullptr;
    SDL_DestroySurface(art);
    const char *mkdirFailed = nullptr;
    cat3dsEnsureGameFolders(&mkdirFailed);
    static Cat3DSInstallStatus states[CAT3DS_GAME_COUNT];
    static bool haveListing;
    bool cached = haveListing && !launchError;
    if (!cached) cat3dsScanInstallations(states);
    haveListing = true;

    gfxSetScreenFormat(GFX_BOTTOM, GSP_RGB565_OES);
    int selected = initialGame;
    bool details = launchError != nullptr;
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
            confirmExit = true;
            redraw = true;
        }
        else if ((keys & KEY_B) && details)
        {
            details = false;
            redraw = true;
        }
        else if (keys & KEY_Y)
        {
            launchError = nullptr;
            cat3dsEnsureGameFolders(&mkdirFailed);
            cat3dsScanInstallations(states);
            cached = false;
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
        }
        if (!confirmExit && (keys & KEY_TOUCH) && !details) {
            touchPosition pos;
            hidTouchRead(&pos);
            if (pos.py >= 24 && pos.py < 152) {
                selected = (pos.py-24)/32;
                redraw = true;
            }
        }
        if (!confirmExit && (keys & (KEY_A | KEY_X))) {
            if (cached) cat3dsScanInstallation(selected, states[selected]);
            if ((keys & KEY_A) && canLaunch(selected,states[selected])) {
                selectionResult = selected;
                break;
            }
            details = true;
            redraw = true;
        }
        if (redraw)
        {
            drawTop(confirmExit);
            SDL_UpdateTexture(texture, nullptr, pixels, kWidth * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, logo && !confirmExit ? logo : texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
            if (confirmExit) {
                BottomPage prompt;
                prompt.print("\n  Exit application?\n\n  A: Exit    B: Back");
                prompt.present();
            } else drawBottom(states, selected, details, mkdirFailed, selected == initialGame ? launchError : nullptr);
            redraw = false;
        }
        SDL_Delay(16);
    }

    while (selectionResult >= 0) {
        if (!aptMainLoop()) {
            selectionResult = -1;
            break;
        }
        hidScanInput();
        // Only the launch button must be released before the engine sees HID.
        if (!(hidKeysHeld() & KEY_A)) break;
        SDL_Delay(16);
    }
    SDL_DestroyTexture(logo);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (selectionResult >= 0 && chosen) *chosen = states[selectionResult];
    return selectionResult;
}
