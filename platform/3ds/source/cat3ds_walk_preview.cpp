/* Catacomb 3DS - FIRST-PERSON MAP TEST, not original engine gameplay.
 * - Reads the actual Catacomb 3-D DOS map planes, unchanged.
 * - Uses elementary DDA raycasting and approximate solid-wall collision.
 * - Uses the existing native EGA backend (no SDL / asset copies).
 * - Uses original DOS wall pictures if a validated texture set is provided.
 * - No actors, interactive doors, sound, or actual game simulation.
 */
#include "cat3ds_walk_preview.h"

#include <math.h>
#include <stdint.h>

extern "C" {
void BE_ST_EGAUpdateGFXBufferFrom4bitsPixel(uint16_t destOff, uint8_t color, uint16_t count);
void BE_ST_EGAUpdateGFXBitsFrom4bitsPixel(uint16_t destOff, uint8_t color, uint8_t bitsMask);
}

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr unsigned kWidth = 320, kHeight = 200;
// Based on the original Catacomb 3-D BuildTables() horizontal view angle:
// atan((VIEWWIDTH / 2) / (VIEWWIDTH * (1 + MINDIST / FOCALLENGTH)))
// = atan(1 / 2.8) ~= 19.65 degrees (39.3 degrees full horizontal FOV).
constexpr float kCameraPlane = 1.f / 2.8f;
// Approximate original wall scale, expanded from its 264 px-wide viewport
// to this preview's full 320 px. Original projection includes a 1-tile
// focallength behind the player's viewpoint; this is a visual approximation,
// NOT the engine's exact wall tracer or camera geometry.
constexpr float kWallScale = float(kWidth) * 1.4f;

// From the original SetupGameLevel(): tile != 0 && tile < NUMFLOORS (36)
// is considered a wall for this early preview. Special tiles are not simulated.
bool blocked(const Cat3DSLevelMap &map, int x, int y)
{
    if (x < 0 || y < 0 || x >= int(map.width) || y >= int(map.height))
        return true;
    const unsigned tile = map.walls[y * map.width + x];
    return tile > 0 && tile < 36;
}

bool freeForPlayer(const Cat3DSLevelMap &map, float x, float y)
{
    constexpr float r = .18f;
    return !blocked(map, int(floorf(x-r)), int(floorf(y-r))) &&
           !blocked(map, int(floorf(x+r)), int(floorf(y-r))) &&
           !blocked(map, int(floorf(x-r)), int(floorf(y+r))) &&
           !blocked(map, int(floorf(x+r)), int(floorf(y+r)));
}

float clampAxis(float v)
{
    return v > 1.f ? 1.f : (v < -1.f ? -1.f : v);
}
}

bool cat3dsStartWalkPreview(const Cat3DSLevelMap &map, Cat3DSWalkPreview *state)
{
    if (!state || !map.width || !map.height || map.width > 64 || map.height > 64)
        return false;
    for (unsigned y = 0; y < map.height; ++y)
        for (unsigned x = 0; x < map.width; ++x)
        {
            const unsigned object = map.objects[y * map.width + x];
            if (object >= 1 && object <= 4)
            {
                const float facing[4] = {-kPi/2.f, 0.f, kPi/2.f, kPi};
                state->x = float(x) + .5f;
                state->y = float(y) + .5f;
                state->angle = facing[object - 1];
                state->spawnX = x;
                state->spawnY = y;
                state->level = map.index;
                return freeForPlayer(map, state->x, state->y);
            }
        }
    return false;
}

bool cat3dsUpdateWalkPreview(const Cat3DSLevelMap &map, Cat3DSWalkPreview *state,
                            float forward, float strafe, float turn, float seconds)
{
    if (!state || seconds <= 0.f) return false;
    if (seconds > .05f) seconds = .05f; // Avoid huge moves after suspend.
    turn = clampAxis(turn);
    const float oldAngle = state->angle;
    state->angle += turn * 2.1f * seconds;
    if (state->angle > kPi) state->angle -= 2.f*kPi;
    if (state->angle < -kPi) state->angle += 2.f*kPi;
    forward = clampAxis(forward);
    strafe = clampAxis(strafe);
    const float norm = (forward && strafe) ? .70710678f : 1.f;
    const float ca = cosf(state->angle), sa = sinf(state->angle);
    const float dx = (ca * forward - sa * strafe) * norm * 2.4f * seconds;
    const float dy = (sa * forward + ca * strafe) * norm * 2.4f * seconds;
    const float px = state->x, py = state->y;
    if (freeForPlayer(map, px + dx, py)) state->x += dx;
    if (freeForPlayer(map, state->x, py + dy)) state->y += dy;
    return state->x != px || state->y != py || state->angle != oldAngle;
}

