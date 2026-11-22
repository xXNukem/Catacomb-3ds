#include "../platform/3ds/original/cat3ds_stereo.h"
#include <assert.h>

int main(void)
{
    uint8_t tiles[8][8] = {{0}}, visible[8][8] = {{0}};
    for (int y = 0; y < 8; ++y) tiles[6][y] = 3;
    tiles[3][4] = 2;
    const double focal = 264.0;
    int previous = 0;
    for (int i = 0; i <= 1000; ++i) {
        int n = Cat3DS_EyeOffset(i / 1000.0f);
        assert(n >= previous && n <= 1280);
        previous = n;
        double eye = Cat3DS_Baseline(n,264,focal) / 65536.0, shift = Cat3DS_LensShift(eye, focal);
        for (int d = 0; d < 3; ++d) {
            const double z = (double[]){1, CAT3DS_SCREEN_TILES, 8}[d];
            const double right = shift - focal * eye / z;
            if (d == 0) assert(right <= 0 && right * 2 * 400 / 264 > -2.1);
            if (d == 1) assert(fabs(right) < 1e-12);
            if (d == 2) assert(right >= 0 && right * 2 * 400 / 264 < 4.0);
        }
    }
    assert(Cat3DS_SpriteOccluded(65536,65537));
    assert(!Cat3DS_SpriteOccluded(65536,65535));
    assert(!Cat3DS_SpriteOccluded(0,65537));
    uint8_t solid[3][3] = {{0},{2,2,2},{0}};
    Cat3DSWallHit clipped = Cat3DS_TraceColumn(solid[0], 0, 3,3,3,1.5,1.25,0,1,1);
    assert(clipped.tile == 2 && clipped.depth == 1.75);
    assert(previous == 1280 && Cat3DS_EyeOffset(0) == 0);
    const int limited = Cat3DS_LimitCornerOffset(previous, tiles[0], 8, 8, 8,
        2.875,4,1,0,264,focal);
    assert(limited > 0 && limited < previous);
    assert(2 * limited / 65536.0 * focal * 400 / 264 * (1/0.125 - 1.0 / CAT3DS_SCREEN_TILES) <= 2.1);
    assert(!Cat3DS_ActorVisible(tiles[0],visible[0],8,8,8,65535,2));
    assert(!Cat3DS_ActorVisible(tiles[0],visible[0],8,8,8,0,0));
    for (int eye = -1; eye <= 1; eye += 2) {
        const double offset = eye * previous / 65536.0;
        Cat3DSWallHit hit = Cat3DS_TraceColumn(tiles[0],visible[0],8,8,8,
            2.875,4+offset,1,-Cat3DS_LensShift(offset,focal)/focal,1);
        assert(hit.tile == (eye > 0 ? 2 : 3));
        assert(hit.depth == (eye > 0 ? 0.125 : 3.125));
        assert(hit.side && hit.column >= 0 && hit.column < 64);
    }
}
