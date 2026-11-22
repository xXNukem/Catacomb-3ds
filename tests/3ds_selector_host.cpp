// Production selector + SDL dummy video. Storage probes and physical HID are
// synthetic. The game callback stands in for a returning original engine.
#include <SDL3/SDL.h>
#include <3ds.h>
#include <sys/iosupport.h>
#include <cassert>
#include <vector>
#include "cat3ds_game_files.h"
#include "cat3ds_session.h"

enum {KEY_X=128,KEY_TOUCH=256,KEY_A=1,KEY_B=2,KEY_START=8,KEY_Y=16,KEY_DUP=32,KEY_DDOWN=64};
struct touchPosition { unsigned px,py; };
static unsigned touchY=120;
static void hidTouchRead(touchPosition *p) { p->px=80; p->py=touchY; }
static std::vector<u32> inputs;
static size_t cursor;
static u32 down;
static const devoptab_t consoleDevice, initialDevice;
const devoptab_t *devoptab_list[3]={nullptr,&initialDevice,&initialDevice};
void consoleInit(int,void*) {
    assert(false && "selector must not install a text console");
}
bool aptMainLoop() { return cursor<inputs.size(); }
void hidScanInput() { down=inputs.at(cursor++); }
// Circle Pad held across the handoff.
u32 hidKeysHeld() { return down | (1u << 28); }
bool cat3dsEnsureGameFolders(const char **) { return true; }
static bool installed=true;
static unsigned fullScans, singleScans;
void cat3dsScanInstallations(Cat3DSInstallStatus *out)
{
    ++fullScans;

    for (int i=0;i<4;++i) {
        out[i].directoryFound=1;
        out[i].coreFileCount=4;
        out[i].coreValidCount=installed?4:0;
        out[i].resourceLayoutValid=out[i].resourceGraphicsValid=out[i].resourceAudioMapsPresent=installed;
        out[i].fileCount=2;
        out[i].validCount=installed?2:0;
    }
}
void cat3dsScanInstallation(int, Cat3DSInstallStatus &state) {
    assert(state.directoryFound);
    state.coreValidCount=installed?4:0;
    state.validCount=installed?2:0;
    state.resourceLayoutValid=state.resourceGraphicsValid=state.resourceAudioMapsPresent=installed;
    ++singleScans;
}
const char *cat3dsGameName(int) { return "Synthetic installation"; }
bool cat3dsCanLaunchApocalypse(const Cat3DSInstallStatus &s) { return s.coreValidCount==4; }
bool cat3dsCanLaunchArmageddon(const Cat3DSInstallStatus &s) { return s.validCount==s.fileCount; }

#include "../platform/3ds/original/cat3ds_sdl_launcher.cpp"
const uint8_t g_cga_8x8TextFont[256*64]={};
int cat3dsSessionAction;

static void prepare(int game)
{
    cursor=0;
    inputs={KEY_START,0,KEY_B,0}; // Cancel an explicit application exit.
    for (int i=0;i<game;++i) inputs.insert(inputs.end(),{KEY_DDOWN,0});
    inputs.insert(inputs.end(),{KEY_A,0});
}

int main()
{
    unsigned char data[]={9},zero[]={0};
    int session=0;
    const int sequence[]={0,1,2,3,0,3,0,1,1,2,2,3,0};
    const int count = sizeof sequence / sizeof *sequence;
    assert(!Cat3DS_RunSessions(data,1,zero,1,[&] {
        prepare(sequence[session]);
        assert(cat3dsSelectGame(nullptr)==sequence[session]);
        assert(cursor==inputs.size() && !down && !SDL_WasInit(0));
        assert(devoptab_list[STD_OUT]==&initialDevice && devoptab_list[STD_ERR]==&initialDevice);
        assert(data[0]==9 && !zero[0]);
        assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO));
        SDL_Window *window = SDL_CreateWindow("engine", 400, 240, 0);
        SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 44100 };
        SDL_AudioStream *audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                                           &spec, nullptr, nullptr);
        assert(window && audio && SDL_ResumeAudioStreamDevice(audio));
        data[0]=0; zero[0]=1;
        SDL_DestroyAudioStream(audio);
        SDL_DestroyWindow(window);
        SDL_Quit();
        cat3dsSessionAction=++session==count?CatExit:CatSwitch;
        return 0;
    },[] { return true; }));
    assert(session==count);
    prepare(0);
    inputs.pop_back(); // System exit while waiting for the launch button to be released.
    assert(cat3dsSelectGame(nullptr)==-1 && !SDL_WasInit(0));
    // A launch-time recheck can also return directly to episode details.
    cursor=0;
    inputs={KEY_B,0,KEY_DDOWN,0,KEY_A,0,KEY_A,0};
    assert(cat3dsSelectGame(nullptr, 1, "Gameplay descriptor recheck failed")==2);
    Cat3DSInstallStatus chosen = {};
    auto before = fullScans;
    auto rechecks = singleScans;
    prepare(0);
    assert(cat3dsSelectGame(&chosen)==0 && fullScans==before && singleScans==rechecks+1);
    cursor=0;
    touchY=120;
    inputs={KEY_TOUCH,0};
    assert(cat3dsSelectGame(nullptr)==3 && cursor==inputs.size());
    // Touching outside the list does not launch anything.
    cursor=0; touchY=8;
    inputs={KEY_TOUCH,0,KEY_A,0};
    assert(cat3dsSelectGame(nullptr)==0 && cursor==inputs.size());
    installed=false;
    cursor=0; touchY=24;
    inputs={KEY_TOUCH,0,KEY_B,0,KEY_START,0,KEY_A};
    assert(cat3dsSelectGame(nullptr)==-1 && cursor==inputs.size());
    for (int game=0;game<4;++game) {
        prepare(game);
        inputs.insert(inputs.end(),{KEY_START,0,KEY_A});
        assert(cat3dsSelectGame(nullptr)==-1 && cursor==inputs.size());
    }
}
