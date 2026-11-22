// No sentinel follows these dirty tiles. The last run reaches the allocation boundary.
#include "c3_def.h"
#include <cassert>
#include <vector>
static int copies;
static unsigned bytes;
void BE_ST_EGAUpdateGFXBufferInAllPlanesScrToScr(uint16_t,uint16_t,uint16_t n) { ++copies; bytes+=n; }
void BE_ST_EGAUpdateGFXByteInAllPlanesScrToScr(uint16_t,uint16_t) { ++copies; ++bytes; }
REFKEEN_NS_B
decltype(updateptr) updateptr;
decltype(blockstarts) blockstarts;
decltype(displayofs) displayofs;
decltype(bufferofs) bufferofs;
decltype(linewidth) linewidth;
void VWL_UpdateScreenBlocks();
static void check()
{
    std::vector<id0_byte_t> tiles(UPDATEWIDE*13);
    updateptr=tiles.data();
    VWL_UpdateScreenBlocks();
    assert(!copies);
    tiles[0]=tiles[tiles.size()-2]=tiles.back()=1;
    VWL_UpdateScreenBlocks();
    assert(copies==48 && bytes==96);
    for (auto tile : tiles) assert(!tile);
}
REFKEEN_NS_E
int main() { cat3d::check(); }
