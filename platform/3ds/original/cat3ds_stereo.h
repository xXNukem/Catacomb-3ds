#ifndef CAT3DS_STEREO_H
#define CAT3DS_STEREO_H

#include <stdint.h>
#include <math.h>

static inline int Cat3DS_SpriteOccluded(int32_t wallDepth, int32_t spriteDepth)
{
    return wallDepth > 0 && wallDepth < spriteDepth;
}

static inline int Cat3DS_ActorVisible(const uint8_t *tiles, const uint8_t *visible,
    int stride, int width, int height, int x, int y)
{
    if (x < 0 || x >= width || y < 0 || y >= height) return 0;
    if (visible[x * stride + y]) return 1;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            const int nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < width && ny >= 0 && ny < height &&
                visible[nx * stride + ny] && !tiles[nx * stride + ny]) return 1;
        }
    }
    return 0;
}

/* Slider amplitude. Convert to a physical baseline for the active viewport. */
static inline int Cat3DS_EyeOffset(float slider)
{
    if (!(slider > 0.025f)) return 0;
    if (slider > 1.0f) slider = 1.0f;
    return (int)((slider - 0.025f) * (1280.0f / 0.975f) + 0.5f);
}

#define CAT3DS_SCREEN_TILES 1.5
/* At infinity the eye images differ by four of the 400 LCD pixels. */
static inline int Cat3DS_Baseline(int amplitude, int visibleColumns, double focal)
{
    const double lcdFocal = focal * 400.0 / visibleColumns;
    return (int)(amplitude / 1280.0 * 65536.0 * 4.0 * CAT3DS_SCREEN_TILES / (2.0 * lcdFocal));
}

static inline double Cat3DS_LensShift(double eyeTiles, double focalPixels)
{
    return eyeTiles * focalPixels / CAT3DS_SCREEN_TILES;
}

static inline int Cat3DS_LimitDepthOffset(int offset, double depth, int columns, double focal)
{
    if (depth >= CAT3DS_SCREEN_TILES) return offset;
    if (depth <= 0.0) return 0;
    const double lcdFocal = focal * 400.0 / columns;
    return (int)fmin(offset, 65536.0 * 2.1 / (2.0 * lcdFocal * (1.0 / depth - 1.0 / CAT3DS_SCREEN_TILES)));
}

/* Bound crossed disparity at a near corner using a conservative lower
   bound on optical depth. Both eyes use the same reduced baseline. */
static inline int Cat3DS_LimitCornerOffset(int offset, const uint8_t *tiles, int stride,
    int width, int height, double ox, double oy, double fx, double fy,
    int columns, double focal)
{
    if (!offset) return 0;
    const double radius = offset / 65536.0;
    const double slope = (columns * 0.5 + Cat3DS_LensShift(radius, focal)) / focal;
    const double rayLength = sqrt(1.0 + slope * slope);
    const int cx = (int)floor(ox), cy = (int)floor(oy);
    double nearest = CAT3DS_SCREEN_TILES;
    for (int x = cx - 2; x <= cx + 2; ++x) {
        for (int y = cy - 2; y <= cy + 2; ++y) {
            if (x < 0 || x >= width || y < 0 || y >= height ||
                (x == cx && y == cy) || !tiles[x * stride + y]) continue;
            const double dx = x + 0.5 - ox, dy = y + 0.5 - oy;
            const double extent = 0.5 * (fabs(fx) + fabs(fy));
            const double farDepth = dx * fx + dy * fy + extent;
            if (farDepth <= 0 || fabs(dy * fx - dx * fy) - extent > farDepth * slope + radius)
                continue;
            const double ax = fmax(0.0, fabs(dx) - 0.5);
            const double ay = fmax(0.0, fabs(dy) - 0.5);
            nearest = fmin(nearest, (sqrt(ax*ax + ay*ay) - radius) / rayLength);
        }
    }
    return Cat3DS_LimitDepthOffset(offset, nearest, columns, focal);
}

#ifdef __cplusplus
// A nearby sprite in open space also needs the crossed-disparity budget.
template<class Actor>
static int Cat3DS_LimitActorOffset(int offset, const Actor *player, double fx, double fy,
                                   int columns, double focal)
{
    if (!offset) return 0;
    for (const Actor *actor = player->next; actor; actor = actor->next) {
        if (!actor->state || !actor->state->shapenum) continue;
        const double dx = (actor->x - player->x) / 65536.0;
        const double dy = (actor->y - player->y) / 65536.0;
        const double z = dx*fx + dy*fy;
        if (z <= 0 || z >= CAT3DS_SCREEN_TILES) continue;
        if (fabs(dy*fx - dx*fy) <= z*columns/(2*focal) + actor->size/65536.0 + 0.5)
            offset = Cat3DS_LimitDepthOffset(offset,z,columns,focal);
    }
    return offset;
}
#endif

static inline int Cat3DS_WallTextureColumn(double worldHit, int hitSide,
                                    int stepX, int stepY)
{
    const double texel = (worldHit - floor(worldHit)) * 64.0;
    int column = (int)floor(texel);
    if (column < 0) column = 0;
    if (column > 63) column = 63;
    if ((hitSide && stepX < 0) || (!hitSide && stepY > 0))
        column = 63 - column;
    return column;
}

