#include <initializer_list>
#include "../platform/3ds/original/cat3ds_wall_clearance.h"
#include <cassert>
int main() {
    constexpr int tile=65536, radius=7*tile/16;
    auto room=[](int x,int y){return x<=1 || y<=1 || x>=5 || y>=5;};
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
