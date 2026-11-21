#include "refkeen.h"
#include "cat3ds_bindings.h"
#include "altcontroller.h"
#include <cassert>

#ifdef REFKEEN_VER_CATABYSS
BE_GameVer_T refkeen_current_gamever = BE_GAMEVER_CATABYSS113;
#endif
RefKeenConfig g_refKeenCfg;
BE_ST_ControllerMapping g_beStControllerMappingTextInput, g_beStControllerMappingDebugKeys;
void BE_ST_AltControlScheme_InitTouchControlsUI(BE_ST_OnscreenTouchControl *) {}
void BE_ST_AltControlScheme_DeclareSensorsUse(int) {}

REFKEEN_NS_B
extern BE_ST_ControllerMapping g_ingame_altcontrol_mapping_gameplay, g_ingame_altcontrol_mapping_inackback;
#ifdef REFKEEN_VER_CAT3D
extern BE_ST_ControllerMapping g_ingame_altcontrol_mapping_menu, g_ingame_altcontrol_mapping_menu_confirm;
#endif
#if defined(REFKEEN_VER_CAT3D) || defined(REFKEEN_VER_CATABYSS)
extern BE_ST_ControllerMapping g_ingame_altcontrol_mapping_scrolls;
#else
extern BE_ST_ControllerMapping g_ingame_altcontrol_mapping_funckeys;
#endif
void RefKeen_PrepareAltControllerScheme();
REFKEEN_NS_E

int main()
{
    using namespace TEST_GAME;
    for (auto &bind : g_refKeenCfg.cat3d.binds) bind.pad=BE_ST_CTRL_BUT_A;
    Cat3DS_ConfigureBindings(g_refKeenCfg);
    RefKeen_PrepareAltControllerScheme();
    auto &map = g_ingame_altcontrol_mapping_gameplay;
    assert(map.pbuttons[BE_ST_CTRL_BUT_A].miscPtr==&g_binding_value_button[0]);
    assert(map.pbuttons[BE_ST_CTRL_BUT_B].miscPtr==&g_binding_value_button[1]);
    assert(map.pbuttons[BE_ST_CTRL_BUT_LSHOULDER].miscPtr==&g_binding_value_button[1]);
    assert(map.pbuttons[BE_ST_CTRL_BUT_RSHOULDER].miscPtr==&g_binding_value_fastturn);
    assert(map.pbuttons[BE_ST_CTRL_BUT_X].miscPtr==&g_binding_value_drink);
    assert(map.pbuttons[BE_ST_CTRL_BUT_Y].miscPtr==&g_binding_value_bolt);
    assert(map.paxes[BE_ST_CTRL_AXIS_LTRIGGER][1].miscPtr==&g_binding_value_nuke);
    assert(map.paxes[BE_ST_CTRL_AXIS_LY][0].miscPtr==&g_binding_value_up);
    assert(map.paxes[BE_ST_CTRL_AXIS_RX][1].miscPtr==&g_binding_value_right);
    assert(map.pbuttons[BE_ST_CTRL_BUT_START].val==BE_ST_SC_ESC);
    assert(map.pbuttons[BE_ST_CTRL_BUT_BACK].val==BE_ST_SC_PAUSE);
    auto &dialog=g_ingame_altcontrol_mapping_inackback;
    assert(dialog.pbuttons[BE_ST_CTRL_BUT_A].val==BE_ST_SC_ENTER);
    assert(dialog.pbuttons[BE_ST_CTRL_BUT_B].val==BE_ST_SC_ESC);
#ifdef REFKEEN_VER_CAT3D
    assert(g_ingame_altcontrol_mapping_menu.pbuttons[BE_ST_CTRL_BUT_A].val==BE_ST_SC_ENTER);
    assert(g_ingame_altcontrol_mapping_menu.pbuttons[BE_ST_CTRL_BUT_B].val==BE_ST_SC_ESC);
    assert(g_ingame_altcontrol_mapping_menu_confirm.pbuttons[BE_ST_CTRL_BUT_A].val==BE_ST_SC_Y);
    assert(g_ingame_altcontrol_mapping_menu_confirm.pbuttons[BE_ST_CTRL_BUT_B].val==BE_ST_SC_N);
#endif
#if defined(REFKEEN_VER_CAT3D) || defined(REFKEEN_VER_CATABYSS)
    auto &layer=g_ingame_altcontrol_mapping_scrolls;
    assert(layer.pbuttons[BE_ST_CTRL_BUT_A].val==BE_ST_SC_7);
    assert(layer.pbuttons[BE_ST_CTRL_BUT_DPAD_UP].val==BE_ST_SC_1);
#else
    auto &layer=g_ingame_altcontrol_mapping_funckeys;
    assert(layer.pbuttons[BE_ST_CTRL_BUT_DPAD_UP].val==BE_ST_SC_F1);
    assert(layer.pbuttons[BE_ST_CTRL_BUT_DPAD_DOWN].val==BE_ST_SC_F3);
#endif
    assert(map.paxes[BE_ST_CTRL_AXIS_RTRIGGER][1].miscPtr==&layer);
    assert(layer.pbuttons[BE_ST_CTRL_BUT_START].val==BE_ST_SC_ESC);
    assert(layer.pbuttons[BE_ST_CTRL_BUT_BACK].val==BE_ST_SC_PAUSE);
}
