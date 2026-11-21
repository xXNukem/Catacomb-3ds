#pragma once
#include <stdint.h>
typedef uint16_t u16; typedef uint32_t u32; 
#include <condition_variable>
#include <mutex>
#include <thread>
struct LightEvent {
    std::mutex mutex;
    std::condition_variable ready;
    bool signaled;
};
typedef std::thread* Thread;
enum {GFX_TOP, GFX_BOTTOM, GFX_LEFT, GFX_RIGHT, GSP_RGB565_OES, GSP_RGBA8_OES, RESET_ONESHOT};
#define R_FAILED(x) ((x)<0)
#define R_SUCCEEDED(x) ((x)>=0)
#define U64_MAX UINT64_MAX
static uint16_t top[400*240], bot[320*240], rightEye[400*240]; static bool hwStereo;
inline void gfxSet3D(bool x){hwStereo=x;}
static float slider=1;
inline float osGet3DSliderState(){return slider;}
inline void gfxSetScreenFormat(int,int){}
inline void* gfxGetFramebuffer(int s,int eye,u16*w,u16*h){*w=240; *h=s==GFX_TOP?400:320; return s==GFX_TOP?(eye==GFX_LEFT?top:rightEye):bot;}
inline void GSPGPU_FlushDataCache(void*,u32){}
inline void gfxScreenSwapBuffers(int,bool){}
static int availableCore;
inline int svcGetProcessorID() { return availableCore; }
inline void LightEvent_Wait(LightEvent *event) {
    std::unique_lock<std::mutex> lock(event->mutex);
    event->ready.wait(lock, [event]{return event->signaled;});
    event->signaled=false;
}
inline void LightEvent_Signal(LightEvent *event) {
    std::lock_guard<std::mutex> lock(event->mutex);
    event->signaled=true;
    event->ready.notify_one();
}
inline void LightEvent_Init(LightEvent *event,int) { event->signaled=false; }
inline void threadJoin(Thread thread,uint64_t) { thread->join(); }
inline void threadFree(Thread thread) { delete thread; }
inline Thread threadCreate(void(*entry)(void*),void *arg,int,int,int core,bool) {
    return core==availableCore ? new std::thread(entry,arg) : nullptr;
}
static u32 quota=15;
inline int APT_GetAppCpuTimeLimit(u32 *out){*out=quota;return 0;}
inline int APT_SetAppCpuTimeLimit(int n){quota=n;return 0;}
inline int APT_CheckNew3DS(bool*v){*v=true;return 0;}
inline void osSetSpeedupEnable(bool){}
