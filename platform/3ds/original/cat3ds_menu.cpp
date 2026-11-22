#include "cat3ds_menu.h"
#include "cat3ds_dualscreen.h"
#include <3ds.h>
#include <stdint.h>
#include <cmath>
#include <cstdio>
#include <string.h>

extern const uint8_t g_cga_8x8TextFont[256*64];
void BE_ST_PollEvents();
int cat3dsSessionAction = CatResume;

static void line(uint16_t *fb, int stride, int x, int y, const char *s, uint16_t color)
{
    for (; *s && x+8<=400; ++s,x+=8) {
        const uint8_t *glyph=g_cga_8x8TextFont+(unsigned char)*s*64;
        for (int v=0;v<8;++v) for (int u=0;u<8;++u)
            if (glyph[v*8+u]) fb[(x+u)*stride+stride-1-y-v]=color;
    }
}

// Parallel cameras, focal length 200 LCD pixels (90 degree horizontal FOV).
struct StereoBench {
    int separation = 2, convergence = 4;
    bool swapped = false;
    int offset(int z, float slider, bool right) const {
        const float shift = 2.0f * separation * slider * (1.0f/convergence - 1.0f/z);
        return int(std::lround((right != swapped ? 1 : -1) * std::fmax(-2.f, std::fmin(2.f, shift))));
    }
    void adjust(Cat3DSButton key) {
        if (key == CatLeft && separation > 0) --separation;
        if (key == CatRight && separation < 4) ++separation;
        if (key == CatUp && convergence < 5) ++convergence;
        if (key == CatDown && convergence > 3) --convergence;
        if (key == CatAccept) swapped = !swapped;
        if (key == CatSelect) *this = StereoBench();
    }
};

static void drawBench(const StereoBench &bench, unsigned frame)
{
    const float slider = osGet3DSliderState();
    const bool stereo = slider > 0;
    gfxSet3D(stereo);
    for (int eye = 0; eye <= int(stereo); ++eye) {
        u16 stride, columns;
        auto *fb = (uint16_t*)gfxGetFramebuffer(GFX_TOP, eye ? GFX_RIGHT : GFX_LEFT, &stride, &columns);
        if (!fb || stride != 240 || columns != 400) return;
        memset(fb, 0, stride*columns*2);
        line(fb,stride,16,12,stereo ? ((bool(eye) != bench.swapped) ? "R" : "L") : "MONO",0xffff);
        char info[48];
        snprintf(info,sizeof info,"Separation %d/4   Screen Z %d",bench.separation,bench.convergence);
        line(fb,stride,48,12,info,0x07ff);
        for (int i=0; i<3; ++i) {
            int z = i==0 ? 2 : i==1 ? 4 : 10;
            int x = 70 + 128*i + bench.offset(z,slider,eye);
            int half = 84/z;
            for (int u=-half; u<=half; ++u)
                for (int v=-half; v<=half; ++v)
                    fb[(x+u)*stride+239-105-v] = i==0 ? 0xfd20 : i==1 ? 0x07ff : 0x87f0;
            line(fb,stride,x-16,154,i==0 ? "Z=2" : i==1 ? "Z=4" : "Z=10",0xffff);
        }
        // Shared moving fiducial at screen depth exposes temporal or vertical skew.
        int x = 30 + frame%340;
        for (int y=50; y<62; ++y) fb[x*stride+239-y]=0xffff;
        line(fb,stride,16,178,"Left/right: separation   Up/down: screen Z",0xffff);
        line(fb,stride,16,198,"A: swap eyes   SELECT: reset   B: back",0xffff);
        GSPGPU_FlushDataCache(fb,stride*columns*2);
    }
    gfxScreenSwapBuffers(GFX_TOP,stereo);
}

static uint16_t *beginPage(u16 &stride)
{
    u16 columns;
    auto *fb=(uint16_t*)gfxGetFramebuffer(GFX_TOP,GFX_LEFT,&stride,&columns);
    if (!fb || stride<240 || columns<400) return nullptr;
    for (int x=0;x<400;++x) for (int y=0;y<240;++y)
        fb[x*stride+239-y] = y==37 || y==198 ? 0x8950 : y<37 || y>198 ? 0x108a : 0x000a;
    return fb;
}

static void presentPage(uint16_t *fb, u16 stride)
{
    GSPGPU_FlushDataCache(fb,stride*400*2);
    gfxScreenSwapBuffers(GFX_TOP,false);
}

extern "C" void Cat3DS_DrawSlotPage(const char *title, const char *const *names,
                                     const char *const *dates, int selected, const char *footer)
{
    u16 stride;
    auto *fb=beginPage(stride);
    if (!fb) return;
    line(fb,stride,24,18,title,0x07ff);
    if (names) for (int i=0;i<6;++i) {
        char row[45];
        snprintf(row,sizeof row,"%c %d  %.32s",i==selected ? '>' : ' ',i+1,names[i]);
        line(fb,stride,22,46+i*25,row,i==selected ? 0xffe0 : 0xffff);
        line(fb,stride,62,56+i*25,dates[i],0xad75);
    }
    line(fb,stride,24,209,footer,0x07ff);
    presentPage(fb,stride);
}

