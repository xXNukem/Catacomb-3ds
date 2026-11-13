/* Host regression: native wall scaler against ReflectionHLE's BuildCompScale
 * row instructions and per-pixel EGA byte-mask semantics. No game assets. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../platform/3ds/original/cat3ds_native_wall_scaler.h"

#define HEIGHT 144
#define SCALECOUNT 132
#define PAGESIZE 65536

static uint64_t repeat[256], masks[256];
static uint32_t seed = 0x3acac034;
static uint32_t rnd(void) { seed ^= seed<<13; seed ^= seed>>17; seed ^= seed<<5; return seed; }
static void reference(uint64_t *ega, unsigned off, unsigned stride, const uint8_t *tex,
                      uint8_t loMask, uint8_t hiMask, int height)
{
    /* Original BuildCompScale: source 64 pixels, step (height<<16)/64.
     * Each generated 'AND [ES:DI+heightofs],AL' is applied one at a time. */
    int32_t fix = 0, step = ((int32_t)height << 16)/64;
    const int top = (HEIGHT - height)/2;
    for (int src=0;src<64;++src) {
        int begin = (uint32_t)fix >> 16;
        fix += step;
        int end = (uint32_t)fix >> 16;
        begin += top;
        end += top;
        if (begin >= HEIGHT || end < 0 || begin == end) continue;
        for (int y = begin; y < end; ++y) {
            if (y < 0 || y >= HEIGHT) continue;
            uint64_t color = repeat[tex[src] & 15];
            if (loMask) {
                uint64_t m = masks[loMask];
                ega[off + y*stride] = (ega[off + y*stride] & ~m) | (color & m);
            }
            if (hiMask) {
                uint64_t m = masks[hiMask];
                ega[off + y*stride + 1] = (ega[off + y*stride + 1] & ~m) | (color & m);
            }
        }
    }
}

int main(void)
{
    for (int c=0;c<256;c++) {
        for (int p=0;p<8;p++) {
            repeat[c] |= (uint64_t)c << (p*8);
            if (c&(1<<(7-p))) masks[c] |= (uint64_t)0xff << (p*8);
        }
    }
    uint64_t *baseline = malloc(sizeof(uint64_t)*PAGESIZE);
    uint64_t *native = malloc(sizeof(uint64_t)*PAGESIZE);
    if (!baseline || !native) return 2;
    int total=0;
    for (int scale=1;scale<=SCALECOUNT;scale++) {
        const int height = scale*2;
        const int top = (HEIGHT-height)/2;
        uint16_t start[65], width[65];
        int32_t fix = 0;
        const int32_t step = ((int32_t)height << 16)/64;
        for (int i=0;i<=64;i++) {
            int begin=(uint32_t)fix>>16;
            fix+=step;
            int end=(uint32_t)fix>>16;
            start[i]=begin;
            width[i]=end>begin?end-begin:0;
        }
        Cat3DS_WallRowMap rows;
        Cat3DS_BuildWallRows(&rows,start,width,height,HEIGHT);
        /* Test all clipping regimes: tiny walls, full height, oversized
         * projected walls, and both single- and double-EGA-byte masks. */
        for (int i=0;i<70;i++) {
            uint8_t tex[64];
            for (int y=0;y<64;y++) tex[y]=(uint8_t)rnd();
            uint8_t maskLo=(uint8_t)rnd(),maskHi=(uint8_t)rnd();
            if (i==0) maskLo=0;
            if (i==1) maskHi=0;
            if (i==2) maskLo=maskHi=0xff;
            if (i==3) maskLo=maskHi=0;
            for (int j=0;j<PAGESIZE;j++) baseline[j]=((uint64_t)rnd()<<32)|rnd();
            memcpy(native,baseline,sizeof(uint64_t)*PAGESIZE);
            const unsigned off=0x900+18;
            reference(baseline,off,40,tex,maskLo,maskHi,height);
            Cat3DS_DrawWallRows(&rows,native,off,40,tex,maskLo,maskHi,repeat,masks);
            if (memcmp(native,baseline,sizeof(uint64_t)*PAGESIZE)) {
                fprintf(stderr,"FAIL scale=%d height=%d i=%d top=%d\n",scale,height,i,top);
                return 1;
            }
            ++total;
        }
    }
    printf("PASS: %d complete bit-for-bit EGA framebuffer comparisons, including oversize walls and clipping.\n",total);
    free(baseline); free(native);
    return 0;
}
