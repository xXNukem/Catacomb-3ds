#include "c3_scale.c"
#include <cassert>
#include <cstdlib>
#include <cstring>

static uint8_t pixels[16];
void BE_ST_EGAUpdateGFXBitsFrom4bitsPixel(uint16_t offset,uint8_t color,uint8_t mask)
{
    assert(offset < 2);
    for (unsigned bit=0; bit<8; ++bit)
        if (mask & (0x80u >> bit)) pixels[8*offset+bit]=color;
}

REFKEEN_NS_B
decltype(bufferofs) bufferofs;
decltype(zbuffer) zbuffer;
int cat3dsStereoOcclusionEnabled=1;
fixed cat3dsWallDepth[VIEWWIDTH], cat3dsSpriteDepth=65536;
#ifdef REFKEEN_VER_CAT3D
void Quit(const char*) { abort(); }
#else
void Quit(const char*, ...) { abort(); }
#endif
static void testMask()
{
    /* One compiled source column with six destination pixels spanning two EGA bytes. */
    auto *scaler=(t_compscale *)calloc(1,sizeof(t_compscale)+9);
    auto *sprite=(t_compshape *)calloc(1,sizeof(t_compshape)+31);
    assert(scaler && sprite);
    scaledirectory[3]=scaler;
    scaler->start[1]=scaler->width[0]=6;
    scaler->codeofs[0]=sizeof(t_compscale);
    scaler->codeofs[1]=sizeof(t_compscale)+8;
    scaler->code[3]=0x26;
    scaler->code[8]=0xcb;
    sprite->width=1;
    sprite->codeofs[0]=sizeof(t_compshape);
    uint8_t *code=(uint8_t *)(sprite+1);
    code[2]=2;
    const uint16_t colorOffset=sizeof(t_compshape)+30;
    memcpy(code+19,&colorOffset,2);
    code[29]=0xcb; code[30]=7;
    for (unsigned pattern=0; pattern<64; ++pattern) {
        memset(pixels,0,sizeof pixels);
        for (unsigned i=0;i<6;++i) cat3dsWallDepth[6+i]=(pattern & (1u<<i)) ? 65535 : 65537;
        ScaleShape(9,sprite,6);
        for (int x=0;x<16;++x) {
            bool drawn=x>=6 && x<12 && !(pattern & (1u<<(x-6)));
            assert(pixels[x]==(drawn ? 7 : 0));
        }
    }
    free(sprite);
    free(scaler);
}
REFKEEN_NS_E
int main() { TEST_GAME::testMask(); }
