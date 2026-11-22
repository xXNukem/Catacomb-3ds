// Exercise the compiled-in driver's close function with counted libctru calls.
#include <cassert>
#include <cstdlib>
#include <SDL3/SDL.h>
enum { NUM_BUFFERS=3, NDSP_FORMAT_STEREO_PCM8, NDSP_FORMAT_MONO_PCM8,
       NDSP_FORMAT_STEREO_PCM16, NDSP_FORMAT_MONO_PCM16, NDSP_INTERP_LINEAR,
       NDSP_WBUF_FREE=0 };
using Result = int;
#define R_FAILED(x) ((x)<0)
#define R_SUMMARY(x) (x)
#define R_MODULE(x) (x)
enum { RS_NOTFOUND=-1, RM_DSP=-1 };
struct ndspWaveBuf { void *data_vaddr; unsigned nsamples, status; };
struct SDL_PrivateAudioData {
    ndspWaveBuf waveBuf[3]; Uint8 *mixbuf; bool ndspInitialized, isCancelled;
    int cv, lock; unsigned nextbuf;
};
using Hidden = SDL_PrivateAudioData;
struct SDL_AudioDevice { Hidden *hidden; SDL_AudioSpec spec; unsigned buffer_size; Uint8 silence_value; };
static SDL_AudioDevice *audio_device;
static int dsp_hook, hooks, callbacks, resets, serviceRefs, linearBlocks;
static bool locked;
void contextLock(SDL_AudioDevice* d) { assert(d->hidden->ndspInitialized && !locked); locked=true; }
void contextUnlock(SDL_AudioDevice*) { assert(locked); locked=false; }
void dspUnhook(int*) { assert(hooks-- == 1); }
void AudioFrameFinished(void*) {}
void ndspSetCallback(void (*cb)(void*),void*) {
    if (cb) assert(callbacks++ == 0);
    else assert(callbacks-- == 1);
}
void N3DSAUD_DspHook(int) {}
void dspHook(int*,void(*)(int)) { assert(hooks++==0); }
int ndspInit() { assert(serviceRefs++==0); return 0; }
void LightLock_Init(int*) {}
void CondVar_Init(int*) {}
const SDL_AudioFormat *SDL_ClosestAudioFormats(SDL_AudioFormat) {
    static const SDL_AudioFormat formats[]={SDL_AUDIO_S16,SDL_AUDIO_UNKNOWN};
    return formats;
}
void SDL_UpdatedAudioDeviceFormat(SDL_AudioDevice *d) { d->buffer_size=32*SDL_AUDIO_FRAMESIZE(d->spec); }
void *linearAlloc(unsigned n) { assert(linearBlocks++==0); return malloc(n); }
void DSP_FlushDataCache(void*,unsigned) {}
void ndspChnSetInterp(int,int) {}
void ndspChnSetRate(int,int) {}
void ndspChnSetFormat(int,unsigned) {}
void ndspChnSetMix(int,float*) {}
void ndspChnWaveBufAdd(int,ndspWaveBuf *b) { assert(!b->status); b->status=1; }
void ndspChnReset(int) { assert(locked || !audio_device); ++resets; }
void ndspExit() { assert(!locked && serviceRefs-- == 1); }
void linearFree(void *p) { assert(!serviceRefs && linearBlocks-- == 1); free(p); }
#define SDL_calloc calloc
#define SDL_malloc malloc
#define SDL_free free
#include "n3ds_audio_close.inc"
int main() {
    for (int round=0;round<64;++round) {
        for (int stage=0;stage<=4;++stage) {
            SDL_AudioDevice d = {};
            if (stage) d.hidden = (Hidden*)calloc(1,sizeof(Hidden));
            if (stage>=2) { d.hidden->ndspInitialized=true; ++serviceRefs; }
            if (stage>=3) {
                audio_device=&d; ++hooks; ++callbacks;
                d.hidden->mixbuf=(Uint8*)malloc(128);
                d.hidden->waveBuf[0].data_vaddr=malloc(384); ++linearBlocks;
            }
            if (stage==4) d.hidden->isCancelled=true;
            N3DSAUDIO_CloseDevice(&d);
            N3DSAUDIO_CloseDevice(&d);
            assert(!audio_device && !d.hidden && !hooks && !callbacks && !serviceRefs && !linearBlocks);
        }
    }
    assert(resets==64);
    SDL_AudioDevice device = {};
    device.spec = {SDL_AUDIO_S16, 2, 48000};
    for (int session=1;session<=4;++session) {
        assert(N3DSAUDIO_OpenDevice(&device) && audio_device==&device);
        for (unsigned i=0;i<NUM_BUFFERS;++i) {
            auto &wave=device.hidden->waveBuf[i];
            assert(wave.nsamples==32 && device.hidden->nextbuf==i);
            for (unsigned k=0;k<device.buffer_size;++k)
                assert(((Uint8*)wave.data_vaddr)[k]==0);
            SDL_memset(device.hidden->mixbuf,session+i,device.buffer_size);
            assert(N3DSAUDIO_PlayDevice(&device,device.hidden->mixbuf,device.buffer_size));
            assert(wave.status && !SDL_memcmp(wave.data_vaddr,device.hidden->mixbuf,device.buffer_size));
        }
        N3DSAUDIO_CloseDevice(&device);
        assert(!audio_device && !device.hidden && !hooks && !callbacks && !serviceRefs && !linearBlocks);
    }
}
