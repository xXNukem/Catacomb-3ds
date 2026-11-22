// Original damage and death drawing with synthetic EGA, sound and input.
#ifdef REFKEEN_VER_CAT3D
#include "c3_def.h"
#else
#include "def.h"
#endif
#include <cassert>
#include "cat3ds_dualscreen.h"

static int modal, ack, copies, sounds, scoreEntries;
void Cat3DS_DualScreenSetGameplay(int enabled) { assert(!enabled); }
void Cat3DS_DualScreenBeginLevelTransition() { ++modal; }
void BE_ST_EGAUpdateGFXBufferInAllPlanesScrToScr(uint16_t,uint16_t,uint16_t) { ++copies; }
void BE_ST_EGAUpdateGFXByteInAllPlanesScrToScr(uint16_t,uint16_t) {}
void BE_ST_EGAUpdateGFXByteInPlaneScrToScr(uint16_t,uint16_t,uint16_t) {}

REFKEEN_NS_B
#define STORE(n) decltype(n) n
STORE(godmode); STORE(bordertime); STORE(pictable);
STORE(ylookup); STORE(linewidth); STORE(bufferofs); STORE(displayofs);
void VW_ColorBorder(id0_int_t) {}
void SD_PlaySound(soundnames) { ++sounds; }
void IN_Ack() { ++ack; }
#ifdef REFKEEN_VER_CAT3D
void Died();
void CheckHighScore(id0_long_t,id0_word_t);
HighScore Scores[MaxScores]={};
STORE(HighScoresDirty); STORE(grsegs); STORE(grneeded); STORE(ca_levelbit);
STORE(PrintY); STORE(PrintX); STORE(fontcolor);
void CA_CacheGrChunk(id0_short_t pic) { assert(pic==HIGHSCORESPIC); }
void VWB_DrawPic(id0_int_t,id0_int_t,id0_int_t pic) { assert(pic==HIGHSCORESPIC); }
void US_Print(const char*) {}
decltype(USL_MeasureString) USL_MeasureString=[](const char*,const char*,id0_word_t *w,id0_word_t *h){ *w=*h=8; };
bool US_LineInput(id0_int_t,id0_int_t,char*,const char*,bool,id0_int_t,id0_int_t) { ++scoreEntries; return true; }
void VW_SetSplitScreen(id0_int_t height) { assert(height==200); }
void VW_Bar(id0_unsigned_t,id0_unsigned_t,id0_unsigned_t,id0_unsigned_t,id0_unsigned_t) {}

STORE(gamestate); STORE(screenloc); STORE(playstate); STORE(screenpage); STORE(shapedirectory); STORE(walldirectory);
void IN_ClearKeysDown() {}
void VW_SetScreen(id0_unsigned_t,id0_unsigned_t) {}
void VW_ScreenToScreen(id0_unsigned_t,id0_unsigned_t,id0_unsigned_t,id0_unsigned_t) { ++copies; }
void MM_SetPurge(memptr*,id0_int_t) {}
#else
STORE(Flags); STORE(LastScan); id0_char_t GetKeyChoice(const char*,bool) { assert(false); return 0; }
STORE(gamestate); STORE(latchpics); STORE(status_flag); STORE(status_delay);
STORE(bcolor); STORE(grneeded); STORE(ca_levelbit); STORE(WindowX); STORE(WindowW);
STORE(PrintY); STORE(fontcolor);
static int cached, drawn;
void CA_CacheGrChunk(id0_short_t chunk) { assert(chunk==FACE5PIC); ++cached; }
void VW_DrawPic(id0_unsigned_t,id0_unsigned_t,id0_unsigned_t pic) { assert(cached && pic==FACE5PIC); ++drawn; }
void VW_Bar(id0_unsigned_t,id0_unsigned_t,id0_unsigned_t,id0_unsigned_t,id0_unsigned_t) {}
void US_CPrintLine(const char*,const char*) {}
void LatchDrawPic(id0_unsigned_t,id0_unsigned_t,id0_unsigned_t) { assert(false); }
#ifndef REFKEEN_VER_CATABYSS
id0_int_t EasyDoDamage(id0_int_t points) { return points; }
#endif
#endif
static void checkDeath()
{
    gamestate.body=1;
    TakeDamage(1);
    assert(!gamestate.body && sounds==1);
#ifdef REFKEEN_VER_CAT3D
    assert(playstate==ex_died && copies==3);
    pictabletype pictures[NUMPICS]={};
    pictable=pictures;
    pictures[DEADPIC-STARTPICS].width=1;
    pictures[DEADPIC-STARTPICS].height=1;
    Died();
    assert(ack==1 && modal==1 && sounds==2 && copies==5);
    NormalScreen();
    FreeUpMemory();
    CheckHighScore(100,1);
    assert(HighScoresDirty && Scores[0].score==100 && scoreEntries==1);
#else
    assert(Flags & FL_DEAD);
    DrawHealth();
    assert(cached==1 && drawn==1);
#endif
}
REFKEEN_NS_E
int main() { TEST_GAME::checkDeath(); }
