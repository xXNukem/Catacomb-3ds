// Exercise the actual mixer, PC speaker and SDL stream with the dummy audio driver.
#include "backend/audio/be_audio_mixer.c"
#include "be_sound_device_flags.h"
#include <cassert>
#include <initializer_list>
#include <cstdlib>

RefKeenConfig g_refKeenCfg = {};

extern bool g_sdlAudioSubsystemUp, g_sdlAudioInitDone;
extern SDL_AudioStream *g_sdlAudioStream;
void (*g_sdlTimerIntFuncPtr)();
void BE_ST_InitAudio();
void BE_ST_ShutdownAudio();
int BE_Cross_GetSelectedGameVerAudioDeviceFlags() { return BE_AUDIO_DEVICE_PCSPKR_REQUIRED | BE_AUDIO_DEVICE_OPL; }
void BE_ST_ExitWithErrorMsg(const char*) { abort(); }
void BE_Cross_LogMessage(BE_Log_Message_Class_T, const char*, ...) {}
void BE_ST_SetTimer(uint16_t word) { BEL_ST_AudioMixerUpdateFromPITRateWord(word ? word : 65536); }
void BEL_ST_SetDigiMixerSource(BE_ST_AudioMixerSource*) { abort(); }
void BEL_ST_GenDigiSamples(BE_ST_SndSample_T*,int) { abort(); }

static int nextSample;
static void ramp(BE_ST_SndSample_T *out, int n)
{
    while (n--) *out++ = ++nextSample;
}

int main()
{
    BESDLResamplingContext context = {};
    BE_ST_SndSample_T input[] = {123, 234}, output[4] = {};
    BEL_ST_InitResampling(&context, 48000, 24000, input);
    uint32_t consumed, produced;
    BEL_ST_DoResample(&context, &consumed, &produced, input, output, 2, 3);
    assert(consumed == 2 && produced == 3);
    BEL_ST_DoResample(&context, &consumed, &produced, input, output+3, 0, 1);
    assert(!consumed && produced == 1 && output[0] == 123 && output[1] == 123 &&
           output[2] == 234 && output[3] == 234);
    BEL_ST_ShutdownResampling(&context);
    assert(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    g_refKeenCfg.sndSampleRate=48000;
    BE_ST_SndSample_T full[4097], split[4097];
    for (auto &sample : full) sample = 12345;
    BEL_ST_ResetOPLChip();
    BEL_ST_GenOPLSamples(full,4097);
    BEL_ST_ResetOPLChip();
    BEL_ST_GenOPLSamples(split,2048);
    BEL_ST_GenOPLSamples(split+2048,2048);
    BEL_ST_GenOPLSamples(split+4096,1);
    assert(!memcmp(full,split,sizeof full));
    for (int channels=1; channels<=2; ++channels) {
        nextSample=0;
        BEL_ST_AudioMixerInit(48000,channels);
        BEL_ST_AudioMixerAddSource(48000,512,BE_AUDIO_VOL_MAX,ramp);
        BE_ST_SetTimer(1000);
        BE_ST_SndSample_T block[514];
        int position=0;
        for (int n : {1,63,64,65,257}) {
            BEL_ST_AudioMixerCallback(block,n);
            for (int j=0;j<n*channels;++j)
                assert(block[j]==position+j/channels+1);
            position+=n;
        }
        BEL_ST_AudioMixerShutdown();
    }
    g_refKeenCfg.oplEmulation=true;
    g_refKeenCfg.pcSpkVol=BE_AUDIO_VOL_MAX;
    for (int cycle=0; cycle<4; ++cycle) {
        const bool device = cycle%2;
        g_refKeenCfg.sndSubSystem=device;
        BE_ST_InitAudio();
        assert(g_sdlAudioInitDone && g_sdlAudioSubsystemUp==device);
        if (device) {
            auto id=SDL_GetAudioStreamDevice(g_sdlAudioStream);
            assert(id && SDL_PauseAudioDevice(id));
            assert(SDL_LockAudioStream(g_sdlAudioStream));
            assert(g_stAudioMixer.samplesPerPartTimesPITRate==UINT64_C(3145728000));
            const unsigned minSamples=3145728000u/PC_PIT_RATE;
            assert(g_stAudioMixer.samplesInCurrentPart>=minSamples &&
                   g_stAudioMixer.samplesInCurrentPart<=minSamples+1);
        }
        BE_ST_SndSample_T pcm[32]={};
        BEL_ST_AudioMixerCallback(pcm,16);
        assert(pcm[0]==0);
        BE_ST_PCSpeakerSetConstVal(true);
        BEL_ST_AudioMixerCallback(pcm,16);
        for (int block=0; block<256 && !pcm[0]; ++block)
            BEL_ST_AudioMixerCallback(pcm,16);
        assert(pcm[0]>0);

        if (device) {
            assert(SDL_UnlockAudioStream(g_sdlAudioStream));
            assert(SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(g_sdlAudioStream)));
        }
        BE_ST_ShutdownAudio();
        BE_ST_ShutdownAudio();
        assert(!g_stAudioMixer.numSources);
        assert(!g_sdlAudioInitDone && !g_sdlAudioSubsystemUp && !g_sdlAudioStream);
    }
    puts("PASS: silent fallback and dummy SDL stream, PIT, PCM, pause/resume and repeated shutdown");
}
