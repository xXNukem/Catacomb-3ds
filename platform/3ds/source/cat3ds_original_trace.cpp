/* Catacomb 3DS - first step in integrating the original renderer.
 *
 * Derived from Catacomb 3-D c3_draw.c (BuildTables / TraceRay),
 * Copyright (C) 1993-2014 Flat Rock Software
 * Copyright (C) 2014-2026 NY00123
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This bounded 3DS adapter preserves the original's unusual camera model:
 * a point one tile behind the player, a prestep to the view plane, and
 * tile-by-tile ray progression with a binary search for diagonal crossings.
 * It does not render or replace the original FollowWalls/DrawWallList engine.
 */
#include "cat3ds_original_trace.h"

#include <math.h>
#include <stdint.h>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr int32_t kOne = 65536; // TILEGLOBAL / FOCALLENGTH from c3_def.h.
constexpr int kOriginalViewWidth = 264; // VIEWWIDTH from c3_def.h.
constexpr int kOriginalMinDist = (2*kOne)/5;

/* BuildTables() uses 16.16 integer scale and an integer-degree edge angle. */
int originalFirstAngle()
{
    int32_t scale = kOne / kOriginalViewWidth;
    scale = int32_t(int64_t(scale) * kOne / (kOne + kOriginalMinDist));
    const double angle = atan(double(kOriginalViewWidth / 2) * scale / kOne);
    return int(angle * 180.0 / kPi) + 1;
}

int32_t fixed(double v)
{
    return int32_t(v * double(kOne));
}

int tileCoord(int64_t coord)
{
    /* Portable floor division; negative fixed point values are invalid. */
    return coord < 0 ? -int((-coord + kOne - 1) / kOne) : int(coord / kOne);
}

uint16_t solidTile(const Cat3DSLevelMap &map, int x, int y)
{
    if (x < 0 || y < 0 || x >= int(map.width) || y >= int(map.height))
        return 0xFFFF; // out-of-map is an explicit hard stop
    const unsigned tile = map.walls[unsigned(y) * map.width + unsigned(x)];
    return tile && tile < 36u ? uint16_t(tile) : 0;
}

struct Hit {
    int x, y;
    uint16_t tile;
    unsigned steps;
};

bool trace(const Cat3DSLevelMap &map, int64_t viewX, int64_t viewY,
           double radians, int64_t prestep, Hit *hit)
{
    const int64_t dx = fixed(cos(radians));
    const int64_t dy = fixed(sin(radians)); // screen Y increases downward
    int64_t tx = viewX + (prestep * dx) / kOne;
    int64_t ty = viewY + (prestep * dy) / kOne;
    int x = tileCoord(tx), y = tileCoord(ty);
    if (solidTile(map, x, y)) return false; // original requires a free start

    /* Original TraceRay() advances one tile per step in a direction.
     * We retain its diagonal-crossing binary search with explicit limits.
     * Signed 16.16 arithmetic replaces DOS signed-magnitude trig tables. */
    for (unsigned step = 1; step <= 128; ++step)
    {
        const int prevX = x, prevY = y;
        tx += dx;
        ty += dy;
        x = tileCoord(tx);
        y = tileCoord(ty);
        if (x != prevX && y != prevY &&
            (solidTile(map, prevX, y) || solidTile(map, x, prevY)))
        {
            int64_t sx = dx, sy = dy;
            for (unsigned attempt = 0; attempt < 16; ++attempt)
            {
                sx /= 2; sy /= 2;
                if (x != prevX && y != prevY)
                { tx -= sx; ty -= sy; }
                else
                { tx += sx; ty += sy; }
                x = tileCoord(tx); y = tileCoord(ty);
                if ((x != prevX) != (y != prevY)) break;
            }
            /* A diagonal corner can be numerically ambiguous: choose a
             * blocking neighbor rather than dereference an invalid tile. */
            if (x == prevX && y == prevY)
            {
                if (solidTile(map, prevX + (dx >= 0 ? 1 : -1), prevY))
                    x = prevX + (dx >= 0 ? 1 : -1);
                else
                    y = prevY + (dy >= 0 ? 1 : -1);
            }
        }
        const uint16_t tile = solidTile(map, x, y);
        if (tile)
        {
            *hit = {x, y, tile, step};
            return tile != 0xFFFF;
        }
    }
    return false;
}
} // namespace

bool cat3dsTraceOriginalViewEdges(const Cat3DSLevelMap &map,
                                 const Cat3DSWalkPreview &camera,
                                 Cat3DSOriginalTrace *result)
{
    if (!result || !map.width || !map.height || map.width > 64 || map.height > 64 ||
        !isfinite(camera.x) || !isfinite(camera.y) || !isfinite(camera.angle) ||
        camera.x < 0.f || camera.y < 0.f ||
        camera.x >= float(map.width) || camera.y >= float(map.height))
        return false;
    const int first = originalFirstAngle();
    /* Screen-space preview uses +Y down and clockwise-positive angles.
     * Original c3_draw uses negated sin(viewangle) for Y movement, so
     * convert the angles at this explicit boundary. */
    const double a = camera.angle;
    const int64_t viewX = fixed(camera.x - cos(a));
    const int64_t viewY = fixed(camera.y - sin(a));
    const int64_t prestep = fixed(1.0 / cos(first * kPi / 180.0));
    Hit left = {}, right = {};
    if (!trace(map, viewX, viewY, a - first * kPi / 180.0, prestep, &left) ||
        !trace(map, viewX, viewY, a + first * kPi / 180.0, prestep, &right))
        return false;
    result->leftX = left.x;
    result->leftY = left.y;
    result->rightX = right.x;
    result->rightY = right.y;
    result->leftWall = left.tile;
    result->rightWall = right.tile;
    result->firstAngleDegrees = first;
    result->stepsLeft = left.steps;
    result->stepsRight = right.steps;
    return true;
}
