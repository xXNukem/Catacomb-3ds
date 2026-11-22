#include "../platform/3ds/original/cat3ds_layout.h"
#include <assert.h>
#include <stdio.h>

static void check(int abyss, int modal, int w, int h, int outW, int outH)
{
    enum { pitch = 648, stride = 245 };
    static uint8_t ega[200 * pitch];
    static uint16_t lcd[400 * stride];
    uint16_t xs[400], ys[240], palette[256];
    Cat3DSLayout layout = Cat3DS_GetLayout(abyss, modal, w);
    assert(layout.sourceWidth == w && layout.sourceHeight == h);
    assert(layout.outputWidth == outW && layout.outputHeight == outH);
    for (int i = 0; i < 256; ++i) palette[i] = i + 1;
    for (int i = 0; i < 200 * pitch; ++i) ega[i] = i % 251;
    for (int x = 0; x < outW; ++x) xs[x] = x * w / outW;
    for (int y = 0; y < outH; ++y) ys[y] = y * h / outH;
    memset(lcd, 0xff, sizeof lcd);
    Cat3DS_BlitTop(layout, xs, ys, ega, pitch, palette, lcd, stride);
    for (int x = 0; x < 400; ++x) {
        for (int y = 0; y < stride; ++y) {
            int dx = x - (400 - outW) / 2, dy = y - (240 - outH) / 2;
            unsigned expected = y >= 240 ? 0xffff : 0;
            if (dx >= 0 && dx < outW && dy >= 0 && dy < outH)
                expected = palette[ega[(dy * h / outH) * pitch + dx * w / outW]];
            assert(lcd[x * stride + stride - 1 - y] == expected);
        }
    }
}

int main(void)
{
    check(0, 0, 264, 144, 400, 240);
    check(1, 0, 240, 120, 400, 240);
    check(1, 1, 640, 200, 320, 240);
    check(1, 1, 320, 200, 320, 240);
    check(1, 0, 240, 120, 400, 240);
    puts("PASS: LCD layout, sampling, borders and stride padding");
}
