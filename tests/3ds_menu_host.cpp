// Exercise the production input loop and both page renderers with scripted HID.
#include <3ds.h>
#include <cassert>
#include "cat3ds_prompts.h"
#include <vector>
enum { KEY_DUP=1, KEY_DDOWN=2, KEY_DLEFT=4, KEY_DRIGHT=8, KEY_A=16, KEY_B=32,
       KEY_START=64, KEY_SELECT=128 };
static std::vector<u32> held;
static size_t indexHeld;
static u32 heldDirection;
static bool ownsScreens;
static int releases;
bool aptMainLoop() { return indexHeld < held.size(); }
void BE_ST_PollEvents() {}
u32 hidKeysHeld() { return held.at(indexHeld++) | heldDirection; }
static unsigned wallTicks;
void gspWaitForVBlank() { ++wallTicks; }
#include "../platform/3ds/original/cat3ds_menu.cpp"
#include "../platform/3ds/original/cat3ds_session.h"
const uint8_t g_cga_8x8TextFont[256*64] = {};
void Cat3DS_DualScreenEnterMenu() { assert(!ownsScreens); ownsScreens=true; }
void Cat3DS_DualScreenLeaveMenu() { assert(ownsScreens); ownsScreens=false; ++releases; }

static int choose(Cat3DSMenu &menu, std::initializer_list<u32> presses)
{
    held = {KEY_START, KEY_START, 0};
    for (auto key : presses) held.insert(held.end(), {key,key,0});
    indexHeld=0;
    heldDirection = 1u << 28; // Circle Pad remains held throughout the dialog.
    const int result=Cat3DS_OpenMenu(menu);
    heldDirection = 0;
    assert(!ownsScreens && indexHeld==held.size() && !held.back());
    return result;
}

// Real per-engine dispatch with synthetic clocks, key states and slot hooks.
namespace engine {
using id0_longword_t = unsigned;
enum { sc_None, sc_Escape, sc_F1, sc_F2, sc_F3, sc_F4, FL_DEAD=8 };
enum { ex_abort=1, ex_loadedgame, ex_resetgame, gd_Normal };
enum SDMode { sdm_Off, sdm_PC, sdm_AdLib };
enum SMMode { smm_Off, smm_AdLib };
struct BE_ST_ControllerMapping {};
int Flags, LastScan, playstate, restartgame, depth;
bool Keyboard[8], Paused, MousePresent, abortgame, loadedgame, AdLibPresent=true;
unsigned lasttimecount;
SDMode SoundMode=sdm_PC;
SMMode MusicMode=smm_Off;
unsigned SD_GetTimeCount() { return wallTicks; }
void SD_SetTimeCount(unsigned ticks) { wallTicks=ticks; }
void SD_SetSoundMode(SDMode mode) { SoundMode=mode; }
void SD_SetMusicMode(SMMode mode) { MusicMode=mode; }
void SD_MusicOff() {}
void SD_MusicOn() {}
void StartMusic() {}
void StopMusic() {}
void CA_LoadAllSounds() {}
void NewGame() {}
void NormalScreen() {}
void FreeUpMemory() {}
void CacheScaleds() {}
void DrawPlayScreen() {}
unsigned saves, loads;
void Cat3DS_GameSlots(bool saving) { if (saving) ++saves; else ++loads; }
void BE_ST_AltControlScheme_Push() { assert(!depth++); }
void BE_ST_AltControlScheme_Pop() { assert(--depth==0); }
void BE_ST_AltControlScheme_PrepareControllerMapping(BE_ST_ControllerMapping*) { assert(depth==1); }
void BE_ST_GetEmuAccuMouseMotion(void*,void*) {}
void IN_ClearKeysDown() { memset(Keyboard,0,sizeof Keyboard); LastScan=sc_None; }
#include "../platform/3ds/original/cat3ds_game_menu.inc"

void checkDispatch()
{
    int row=-1;
    assert(!Cat3DS_HandleMenu(false,row));
    Paused=true;
    held={KEY_SELECT,0,KEY_SELECT,KEY_SELECT,0};
    indexHeld=0;
    wallTicks=19;
    assert(Cat3DS_HandleMenu(false,row));
    assert(wallTicks==19 && lasttimecount==19 && !Paused && !depth && row==-1);
    for (int target : {CatSave,CatLoad,CatSwitch}) {
        cat3dsSessionAction=CatResume;
        held={KEY_START,0};
        for (int i=0;i<target;++i) held.insert(held.end(),{KEY_DDOWN,0});
        held.insert(held.end(),{KEY_A,0});
        if (target==CatSwitch) held.insert(held.end(),{KEY_A,0});
        indexHeld=0;
        LastScan=sc_Escape;
        row=-1;
        auto handled=Cat3DS_HandleMenu(false,row);
        assert(wallTicks==19 && !depth && indexHeld==held.size());
        if (target==CatSwitch) assert(handled && cat3dsSessionAction==CatSwitch && playstate==ex_abort);
        else {
            assert(row==target);
#ifdef REFKEEN_VER_CAT3D
            assert(handled && saves==1 && loads==unsigned(target==CatLoad));
#else
            assert(!handled && Keyboard[target==CatSave ? sc_F3 : sc_F4]);
#endif
        }
    }
#ifndef REFKEEN_VER_CAT3D
    playstate=0;
    Flags=FL_DEAD;
    row=-1;
    held={0,KEY_B,0,KEY_START,0,KEY_DDOWN,0,KEY_DDOWN,0,KEY_DDOWN,0,KEY_A,0,
          KEY_DDOWN,0,KEY_A,0,KEY_A,0};
    indexHeld=0;
    assert(Cat3DS_HandleMenu(false,row));
    assert(!Flags && playstate==ex_resetgame && !depth && indexHeld==held.size());
#endif
}
}

