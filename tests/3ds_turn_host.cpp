// Compile the production movement routine independently so every Thrust call
// is observable, including a call whose collision would keep the player still.
#ifdef REFKEEN_VER_CAT3D
#include "c3_def.h"
#else
#include "def.h"
#endif
#include <cassert>

REFKEEN_NS_B
#ifdef REFKEEN_VER_CAT3D
decltype(c) c;
static auto &input=c;
#else
decltype(control) control;
static auto &input=control;
#endif
decltype(tics) tics;
decltype(running) running;
decltype(mousexmove) mousexmove;
decltype(mouseymove) mouseymove;
static int translations;
void Thrust(id0_int_t, id0_unsigned_t) { ++translations; }
#include "movement.inc"

static void checkTurn()
{
    objtype avatar={};
    for (int fast=0;fast<=1;++fast) {
        running=fast;
        for (int heldFire=0;heldFire<=1;++heldFire) {
            input.button0=heldFire;
            input.xaxis=motion_Right;
            for (int ticks=1;ticks<=3;ticks+=2) {
                tics=ticks;
                avatar.x=TILEGLOBAL+TILEGLOBAL/16;
                avatar.y=3*TILEGLOBAL;
                const int step=tics*(fast ? 4 : 2);
                for (int turn=step;turn<=ANGLES;turn+=step) {
                    ControlMovement(&avatar);
                    assert(avatar.angle==(ANGLES-turn)%ANGLES);
                    assert(avatar.x==TILEGLOBAL+TILEGLOBAL/16 && avatar.y==3*TILEGLOBAL);
                }
                assert(!translations && !avatar.angle);
            }
        }
    }
    input.button1=true;
    ControlMovement(&avatar);
    assert(translations==1 && !avatar.angle);
}
REFKEEN_NS_E
int main() { TEST_GAME::checkTurn(); }
