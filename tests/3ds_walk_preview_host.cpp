#include "../platform/3ds/source/cat3ds_walk_preview.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Link-only stub: map/renderer test does not need executable unpacking. */
extern "C" bool Unlzexe_unpack(FILE *, unsigned char *, int, uint16_t *) { return false; }

static unsigned fills = 0, masks = 0;
static unsigned ceilingLines = 0, floorLines = 0;
static unsigned singlePixelWrites = 0, doublePixelWrites = 0;
extern "C" void BE_ST_EGAUpdateGFXBufferFrom4bitsPixel(uint16_t off, uint8_t color, uint16_t count)
{
    assert(off < 8000 && count <= 40 && color < 16); ++fills;
    if (count == 40) {
        assert(off % 40 == 0);
        if (off < 100 * 40) {assert(color == 0); ++ceilingLines;}
        else {assert(color == 8); ++floorLines;}
    }
}
extern "C" void BE_ST_EGAUpdateGFXBitsFrom4bitsPixel(uint16_t off, uint8_t color, uint8_t mask)
{
    assert(off < 8000 && color < 16 && mask); ++masks;
    switch (mask) {
      case 0x80: case 0x40: case 0x20: case 0x10:
      case 0x08: case 0x04: case 0x02: case 0x01:
        ++singlePixelWrites; break;
      case 0xC0: case 0x30: case 0x0C: case 0x03:
        ++doublePixelWrites; break;
      default: assert(false && "EGA view writes must be pixel or two-pixel masks");
    }
}
int main()
{
    Cat3DSLevelMap map = {};
    map.width = 16; map.height = 16; map.index = 0;
    for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)
        if (!x || !y || x==15 || y==15) map.walls[y*16+x]=1;
    map.objects[8*16+8]=2; // East.
    Cat3DSWalkPreview w = {};
    assert(cat3dsStartWalkPreview(map, &w));
    assert(fabs(w.x - 8.5f)<.01f && fabs(w.angle)<.01f);
    cat3dsRenderWalkPreview(map,w);
    assert(fills==200 && masks>100 && singlePixelWrites>100);
    assert(ceilingLines == 100 && floorLines == 100);
    static Cat3DSWallTextures textures = {};
    textures.ready = true;
    for (unsigned i = 0; i < 25; ++i)
        for (unsigned j = 0; j < 4096; ++j)
            textures.pixels[i][j] = uint8_t((i + j) & 15);
    const unsigned masksBefore = masks;
    cat3dsRenderWalkPreview(map, w, &textures);
    assert(fills == 400 && masks > masksBefore);
    assert(ceilingLines == 200 && floorLines == 200);
    // The lower-quality mode still fills every other image column.
    const unsigned beforeDouble = doublePixelWrites;
    cat3dsRenderWalkPreview(map, w, &textures, false);
    assert(fills == 600 && doublePixelWrites > beforeDouble);
    assert(ceilingLines == 300 && floorLines == 300);
    for(int i=0;i<400;i++)cat3dsUpdateWalkPreview(map,&w,1,0,0,.016f);
    assert(w.x < 14.83f && w.x > 14.f);
    assert(w.y == 8.5f);
    cat3dsUpdateWalkPreview(map,&w,0,0,1,.01f);
    assert(w.angle>0);
    map.objects[8*16+8]=0;
    assert(!cat3dsStartWalkPreview(map,&w));
    printf("3ds_walk_preview_host: pass; EGA writes %u fills, %u masks\n",fills,masks);
}
