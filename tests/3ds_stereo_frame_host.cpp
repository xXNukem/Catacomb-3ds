/* Exercise the engine frame driver with synthetic assets and a deterministic clock.
   The scaler records column data where the EGA backend would rasterize it. */
#include <cassert>
#include <cstring>
#ifdef REFKEEN_VER_CAT3D
#include "c3_draw.c"
#elif defined(REFKEEN_VER_CATABYSS)
#include "c4_draw.c"
#else
#include "c5_draw.c"
#endif

static int offset, captures, publishes, observations, passes, shapes;
static unsigned signatures[2];
static int actorX[2], fades, wallAnimations;
static uint8_t observed[64*64];
int Cat3DS_DualScreenStereoOffset() { return offset; }
void Cat3DS_DualScreenCaptureLeftEGA(uint16_t) { ++captures; }
void Cat3DS_DualScreenForgetStereo() {}
void Cat3DS_DualScreenBeginLevelTransition() {}
void Cat3DS_DualScreenReportRenderTicks(uint64_t) {}
void Cat3DS_DualScreenObserve(int,int,int,const uint8_t*,const uint8_t *visible,int32_t,int32_t,int)
{ ++observations; memcpy(observed, visible, sizeof observed); }
void BE_ST_SetScreenStartAddress(uint16_t) { ++publishes; }
void BE_ST_GetEmuAccuMouseMotion(int16_t*,int16_t*) {}
void BE_ST_EGAUpdateGFXBufferFrom4bitsPixel(uint16_t,uint8_t,uint16_t) {}

REFKEEN_NS_B
#define STORAGE(name) decltype(name) name
#ifdef REFKEEN_VER_CAT3D
STORAGE(aborttrace);
#endif
STORAGE(displayofs); STORAGE(handheight); STORAGE(wallheight);
STORAGE(MousePresent); STORAGE(walldirectory); STORAGE(shapedirectory); STORAGE(tics);
STORAGE(grsegs); STORAGE(player); STORAGE(spotvis); STORAGE(tilemap); STORAGE(wallofs);
STORAGE(wallseg); STORAGE(ylookup); STORAGE(mapwidth); STORAGE(sintable); STORAGE(boltsleft);
STORAGE(bufferofs); STORAGE(gamestate); STORAGE(mapheight); STORAGE(picmtable); STORAGE(wallwidth);
#ifdef REFKEEN_VER_CAT3D
STORAGE(DemoMode);
#else
STORAGE(playstate); STORAGE(realtics);
static id0_unsigned_t color;
id0_unsigned_t *skycolor=&color, *groundcolor=&color;
void Quit(const char*, ...) { abort(); }
STORAGE(bordertime);
void Thrust(id0_int_t,id0_unsigned_t) { abort(); }
#ifndef REFKEEN_VER_CATABYSS
void AnimateWallList() { ++wallAnimations; }
#endif
#endif
void FizzleFade(uint16_t,uint16_t,uint16_t,uint16_t,bool) { ++fades; }
void VW_MaskBlock(memptr,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t) {}
id0_longword_t SD_GetTimeCount() { return 4; }
void SD_SetTimeCount(id0_longword_t) {}
void SD_AddToTimeCount(id0_longword_t) {}
void SD_TimeCountWaitForDest(id0_longword_t) {}
void ScaleShape(id0_int_t x,t_compshape*,id0_unsigned_t) { assert(shapes < 2); actorX[shapes++]=x; }
void ScaleWalls()
{
    unsigned hash=0;
    assert(passes < 2);
    for (int x=0; x<VIEWWIDTH; ++x)
        if (wallwidth[x]) hash = hash*31u + wallheight[x]*64u + wallofs[x]/64u;
    signatures[passes++]=hash;
}
static void testFrame()
{
    objtype avatar={}, actor={};
    statetype state={};
    state.shapenum=FIRSTSCALEPIC;
    mapwidth=mapheight=8;
    for(int i=0;i<8;i++) tilemap[0][i]=tilemap[7][i]=tilemap[i][0]=tilemap[i][7]=3;
    static uint8_t texture[4096];
    for (auto &wall : walldirectory) wall=texture;
    for (int i=0;i<ANGLES+ANGLES/4;++i) {
        const double value=sin(i*2.0*3.141592653589793/ANGLES);
        sintable[i]=(int)(fabs(value)*65535) | (value<0 ? 0x80000000u : 0);
    }
    firstangle=19; lastangle=-19; prestep=(fixed)(TILEGLOBAL / cos(19*3.141592653589793/180));
    walls[0].x2=VIEWX-1; walls[0].height2=32000;
    scale=172;
    avatar.x=3*TILEGLOBAL; avatar.y=4*TILEGLOBAL;
    avatar.next=&actor; player=&avatar;
    actor.state=&state; actor.x=6*TILEGLOBAL; actor.y=4*TILEGLOBAL;
    actor.tilex=6; actor.tiley=4;
    const objtype initial=actor;
    offset=0; ThreeDRefresh();
    assert(passes==1 && captures==0 && publishes==1 && observations==1 && shapes==1);
    const objtype expected=actor;
    const int monoActorX=actorX[0];
    const auto expectedTics=tics;
    const auto expectedPage=screenpage;
    uint8_t expectedMap[sizeof observed]; memcpy(expectedMap,observed,sizeof observed);
    actor=initial;
    screenpage=0; lasttimecount=0;
    offset=1280; passes=captures=publishes=observations=shapes=0;
    ThreeDRefresh();
    assert(passes==2 && signatures[0]!=signatures[1]);
    assert(captures==1 && publishes==1 && observations==1 && shapes==2);
    assert(actorX[0] <= monoActorX && monoActorX <= actorX[1] && actorX[0] < actorX[1]);
    assert(screenpage==expectedPage && tics==expectedTics);
#if defined(REFKEEN_VER_CATAPOC) || defined(REFKEEN_VER_CATARM)
    assert(wallAnimations==2);
#endif
    assert(!memcmp(&actor,&expected,sizeof actor) && !memcmp(observed,expectedMap,sizeof observed));
    fizzlein=true; passes=captures=publishes=observations=shapes=0;
    ThreeDRefresh();
    assert(fades==1 && passes==1 && captures==0 && publishes==1 && !fizzlein);
    passes=captures=publishes=observations=shapes=0;
    ThreeDRefresh();
    assert(passes==2 && captures==1 && publishes==1 && fades==1);
    actor.x=avatar.x+TILEGLOBAL/3;
    actor.tilex=actor.x>>TILESHIFT;
    passes=captures=publishes=observations=shapes=0;
    ThreeDRefresh();
    assert(shapes==2 && actorX[0]>=actorX[1]);
    assert(int(actorX[0])-int(actorX[1]) <= 2);
    avatar.next=nullptr;
    for (int corner=0;corner<2;++corner) {
        const fixed x=corner ? TILEGLOBAL+TILEGLOBAL/16 : 4*TILEGLOBAL;
        const fixed y=corner ? TILEGLOBAL+TILEGLOBAL/16 : 3*TILEGLOBAL;
        avatar.x=x; avatar.y=y;
        for (int mode=0;mode<2;++mode) {
            offset=1280*mode;
            for (int angle=0;angle<=ANGLES;++angle) {
                avatar.angle=angle%ANGLES;
                passes=captures=publishes=observations=shapes=0;
                ThreeDRefresh();
                assert(avatar.x==x && avatar.y==y && avatar.angle==angle%ANGLES);
                assert(publishes==1 && observations==1);
            }
            assert(avatar.angle==0);
        }
    }
}
REFKEEN_NS_E
int main() { TEST_GAME::testFrame(); }