void cat3dsRenderWalkPreview(const Cat3DSLevelMap &map, const Cat3DSWalkPreview &state,
                                const Cat3DSWallTextures *textures, bool highQuality)
{
    // Exact flat EGA background from Catacomb 3-D c3_draw.c ClearScreen():
    // black ceiling (0), dark-grey floor (8). No procedural floor textures.
    for (unsigned y = 0; y < kHeight; ++y)
        BE_ST_EGAUpdateGFXBufferFrom4bitsPixel(uint16_t(y * 40),
                                               y < kHeight / 2 ? 0 : 8, 40);

    const float ca = cosf(state.angle), sa = sinf(state.angle);
    // Render at native 320 columns by default. Optional 160-column fallback
    // draws 2-pixel-wide columns for slower hardware.
    const unsigned rays = highQuality ? kWidth : kWidth / 2;
    for (unsigned ray = 0; ray < rays; ++ray)
    {
        const float cameraX = (2.f * (float(ray) + .5f) / float(rays)) - 1.f;
        const float dirX = ca - sa * kCameraPlane * cameraX;
        const float dirY = sa + ca * kCameraPlane * cameraX;
        const float ddx = fabsf(dirX) > 1.e-6f ? fabsf(1.f/dirX) : 1.e30f;
        const float ddy = fabsf(dirY) > 1.e-6f ? fabsf(1.f/dirY) : 1.e30f;
        int mx = int(floorf(state.x)), my = int(floorf(state.y));
        const int stepX = dirX < 0.f ? -1 : 1, stepY = dirY < 0.f ? -1 : 1;
        float distanceX = (dirX < 0.f ? state.x - float(mx) : float(mx + 1) - state.x) * ddx;
        float distanceY = (dirY < 0.f ? state.y - float(my) : float(my + 1) - state.y) * ddy;
        bool hit = false, ySide = false;
        unsigned wall = 0;
        float dist = 24.f;
        // Maximum 128 DDA steps even for corrupted/unclosed levels.
        for (unsigned i = 0; i < 128; ++i)
        {
            if (distanceX < distanceY)
            {
                dist = distanceX; distanceX += ddx; mx += stepX; ySide = false;
            }
            else
            {
                dist = distanceY; distanceY += ddy; my += stepY; ySide = true;
            }
            if (dist > 24.f) break;
            if (blocked(map, mx, my))
            {
                hit = true;
                if (mx >= 0 && my >= 0 && mx < int(map.width) && my < int(map.height))
                    wall = map.walls[my * map.width + mx];
                break;
            }
        }
        if (!hit) continue;
        if (dist < .1f) dist = .1f;
        /* Keep the *unclipped* projection for correct texture Y sampling
         * even with nearby walls taller than the screen. */
        // Old preview used 150/dist with a 66-degree FOV and only 80
        // columns, making wall patterns shrink/alias far too quickly.
        // Scale follows the original VIEWWIDTH/FOV relationship approximately.
        const float projectedHeight = kWallScale / (dist + 1.f);
        const float projectedTop = (float(kHeight) - projectedHeight) * .5f;
        int top = int(floorf(projectedTop));
        int bottom = int(ceilf(projectedTop + projectedHeight));
        if (top < 0) top = 0;
        if (bottom > int(kHeight)) bottom = int(kHeight);
        /* Map the wall code to the *original* light/dark picture from
         * cat3d/c3_draw.c. Geometry is still our temporary DDA raycaster. */
        const int picture = cat3dsWallPicture(wall, ySide);
        const bool hasPicture = textures && textures->ready &&
                                picture >= 134 && picture <= 158;
        int texX = 0;
        if (hasPicture)
        {
            float hit = ySide ? state.x + dist * dirX : state.y + dist * dirY;
            hit -= floorf(hit);
            texX = int(hit * 64.f);
            if ((!ySide && dirX > 0.f) || (ySide && dirY < 0.f))
                texX = 63 - texX;
            if (texX < 0) texX = 0;
            if (texX > 63) texX = 63;
        }
        const uint8_t bright[4] = {11, 13, 14, 10};
        const uint8_t dark[4] = {3, 5, 6, 2};
        const uint8_t fallback = ySide ? dark[(wall / 4) & 3] : bright[(wall / 4) & 3];
        const uint16_t xByte = uint16_t(highQuality ? ray / 8 : ray / 4);
        const uint8_t mask = highQuality ? uint8_t(0x80u >> (ray & 7u))
                                         : uint8_t(0xC0u >> (2u * (ray & 3u)));
        for (int y = top; y < bottom; ++y)
        {
            uint8_t color = fallback;
            if (hasPicture)
            {
                int texY = int((float(y) + .5f - projectedTop) * (64.f / projectedHeight));
                if (texY < 0) texY = 0;
                if (texY > 63) texY = 63;
                color = textures->pixels[picture - 134][texY * 64 + texX];
            }
            BE_ST_EGAUpdateGFXBitsFrom4bitsPixel(uint16_t(y * 40 + xByte), color, mask);
        }
    }
    // Small centered reticle (preview only).
    BE_ST_EGAUpdateGFXBitsFrom4bitsPixel(uint16_t(100 * 40 + 19), 15, 0x01);
    BE_ST_EGAUpdateGFXBitsFrom4bitsPixel(uint16_t(100 * 40 + 20), 15, 0x80);
    BE_ST_EGAUpdateGFXBitsFrom4bitsPixel(uint16_t(99 * 40 + 20), 15, 0x80);
    BE_ST_EGAUpdateGFXBitsFrom4bitsPixel(uint16_t(101 * 40 + 20), 15, 0x80);
}
