#include <stdint.h>
inline uint64_t SDL_GetPerformanceCounter(){static uint64_t n; return __atomic_add_fetch(&n,1,__ATOMIC_RELAXED);}
inline uint64_t SDL_GetPerformanceFrequency(){return 1000;}
inline uint64_t SDL_GetTicks(){return SDL_GetPerformanceCounter();}