int main()
{
    Cat3DSMenu menu={};
    for (auto page : {CatRoot, CatSettings, CatGuide, CatConfirm, CatPause}) {
        menu.page=page;
        draw(menu);
        assert(top[0] && top[239] && top[399*240] && top[400*240-1]);
    }
    StereoBench bench;
    for (int pos=0; pos<=10; ++pos) {
        float sliderPos = pos/10.f;
        assert(!bench.offset(4,sliderPos,true));
        for (int z : {2,4,10})
            assert(bench.offset(z,sliderPos,false)==-bench.offset(z,sliderPos,true));
    }
    assert(bench.offset(2,1,true)<0 && bench.offset(10,1,true)>0);
    bench.adjust(CatAccept);
    assert(bench.offset(10,1,true)<0);
    for (int i=0;i<8;++i) { bench.adjust(CatRight); bench.adjust(CatUp); }
    assert(bench.separation==4 && bench.convergence==5 && bench.offset(2,1,true)<=2);
    bench.adjust(CatSelect);
    drawBench(bench,75);
    assert(hwStereo && !bench.swapped && bench.convergence==4 && bench.separation==2);
    assert(top[105*240+239-50]==0xffff && rightEye[105*240+239-50]==0xffff);
    assert(top[198*240+239-105]==0x07ff && rightEye[198*240+239-105]==0x07ff);
    gfxSet3D(false);
    const char scroll[]="Press ENTER or ESC to exit.\nNext page";
    assert(!strcmp(Cat3DS_ControlPrompt(scroll,strchr(scroll,'\n')),"Press A or B to close."));
    assert(!strcmp(Cat3DS_ControlPrompt("Press SPACE to try again.",nullptr),"Press A to try again."));
    assert(!Cat3DS_ControlPrompt("You enter a new area ...",nullptr));
    for (int game=0; game<4; ++game) {
        Cat3DSMenu m = {CatRoot,CatResume,0,0,game,1,0,true,false,false};
        assert(choose(m,{KEY_DDOWN,KEY_A})==CatSave);
        assert(choose(m,{KEY_DDOWN,KEY_A})==CatLoad);
        assert(choose(m,{KEY_DDOWN,KEY_A,KEY_DRIGHT,KEY_B,KEY_B})==CatResume);
        assert(m.guide==1);
        m.row=CatHelp;
        if (game) assert(choose(m,{KEY_A})==CatHelp);
        else assert(choose(m,{KEY_A,KEY_B,KEY_B})==CatResume);
        m.row=CatOptions;
        assert(choose(m,{KEY_A,KEY_DRIGHT,KEY_B,KEY_START})==CatResume && m.sound==2);
        m.row=CatSwitch;
        assert(choose(m,{KEY_A,KEY_B,KEY_B})==CatResume);
        assert(choose(m,{KEY_A,KEY_A})==CatSwitch);
        m.page=CatRoot; m.row=CatExit;
        assert(choose(m,{KEY_A,KEY_A})==CatExit);
        m.page=CatPause;
        assert(choose(m,{KEY_SELECT})==CatResume);
        m.page=CatRoot; m.row=CatLoad; m.dead=true;
        assert(choose(m,{KEY_B,KEY_START,KEY_A})==CatLoad);
    }
    // Same-edition restart and alternating selectors, using the actual
    // restart controller with mock engine memory and cleanup.
    unsigned char data[]={9,8,7}, zero[4]={};
    int starts=0, closes=0, directories=0;
    const int games[]={0,0,3,1,2,0,3,1,2};
    assert(!Cat3DS_RunSessions(data,sizeof data,zero,sizeof zero,[&] {
        assert(cat3dsSessionAction==CatResume && starts==closes);
        assert(data[0]==9 && data[1]==8 && data[2]==7);
        for (auto value:zero) assert(!value);
        zero[games[starts]]=1;
        memset(data,0,sizeof data);
        ++starts;
        cat3dsSessionAction=starts==9?CatExit:CatSwitch;
        ++closes;
        return 0;
    }, [&] { ++directories; return true; }));
    assert(starts==9 && closes==9 && directories==8 && releases==40);
    engine::checkDispatch();
}
