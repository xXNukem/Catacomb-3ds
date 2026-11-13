#ifndef CAT3DS_WALL_TEXTURES_H
#define CAT3DS_WALL_TEXTURES_H

#include <stddef.h>
#include <stdint.h>

/* ORIGINAL EGA WALL ART from the user's Catacomb 3-D files.
 * Graphics chunks 134..158, 25 64x64 pictures (light/dark variants).
 * The temporary raycaster reads this cache; no game assets are embedded.
 * This is not a replacement for the eventual original game renderer. */
struct Cat3DSWallTextures {
    uint8_t pixels[25][64 * 64];
    bool ready;
};

bool cat3dsLoadCat3dWallTextures(const char *version, const char *exeFilename,
                                const char *folder, Cat3DSWallTextures *textures,
                                char *error, size_t errorSize);

/* Map tile code -> original picture ID, following walllight1/walldark1
 * in ReflectionHLE's cat3d/c3_draw.c. -1 means no standard wall picture.
 * ySide=true is a north/south wall (dark image in the original renderer). */
int cat3dsWallPicture(unsigned tile, bool ySide);

#endif
