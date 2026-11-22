#ifndef CAT3DS_LAYOUT_H
#define CAT3DS_LAYOUT_H

#include <stdint.h>
#include <string.h>

typedef struct {
    int sourceWidth, sourceHeight;
    int outputWidth, outputHeight;
} Cat3DSLayout;

static inline Cat3DSLayout Cat3DS_GetLayout(int adventure, int modal, int egaWidth)
{
    /* DOS 320x200 fills a 4:3 display (pixel aspect 5:6).
       The central 240x120 aperture therefore fills the 400x240 LCD. */
    if (adventure && modal) return (Cat3DSLayout){egaWidth, 200, 320, 240};
    if (adventure) return (Cat3DSLayout){240, 120, 400, 240};
    return (Cat3DSLayout){264, 144, 400, 240};
}
/* libctru stores LCD columns contiguously, bottom pixel first. */
static inline void Cat3DS_BlitTop(Cat3DSLayout layout, const uint16_t *topScaleX,
                                  const uint16_t *topScaleY, const uint8_t *egaPixels,
                                  int pitch, const uint16_t *palette, uint16_t *fb,
                                  unsigned stride)
{
    const int left = (400 - layout.outputWidth) / 2;
    const int border = (240 - layout.outputHeight) / 2;
    for (int x = 0; x < 400; ++x) {
        uint16_t *col = fb + (size_t)x * stride;
        if (x < left || x >= left + layout.outputWidth) {
            memset(col + stride - 240, 0, 240 * sizeof(*col));
            continue;
        }
        if (border) {
            memset(col + stride - 240, 0, border * sizeof(*col));
            memset(col + stride - border, 0, border * sizeof(*col));
        }
        const int sampleX = topScaleX[x - left];
        for (int y = 0; y < layout.outputHeight; ++y)
            col[stride - border - y - 1] = palette[egaPixels[(size_t)topScaleY[y] * pitch + sampleX]];
    }
}
#endif
