// Compile with the Apocalypse namespace and its original controller tables.
#include "def.h"
#include "altcontroller.h"
#include "cat3ds_layout.h"
#include <cassert>

uint16_t BE_ST_Compat_GetFarPtrRelocationSegOffset() { return 0x120; }
RefKeenConfig g_refKeenCfg;
BE_ST_ControllerMapping g_beStControllerMappingTextInput, g_beStControllerMappingDebugKeys;
void BE_ST_AltControlScheme_InitTouchControlsUI(BE_ST_OnscreenTouchControl *) {}
void BE_ST_AltControlScheme_DeclareSensorsUse(int) {}

namespace catapoc {
extern BE_ST_ControllerMapping g_ingame_altcontrol_mapping_gameplay, g_ingame_altcontrol_mapping_funckeys;
void RefKeen_PrepareAltControllerScheme();
}

void checkApocalypseControlsAndLayout()
{
    using namespace catapoc;
    for (auto &bind : g_refKeenCfg.cat3d.binds) bind.pad = -1;
    auto &cfg = g_refKeenCfg.cat3d;
    cfg.useLeftStick = true;
    cfg.binds[BE_ST_CTRL_BIND_CAT3D_FIRE].pad = BE_ST_CTRL_BUT_A;
    cfg.binds[BE_ST_CTRL_BIND_CAT3D_DRINK].pad = BE_ST_CTRL_BUT_X;
    cfg.binds[BE_ST_CTRL_BIND_CAT3D_BOLT].pad = BE_ST_CTRL_BUT_Y;
    cfg.binds[BE_ST_CTRL_BIND_CAT3D_NUKE].pad = BE_ST_CTRL_EXTRA_BUT_LTRIGGER;
    RefKeen_PrepareAltControllerScheme();
    const auto &pad = g_ingame_altcontrol_mapping_gameplay;
    assert(pad.pbuttons[BE_ST_CTRL_BUT_START].val == BE_ST_SC_ESC);
    assert(pad.pbuttons[BE_ST_CTRL_BUT_BACK].val == BE_ST_SC_PAUSE);
    assert(pad.pbuttons[BE_ST_CTRL_BUT_A].miscPtr == &g_binding_value_button[0]);
    assert(pad.pbuttons[BE_ST_CTRL_BUT_LSHOULDER].miscPtr == &g_binding_value_button[1]);
    assert(pad.pbuttons[BE_ST_CTRL_BUT_X].miscPtr == &g_binding_value_drink);
    assert(pad.pbuttons[BE_ST_CTRL_BUT_Y].miscPtr == &g_binding_value_bolt);
    assert(pad.pbuttons[BE_ST_CTRL_BUT_DPAD_UP].miscPtr == &g_binding_value_up);
    assert(pad.paxes[BE_ST_CTRL_AXIS_LY][0].miscPtr == &g_binding_value_up);
    assert(pad.paxes[BE_ST_CTRL_AXIS_LTRIGGER][1].miscPtr == &g_binding_value_nuke);
    const auto &functions = g_ingame_altcontrol_mapping_funckeys;
    assert(pad.paxes[BE_ST_CTRL_AXIS_RTRIGGER][1].miscPtr == &functions);
    assert(functions.pbuttons[BE_ST_CTRL_BUT_DPAD_UP].val == BE_ST_SC_F1);
    assert(functions.pbuttons[BE_ST_CTRL_BUT_DPAD_RIGHT].val == BE_ST_SC_F2);
    assert(functions.pbuttons[BE_ST_CTRL_BUT_DPAD_DOWN].val == BE_ST_SC_F3);
    assert(functions.pbuttons[BE_ST_CTRL_BUT_DPAD_LEFT].val == BE_ST_SC_F4);

    auto world = Cat3DS_GetLayout(true, false, VIEWWIDTH);
    assert(world.sourceWidth == 240 && world.sourceHeight == VIEWHEIGHT);
    assert(world.outputWidth == 400 && world.outputHeight == 240 && STATUSLINES == 80);
    auto help = Cat3DS_GetLayout(true, true, 640);
    assert(help.sourceWidth == 640 && help.sourceHeight == VIEWHEIGHT + STATUSLINES);
}