typedef struct {
    int tile, side, column;
    double depth;
} Cat3DSWallHit;

static inline Cat3DSWallHit Cat3DS_TraceColumn(const uint8_t *tiles, uint8_t *visible,
    int stride, int mapwidth, int mapheight, double originX, double originY,
    double dx, double dy, double nearPlane)
{
    const int maxSteps = mapwidth + mapheight + 2;
    double startDepth = nearPlane + 0.00001;
    double startX = originX + dx * startDepth;
    double startY = originY + dy * startDepth;
    int tx = (int)floor(startX);
    int ty = (int)floor(startY);
    /* A corner may intersect the near plane before its sampled cell.
       Recover the entry face along this ray when the focal cell is open. */
    if (tx < 0 || ty < 0 || tx >= mapwidth || ty >= mapheight || tiles[tx * stride + ty]) {
        const int ox = (int)floor(originX), oy = (int)floor(originY);
        if (ox >= 0 && oy >= 0 && ox < mapwidth && oy < mapheight && !tiles[ox * stride + oy]) {
            startDepth = 0.0;
            startX = originX;
            startY = originY;
            tx = ox;
            ty = oy;
        }
    }
    const int stepX = dx < 0.0 ? -1 : 1;
    const int stepY = dy < 0.0 ? -1 : 1;
    const double deltaX = dx != 0.0 ? 1.0 / fabs(dx) : 1.0e30;
    const double deltaY = dy != 0.0 ? 1.0 / fabs(dy) : 1.0e30;
    double nextX = dx > 0.0 ? ((double)(tx + 1) - startX) * deltaX
                   : dx < 0.0 ? (startX - (double)tx) * deltaX : 1.0e30;
    double nextY = dy > 0.0 ? ((double)(ty + 1) - startY) * deltaY
                   : dy < 0.0 ? (startY - (double)ty) * deltaY : 1.0e30;
    int hitSide = -1;
    double hitDepth = 0.0;
    int color = 0;
    /* If the virtual focal point is in masonry as well, the near plane
       cuts the solid. Use its first forward exit face. No entry face is
       visible from inside, and this column must still hide actors. */
    const int inside = tx >= 0 && tx < mapwidth && ty >= 0 && ty < mapheight &&
                       tiles[tx * stride + ty];
    for (int i = 0; i < maxSteps; ++i) {
        if (tx < 0 || ty < 0 || tx >= mapwidth || ty >= mapheight)
            break;
        const int cell = tiles[tx * stride + ty];
        if (inside) {
            if (!cell) break;
            color = cell;
        } else {
            if (cell) {
                color = cell;
                break;
            }
            if (visible) visible[tx * stride + ty] = 1;
        }
        if (nextX < nextY) {
            hitDepth = startDepth + nextX;
            nextX += deltaX;
            tx += stepX;
            hitSide = 1; /* Constant X: light face. */
        } else {
            hitDepth = startDepth + nextY;
            nextY += deltaY;
            ty += stepY;
            hitSide = 0; /* Constant Y: dark face. */
        }
    }
    Cat3DSWallHit result = {0, 0, 0, 0.0};
    if (color && hitDepth > 0.0) {
        result.tile = color;
        result.side = hitSide;
        result.depth = hitDepth;
        const double along = hitSide ? originY + hitDepth * dy : originX + hitDepth * dx;
        result.column = Cat3DS_WallTextureColumn(along, hitSide, inside ? -stepX : stepX, inside ? -stepY : stepY);
    }
    return result;
}

/* CAT3DS_VIEW_CLEARANCE_V1
 * The DOS raycaster's focal point was one tile behind the player. The 3DS
 * DDA camera instead started at the player's feet, magnifying the wall as
 * the player approached it. Retreat ONLY when a wall is too close in the
 * viewing direction, and never retreat through a wall behind the player.
 * Result in 16.16 tile units. Gameplay/player coordinates are unchanged.
 */
static inline int32_t Cat3DS_ViewClearanceRetreat(
    const uint8_t *tiles, int stride, int width, int height,
    int32_t playerX, int32_t playerY, double forwardX, double forwardY)
{
    const double px = (double)playerX / 65536.0;
    const double py = (double)playerY / 65536.0;
    const int cx = (int)floor(px), cy = (int)floor(py);
    if (cx < 0 || cy < 0 || cx >= width || cy >= height ||
        tiles[cx * stride + cy]) return 0;

    const Cat3DSWallHit ahead = Cat3DS_TraceColumn(tiles, 0, stride, width,
        height, px, py, forwardX, forwardY, 0.0);
    /* At this depth the 3DS wall projection fits below the original
     * maximum scaler size, rather than appearing enormously magnified. */
    const double desiredDepth = 1.125;
    if (!ahead.tile || ahead.depth >= desiredDepth) return 0;

    const Cat3DSWallHit behind = Cat3DS_TraceColumn(tiles, 0, stride, width,
        height, px, py, -forwardX, -forwardY, 0.0);
    const double roomBehind = behind.tile ? fmax(0.0, behind.depth - 0.0625) : 1.0;
    const double retreat = fmax(0.0, fmin(1.0,
        fmin(desiredDepth - ahead.depth, roomBehind)));
    return (int32_t)(retreat * 65536.0);
}
#endif