static void draw(const Cat3DSMenu &m)
{
    u16 stride;
    auto *fb=beginPage(stride);
    if (!fb) return;
    auto label=[&](int y,const char *s,bool selected=false){
        if (selected) line(fb,stride,22,y,">",0xffe0);
        line(fb,stride,38,y,s,selected ? 0xffe0 : 0xffff);
    };
    const char *games[]={"CATACOMB 3-D","CATACOMB ABYSS","CATACOMB ARMAGEDDON","CATACOMB APOCALYPSE"};
    line(fb,stride,24,22,games[m.game],0x07ff);
    if (m.page==CatRoot) {
        const char *items[]={"Resume game","Save game","Load game","Controls","Original help","Options","Switch game","Exit application"};
        for (int i=m.dead?2:0;i<8;++i) {
            if (m.title && i == CatSave) continue;
            label(48+i*18,m.title && i == CatResume ? "Start game" : items[i],i==m.row);
        }
    } else if (m.page==CatSettings) {
        const char *sounds[]={"Sound: off","Sound: PC speaker","Sound: AdLib"};
        label(56,sounds[m.sound],m.subrow==0);
        if (m.game==0) {
            label(80,m.music?"Music: AdLib":"Music: off",m.subrow==1);
            label(104,"Original options and extras",m.subrow==2);
        }
        label(m.game==0?152:104,"Technical: stereo test",m.subrow==(m.game==0?4:2));
        label(m.game==0?128:80,"New game",m.subrow==(m.game==0?3:1));
    } else if (m.page==CatGuide && !m.guide) {
        label(48,"Controls 1/2");
        label(72,"Circle Pad/D-pad/C-Stick: move, turn");
        label(94,"B or L + left/right: strafe");
        label(116,"A: fire    X: drink potion");
        label(138,m.game ? "Y: Zapper    ZL: Xterminator" : "Y: cast bolt    ZL: cast nuke");
        label(160,m.game ? "R: quick turn" : "R: run and turn faster");
        label(182,"D-pad right: next page");
    } else if (m.page==CatGuide) {
        label(48,"Controls 2/2");
        if (m.game<2) {
            label(70,"ZR + up/right/down/left: scrolls 1-4");
            label(86,"ZR + Y/B/A/X: scrolls 5-8");
        } else {
            label(70,"ZR + up: help, right: sound");
            label(86,"ZR + down: save, left: load");
        }
        label(112,"START: menu    SELECT: pause/resume");
        label(134,"D-pad: select    A: confirm    B: back");
        label(156,"Save and load: six slots");
        label(178,"Switch games from the START menu.");
    } else if (m.page==CatConfirm) {
        if (!m.title) label(80,"Unsaved progress will be lost.");
        label(112,m.subrow==CatNew?"Start a new game?":m.row==CatSwitch?"Return to the game selector?":"Exit Catacomb 3DS?");
    } else if (m.page==CatPause) label(96,"Paused. Press SELECT to resume.");
    else label(96,"This edition has no built-in help.");
    line(fb,stride,24,209,"A Confirm    B Back    START Close",0x07ff);
    presentPage(fb,stride);
}

int Cat3DS_OpenMenu(Cat3DSMenu &m)
{
    Cat3DS_DualScreenEnterMenu();
    const u32 buttons[]={KEY_DUP,KEY_DDOWN,KEY_DLEFT,KEY_DRIGHT,KEY_A,KEY_B,KEY_START,KEY_SELECT};
    u32 menuButtons = 0;
    for (u32 button : buttons) menuButtons |= button;
    bool armed=false, dirty=true;
    int result=-1;
    StereoBench bench;
    unsigned frame=0;
    while (aptMainLoop()) {
        BE_ST_PollEvents();
        // SDL's N3DS event pump scans HID before dispatching controller events.
        // A held Circle Pad direction or touch contact must not block confirmation.
        u32 held=hidKeysHeld() & menuButtons;
        if (!held) {
            if (result>=0) break;
            armed=true;
        } else if (armed && result<0) {
            for (int i=0;i<8;++i) if (held & buttons[i]) {
                if (m.page==CatDiagnostic) bench.adjust((Cat3DSButton)i);
                const bool diagnostic = m.page==CatDiagnostic;
                result=m.step((Cat3DSButton)i);
                if (diagnostic && m.page!=CatDiagnostic) gfxSet3D(false);
                dirty=true; break;
            }
            armed=false;
        }
        if (m.page==CatDiagnostic) drawBench(bench,frame++);
        else if (dirty) { draw(m); dirty=false; }
        gspWaitForVBlank();
    }
    if (m.page==CatDiagnostic) gfxSet3D(false);
    Cat3DS_DualScreenLeaveMenu();
    return result<0 ? CatExit : result;
}

void Cat3DS_WaitForPadRelease()
{
    while (aptMainLoop()) {
        BE_ST_PollEvents();
        if (!hidKeysHeld()) return;
        gspWaitForVBlank();
    }
}
