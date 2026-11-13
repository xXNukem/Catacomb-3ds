#ifndef CAT3DS_WALK_PREVIEW_H
#define CAT3DS_WALK_PREVIEW_H

#include "cat3ds_level_map.h"
#include "cat3ds_wall_textures.h"

/* Temporary raycasting walk test over authentic decompressed level data.
 * This is NOT the original Catacomb 3-D renderer / player logic. */
struct Cat3DSWalkPreview {
    float x, y, angle;
    unsigned spawnX, spawnY;
    unsigned level;
};

bool cat3dsStartWalkPreview(const Cat3DSLevelMap &map, Cat3DSWalkPreview *state);
bool cat3dsUpdateWalkPreview(const Cat3DSLevelMap &map, Cat3DSWalkPreview *state,
                            float forward, float strafe, float turn, float seconds);
void cat3dsRenderWalkPreview(const Cat3DSLevelMap &map, const Cat3DSWalkPreview &state,
                                const Cat3DSWallTextures *textures = nullptr,
                                bool highQuality = true);

#endif
