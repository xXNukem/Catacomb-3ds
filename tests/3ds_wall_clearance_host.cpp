#include <initializer_list>
#include "../platform/3ds/original/cat3ds_wall_clearance.h"
#include <cassert>
int main() {
    constexpr int tile=65536, radius=CAT3DS_WALL_RADIUS;
    static_assert(radius > 2*tile/5, "clearance must exceed original player radius");
    static_assert(2*radius < tile, "one tile passage must remain traversable");
    auto room=[](int x,int y){return x<=1 || y<=1 || x>=5 || y>=5;};
    // A wall exists in the visual tilemap even when an actor overlay is empty.
    auto visibleWall=[](int x,int y){return x==4 && y==3;};
    int x=3*tile+tile/2, y=3*tile+tile/2;
    for (int i=0;i<32;++i) x+=Cat3DS_WallStep(x,y,8192,visibleWall);
    assert(x==4*tile-radius-1);
    // Keep the passage open and permit moving away from the wall.
    assert(Cat3DS_WallStep(x,y,-8192,visibleWall)==-8192);
    for (int dx : {-8192,8192}) for (int dy : {-8192,8192}) {
        int x=3*tile,y=x;
        for (int i=0;i<100;++i) {
            x+=Cat3DS_WallStep(x,y,dx,room);
            y+=Cat3DS_WallStep(y,x,dy,[&](int a,int b){return room(b,a);});
        }
        assert(x==(dx<0 ? 2*tile+radius : 5*tile-radius-1));
        assert(y==(dy<0 ? 2*tile+radius : 5*tile-radius-1));
        assert(Cat3DS_WallStep(x,y,-dx,room)==-dx);
    }
}
