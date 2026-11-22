// 3DS near-wall projection test. No commercial assets or console required.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <initializer_list>
#include "../platform/3ds/original/cat3ds_native_wall_scaler.h"

static void reference(uint8_t *result, int halfHeight, int screenHeight) {
    std::memset(result, 255, 144);
    const int projected = halfHeight * 2;
    const int top = (screenHeight - projected) / 2;
    // Original scaler distributes each source texel into a half-open range.
    const int step = (projected << 16) / 64;
    for (int src=0; src<64; ++src) {
        const int begin = top + ((src * int64_t(step)) >> 16);
        const int end = top + (((src+1) * int64_t(step)) >> 16);
        for (int y=std::max(0, begin); y<std::min(screenHeight,end); ++y)
            result[y] = src;
    }
}

int main() {
    static_assert(CAT3DS_NEAR_WALL_SCALE_LIMIT == 4096, "bound extreme wall magnification");
    int checks = 0;
    for (int screenHeight : {120, 144}) {
        for (int halfHeight=133; halfHeight <= CAT3DS_NEAR_WALL_SCALE_LIMIT; ++halfHeight) {
            Cat3DS_WallRowMap rows{};
            Cat3DS_BuildTallWallRows(&rows, halfHeight, screenHeight);
            uint8_t expected[144];
            reference(expected, halfHeight, screenHeight);
            assert(rows.first == 0 && rows.end == screenHeight);
            for (int y=0; y<screenHeight; ++y) {
                if (rows.source[y] != expected[y]) {
                    std::fprintf(stderr, "FAIL screen=%d scale=%d y=%d got=%d expected=%d\n",
                       screenHeight,halfHeight,y,rows.source[y],expected[y]);
                    return 1;
                }
                ++checks;
            }
        }
    }
    // Real centered-camera case: a front wall at clearance 15/32 tile.
    // This produces a projected wall > Cat3D's MAXSCALE (132) even with
    // stereo off, which the old renderer flattened to scale 132.
    const double nearDepth = 15.0 / 32.0;
    const int fullScale = int(65536.0 / (2.0 * nearDepth * (65536.0 / 264.0)));
    assert(fullScale > 132 && fullScale < CAT3DS_NEAR_WALL_SCALE_LIMIT);
    // For a near wall, clipping should show a smaller texture interval
    // than the legacy MAXSCALE=160 cap: no larger-than-supported scale index.
    Cat3DS_WallRowMap large{}, capped{};
    Cat3DS_BuildTallWallRows(&large, 420, 120);
    Cat3DS_BuildTallWallRows(&capped, 160, 120);
    assert(large.source[0] > capped.source[0]);
    assert(large.source[119] < capped.source[119]);
    std::printf("PASS: %d near-wall row mappings (120/144 px), safe oversize projection\n", checks);
}
