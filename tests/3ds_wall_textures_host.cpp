/* Host-only test for EGA wall picture decoding. Does not bundle game assets.
 * Usage (from project root):
 *   g++ -std=c++11 -Wall -Wextra -Werror \
 *     tests/3ds_wall_textures_host.cpp \
 *     platform/3ds/source/cat3ds_wall_textures.cpp \
 *     -o /tmp/cat3ds_wall_test
 *   /tmp/cat3ds_wall_test /path/to/cat3d/ /path/to/unpacked-image.bin
 * The stub mimics LZEXE unpack output only: the production build uses the
 * original ReflectionHLE Unlzexe_unpack implementation instead.
 */
#include "../platform/3ds/source/cat3ds_wall_textures.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *unpackedImage = NULL;
extern "C" bool Unlzexe_unpack(FILE *exe, unsigned char *out, int size, uint16_t *extra)
{
    (void)exe;
    FILE *f = fopen(unpackedImage, "rb");
    if (!f) return false;
    const bool ok = fread(out, 1, size_t(size), f) == size_t(size) && fgetc(f) == EOF;
    fclose(f);
    if (extra) *extra = 0;
    return ok;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s /path/to/cat3d /path/to/unpacked-image.bin\n", argv[0]);
        return 2;
    }
    unpackedImage = argv[2];
    static Cat3DSWallTextures textures = {};
    char error[128] = {};
    const bool ok = cat3dsLoadCat3dWallTextures(
        "Catacomb 3-D v1.22", "CAT3D.EXE", argv[1], &textures, error, sizeof(error));
    if (!ok) { fprintf(stderr, "Wall art error: %s\n", error); return 1; }
    assert(textures.ready);
    unsigned varyingPictures = 0;
    for (unsigned i = 0; i < 25; ++i)
    {
        unsigned palette = 0;
        for (unsigned j = 0; j < 4096; ++j)
        {
            assert(textures.pixels[i][j] < 16);
            palette |= 1u << textures.pixels[i][j];
        }
        if (palette && (palette & (palette - 1))) ++varyingPictures;
    }
    assert(varyingPictures == 25);
    assert(cat3dsWallPicture(1, false) == 137);
    assert(cat3dsWallPicture(1, true) == 138);
    assert(cat3dsWallPicture(2, false) == 140);
    assert(cat3dsWallPicture(2, true) == 139);
    assert(cat3dsWallPicture(15, false) == 134);
    assert(cat3dsWallPicture(18, false) == 151);
    assert(cat3dsWallPicture(21, true) == 152);
    assert(cat3dsWallPicture(33, false) == 158);
    assert(cat3dsWallPicture(0, true) == -1);
    assert(cat3dsWallPicture(35, false) == -1);
    printf("PASS: 25/25 original 64x64 EGA textures decoded; tile mapping OK\n");
}
