// Run each original sound manager against a recording timer/device.
#ifdef REFKEEN_VER_CAT3D
#include "c3_def.h"
#else
#include "def.h"
#endif
#include <cassert>
#include <cstdlib>
#include <initializer_list>

int g_be_argc = 1;
const char **g_be_argv;
static void (*timerTick)();
static int pcFrequency, oplFrequency, locks;
void BE_ST_StartAudioAndTimerInt(void (*tick)()) { timerTick = tick; }
void BE_ST_StopAudioAndTimerInt() { timerTick = nullptr; }
void BE_ST_SetTimer(uint16_t) {}
void BE_ST_LockAudioRecursively() { ++locks; }
void BE_ST_UnlockAudioRecursively() { assert(locks > 0); --locks; }
bool BE_ST_IsEmulatedOPLChipReady() { return true; }
void BE_ST_OPL2Write(uint8_t reg, uint8_t value) { if (reg == 0xa0) oplFrequency = value; }
void BE_ST_PCSpeakerSetConstVal(bool) {}
void BE_ST_PCSpeakerSetInvFreq(uint16_t freq) { pcFrequency = freq; }
void BE_ST_ShortSleep() {}
REFKEEN_NS_B
id0_byte_t *audiosegs[NUMSNDCHUNKS];
id0_int_t US_CheckParm(const id0_char_t *, const id0_char_t **) { return -1; }
#ifdef REFKEEN_VER_CAT3D
void Quit(const id0_char_t *) { abort(); }
#else
void Quit(const id0_char_t *, ...) { abort(); }
#endif
REFKEEN_NS_E

int main()
{
#if defined(REFKEEN_VER_CAT3D)
    using namespace cat3d;
#elif defined(REFKEEN_VER_CATABYSS)
    using namespace catabyss;
#elif defined(REFKEEN_VER_CATAPOC)
    using namespace catapoc;
#else
    using namespace catarm;
#endif
    PCSound pc = {};
    AdLibSound opl = {};
    pc.common.length = opl.common.length = 1;
    pc.data[0] = opl.data[0] = 11;
    opl.inst.mSus = 1;
    audiosegs[STARTPCSOUNDS + GETBOLTSND] = reinterpret_cast<uint8_t *>(&pc);
    audiosegs[STARTADLIBSOUNDS + GETBOLTSND] = reinterpret_cast<uint8_t *>(&opl);
    SD_Startup();
    assert(timerTick && AdLibPresent);
    for (SDMode mode : {sdm_PC, sdm_AdLib}) {
        assert(SD_SetSoundMode(mode));
        SD_PlaySound(GETBOLTSND);
        assert(SD_SoundPlaying() == GETBOLTSND);
        for (int tick = 0; tick < 16; ++tick) timerTick();
        assert(!SD_SoundPlaying());
    }
    assert(pcFrequency == 11 * 60 && oplFrequency == 11);
#ifdef REFKEEN_VER_CAT3D
    uint16_t sequence[] = {4, 0x2aa0, 1};
    assert(SD_SetMusicMode(smm_AdLib));
    SD_StartMusic(reinterpret_cast<MusicGroup *>(sequence));
    timerTick();
    assert(oplFrequency == 0x2a);
    SD_MusicOff();
    oplFrequency = 0;
    timerTick();
    assert(!oplFrequency);
    SD_MusicOn();
    timerTick();
    assert(oplFrequency == 0x2a);
#endif
    SD_Shutdown();
    assert(!timerTick && locks == 0);
    SD_Startup();
    assert(timerTick && SD_SetSoundMode(sdm_PC));
    SD_PlaySound(GETBOLTSND);
    SD_StopSound();
    assert(!SD_SoundPlaying());
    SD_Shutdown();
    assert(!timerTick && locks == 0);
    puts("synthetic PC speaker and AdLib effects: PASS");
}
