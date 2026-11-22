#pragma once
#include <stdint.h>

// Coordinates use the engine's 16.16 tile units. Radius 15/32 leaves
// a 1/32-tile lateral allowance on each side in a one-tile passage.
// Apply this only to ordinary solid walls, not interactive special tiles.
constexpr int32_t CAT3DS_WALL_RADIUS = 15 * 65536 / 32;
template<class Solid>
static int32_t Cat3DS_WallStep(int32_t pos, int32_t cross, int32_t step, Solid solid)
{
    if (!step) return 0;
    constexpr int tile = 65536, radius = CAT3DS_WALL_RADIUS;
    const int sign = step > 0 ? 1 : -1;
    const int edge = pos + sign*radius;
    for (int col = edge/tile; col != (edge+step)/tile + sign; col += sign) {
        for (int row=(cross-radius)/tile; row<=(cross+radius)/tile; ++row) {
            if (!solid(col,row)) continue;
            const int gap = sign > 0 ? col*tile-edge-1 : (col+1)*tile-edge;
            return sign*gap > 0 ? gap : 0;
        }
    }
    return step;
}
