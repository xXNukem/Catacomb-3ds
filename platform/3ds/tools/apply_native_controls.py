#!/usr/bin/env python3
"""Apply physical 3DS controls and remove virtual overlays from ReflectionHLE.

Runs only on existing source files. Preflight checks all anchors before writing;
backs up every modified source file on first run. Does not touch game assets.
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
backup_root = root / 'platform/3ds/.backup-before-native-controls'
changes = {}

def replace_one(path, old, new):
    data = changes.get(path, (root/path).read_text())
    if new in data:
        changes[path] = data
        return
    if data.count(old) != 1:
        raise ValueError(f'{path}: expected exactly one anchor, got {data.count(old)}')
    changes[path] = data.replace(old,new,1)

path = 'src/backend/startup/be_startup_sdl.c'
old = '\tBEL_ST_ParseConfigFiles();\n'
new = '''\tBEL_ST_ParseConfigFiles();
#ifdef __3DS__
	/* Catacomb 3DS: physical controls, independent of legacy desktop cfg.
	 * All changes are compile-time 3DS-only; ReflectionHLE remains original. */
	g_refKeenCfg.altControlScheme = true;
	g_refKeenCfg.touchInputToggle = TOUCHINPUT_OFF;
	g_refKeenCfg.isBilinear = false; /* EGA pixels; avoid extra filtering */
	g_refKeenCfg.cat3d.useLeftStick = true;
	g_refKeenCfg.cat3d.analogMotion = false;
	g_refKeenCfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_FIRE].pad = BE_ST_CTRL_BUT_A;
	g_refKeenCfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_STRAFE].pad = BE_ST_CTRL_BUT_B;
	g_refKeenCfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_DRINK].pad = BE_ST_CTRL_BUT_X;
	g_refKeenCfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_BOLT].pad = BE_ST_CTRL_BUT_Y;
	g_refKeenCfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_NUKE].pad = BE_ST_CTRL_EXTRA_BUT_LTRIGGER;
	g_refKeenCfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_FASTTURN].pad = BE_ST_CTRL_BUT_RSHOULDER;
	g_refKeenCfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_SCROLLS].pad = BE_ST_CTRL_EXTRA_BUT_RTRIGGER;
#endif
'''
replace_one(path,old,new)

path = 'src/id91_11/cat_all/altcontroller.c'
old = '\tCheckNonKeyMappings(BE_ST_CTRL_BIND_CAT3D_DEBUGKEYS, &g_ingame_but_debug_keys_map);\n'
new = '''\tCheckNonKeyMappings(BE_ST_CTRL_BIND_CAT3D_DEBUGKEYS, &g_ingame_but_debug_keys_map);
#ifdef __3DS__
	/* Physical 3DS pad: D-pad duplicates Circle Pad, L also holds strafe,
	 * START opens the original game control panel, SELECT pauses.
	 * Use the game's existing action bindings, never synthetic key events. */
	g_ingame_altcontrol_mapping_gameplay.pbuttons[BE_ST_CTRL_BUT_DPAD_UP] = g_ingame_but_up_map;
	g_ingame_altcontrol_mapping_gameplay.pbuttons[BE_ST_CTRL_BUT_DPAD_DOWN] = g_ingame_but_down_map;
	g_ingame_altcontrol_mapping_gameplay.pbuttons[BE_ST_CTRL_BUT_DPAD_LEFT] = g_ingame_but_left_map;
	g_ingame_altcontrol_mapping_gameplay.pbuttons[BE_ST_CTRL_BUT_DPAD_RIGHT] = g_ingame_but_right_map;
	g_ingame_altcontrol_mapping_gameplay.pbuttons[BE_ST_CTRL_BUT_LSHOULDER] = g_ingame_but_strafe_map;
	g_ingame_altcontrol_mapping_gameplay.pbuttons[BE_ST_CTRL_BUT_START] = (BE_ST_ControllerSingleMap){BUT_BACK_MAP};
	g_ingame_altcontrol_mapping_gameplay.pbuttons[BE_ST_CTRL_BUT_BACK] = (BE_ST_ControllerSingleMap){BUT_PAUSE_MAP};
#endif
'''
replace_one(path,old,new)

path='src/backend/video/be_video.c'
old = '''\tg_sdlForceGfxControlUiRefresh = false;

\tif (g_refKeenCfg.touchInputDebugging)'''
new = '''\tg_sdlForceGfxControlUiRefresh = false;

#ifndef __3DS__
\t/* Native 3DS has physical controls. Never composite desktop/mobile
\t * touch pads or virtual controller textures on the game framebuffer. */
\tif (g_refKeenCfg.touchInputDebugging)'''
replace_one(path, old, new)
old = '''\t\t}
\t}

\tBEL_ST_UpdateWindow();
}

 // Text mode'''
new = '''\t\t}
\t}
#endif /* !__3DS__ */

\tBEL_ST_UpdateWindow();
}

 // Text mode'''
replace_one(path, old, new)

path='src/backend/input/be_input_sdl.c'
data=(root/path).read_text()
pattern = re.compile(r'(?P<indent>\t{4,5})g_sdlShowControllerUI = true;\n(?P=indent)g_sdlForceGfxControlUiRefresh = true;\n(?P=indent)BEL_ST_ConditionallyShowAltInputPointer\(\);\n(?P=indent)BEL_ST_CheckForHidingTouchUI\(\);')
def repl(m):
    ind=m.group('indent')
    return (f'{ind}#ifndef __3DS__\n'+
            f'{ind}g_sdlShowControllerUI = true;\n'+
            f'{ind}g_sdlForceGfxControlUiRefresh = true;\n'+
            f'{ind}BEL_ST_ConditionallyShowAltInputPointer();\n'+
            f'{ind}#endif\n'+
            f'{ind}BEL_ST_CheckForHidingTouchUI();')
updated,count=pattern.subn(repl,data)
if count == 2:
    changes[path] = updated
elif count == 0 and '#ifndef __3DS__' in data and 'g_sdlShowControllerUI = true;' in data:
    changes[path] = data
else:
    raise ValueError(f'be_input_sdl.c: expected 2 untouched controller UI sites, got {count}')

path='src/backend/input/be_input_controller_mappings.c'
old='''void BEL_ST_AltControlScheme_ConditionallyShowOnScreenControls(void)
{
	if (g_sdlControllerMappingActualCurr == &g_beStControllerMappingTextInput)'''
new='''void BEL_ST_AltControlScheme_ConditionallyShowOnScreenControls(void)
{
#ifdef __3DS__
	/* Keep controller mappings but never construct virtual control textures. */
	return;
#endif
	if (g_sdlControllerMappingActualCurr == &g_beStControllerMappingTextInput)'''
replace_one(path,old,new)

# All anchors must exist before a single source file is overwritten.
for path in changes:
    if not (root/path).is_file():
        sys.exit(f'Missing ReflectionHLE source file: {path}')

modified = []
for path, code in changes.items():
    dest = root / path
    original = dest.read_text()
    if original == code:
        continue
    backup = backup_root / path
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        backup.write_bytes(dest.read_bytes())
    dest.write_text(code)
    modified.append(path)

print('Catacomb 3DS: physical controls + no virtual overlay installed')
if modified:
    for path in modified:
        print('  updated:', path)
else:
    print('  already applied; no changes')
print('  backups:', backup_root.relative_to(root))
print('  unchanged: launcher, original game logic, renderer, game data')
