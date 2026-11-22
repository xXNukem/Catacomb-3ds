// Host regression against the production 3DS DDA camera placement helper.
// No DOS data, SDL, devkitPro or console are needed.
#include "../platform/3ds/original/cat3ds_stereo.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

static uint8_t tiles[64 * 64];
static void wall(int x, int y) { tiles[x * 64 + y] = 1; }
static void reset() { memset(tiles, 0, sizeof tiles); }
static int32_t fix(double v) { return static_cast<int32_t>(v * 65536.0); }
static double real(int32_t v) { return static_cast<double>(v) / 65536.0; }
static double visibleDepth(double x, double y, double dx, double dy)
{
    return Cat3DS_TraceColumn(tiles, nullptr, 64, 64, 64, x, y,
                              dx, dy, 0.0).depth;
}

int main()
{
    // Standing in an ordinary long corridor must not move the camera.
    reset();
    for (int x = 0; x <= 20; ++x) { wall(x, 0); wall(x, 2); }
    for (int y = 0; y <= 2; ++y) { wall(0, y); wall(20, y); }
    for (int step = 0; step <= 22; ++step) {
        double x = 3.0 + step * 0.75;
        int32_t back = Cat3DS_ViewClearanceRetreat(tiles,64,64,64,fix(x),fix(1.5),1.0,0.0);
        if (x <= 18.75) assert(back == 0);
        else {
            double eyeX = x - real(back);
            assert(visibleDepth(eyeX, 1.5, 1, 0) >= 1.1249);
        }
    }

    // The problem case: player collision permits approaching to ~0.47 tile;
    // the optical camera must stay >= 1.125 tiles away from the front wall.
    reset();
    wall(8, 7);
    const double playerX = 8.0 - 15.0 / 32.0;
    const int32_t offset = Cat3DS_ViewClearanceRetreat(tiles,64,64,64,
                                                         fix(playerX), fix(7.5), 1, 0);
    assert(offset > fix(0.6) && offset < fix(0.7));
    assert(std::fabs(visibleDepth(playerX - real(offset), 7.5, 1, 0)-1.125) < 0.0001);

    // Tight room: put the camera as far back as is safe, but NEVER through
    // the rear wall. Ordinary physical passage and pickup reach stay intact.
    reset();
    for (int i = 0; i <= 2; ++i) {
        wall(i,0); wall(i,2); wall(0,i); wall(2,i);
    }
    int32_t boxOffset = Cat3DS_ViewClearanceRetreat(tiles,64,64,64,
                                                     fix(1.5), fix(1.5), 1, 0);
    const double eyeX = 1.5 - real(boxOffset);
    assert(std::fabs(real(boxOffset)-0.4375)<0.0001);
    assert(eyeX >= 1.0624 && eyeX < 1.0630);
    assert(visibleDepth(eyeX, 1.5, -1, 0) >= 0.0624);
    assert(visibleDepth(eyeX, 1.5, 1, 0) >= 0.9374);

    // A side wall close to the player must NOT move a forward-facing camera.
    reset();
    for (int x = 0; x <= 20; ++x) { wall(x,0); wall(x,2); }
    assert(Cat3DS_ViewClearanceRetreat(tiles,64,64,64,
                                      fix(5.5),fix(1.5),1,0) == 0);

    // Looking diagonally at a corner is safe; the camera remains in an
    // empty tile, regardless of which wall face was hit first.
    reset();
    for (int i=0;i<=5;++i) {
        wall(i,0);wall(i,5);wall(0,i);wall(5,i);
    }
    wall(3,3);
    const double diagonal = std::sqrt(0.5);
    int32_t diagonalOffset = Cat3DS_ViewClearanceRetreat(tiles,64,64,64,
                                                         fix(2.5), fix(2.5), diagonal, diagonal);
    double x = 2.5 - real(diagonalOffset)*diagonal;
    double y = 2.5 - real(diagonalOffset)*diagonal;
    assert(!tiles[(int)x*64+(int)y]);
    assert(diagonalOffset >= 0 && diagonalOffset <= 65536);

    // Unexpected position inside solid geometry must be left untouched.
    assert(Cat3DS_ViewClearanceRetreat(tiles,64,64,64,
                                      fix(3.5),fix(3.5),1,0) == 0);
    std::puts("PASS: camera stays clear of nearby walls, retains open-space POV, and never moves player physics.");
}
