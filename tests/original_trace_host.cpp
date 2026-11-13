#include "../platform/3ds/source/cat3ds_original_trace.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main()
{
    static Cat3DSLevelMap map = {};
    map.index=0; map.width=16; map.height=16;
    for(unsigned y=0; y<16; ++y) for(unsigned x=0; x<16; ++x)
        if (!x || !y || x==15 || y==15) map.walls[y*16+x]=1;
    Cat3DSWalkPreview camera = {};
    camera.x=8.5f; camera.y=8.5f;
    for(int a=0; a<360; ++a)
    {
        camera.angle=float(a*3.14159265358979323846/180.0);
        Cat3DSOriginalTrace result = {};
        assert(cat3dsTraceOriginalViewEdges(map,camera,&result));
        assert(result.firstAngleDegrees == 20);
        assert(result.leftWall == 1 && result.rightWall == 1);
        assert(result.leftX>=0 && result.rightX>=0 && result.leftY>=0 && result.rightY>=0);
    }
    // A gap or corrupt/sparse map must never result in an out-of-bounds read.
    for (unsigned y=0; y<16; ++y) map.walls[y*16+15]=0;
    camera.angle=0;
    Cat3DSOriginalTrace r={};
    assert(!cat3dsTraceOriginalViewEdges(map,camera,&r));
    camera.x=INFINITY;
    assert(!cat3dsTraceOriginalViewEdges(map,camera,&r));
    puts("original_trace_host: PASS (360 headings, original 20-degree edges)");
}
