#!/usr/bin/env python3
"""Fix lost 3DS key presses in the SDL3 Catacomb 3DS selector.

SDL3 N3DS_PumpEvents calls hidScanInput(), so hidKeysDown() may no longer
contain the edge if the launcher scans again. Derive edges from held state.
Only changes the input handling of cat3ds_sdl_launcher.cpp.
"""
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
target = root / 'platform/3ds/original/cat3ds_sdl_launcher.cpp'
if not target.is_file():
    sys.exit(f'ERROR: expected menu source not found: {target}')

source = target.read_text(encoding='utf-8')
old_state = '    int selectionResult = -1;\n\n    while (aptMainLoop())'
new_state = '    int selectionResult = -1;\n    u32 previousHeld = 0;\n\n    while (aptMainLoop())'
old_keys = '        hidScanInput();\n        const u32 keys = hidKeysDown();'
new_keys = '''        /* SDL3's Nintendo 3DS event pump already scans HID input.
         * Repeated scans can clear hidKeysDown() before it is read.
         * Held input persists; derive just-pressed transitions here. */
        hidScanInput();
        const u32 held = hidKeysHeld();
        const u32 keys = held & ~previousHeld;
        previousHeld = held;'''

if new_state in source and new_keys in source:
    print('SDL3 menu input: already fixed. No changes needed.')
    sys.exit(0)

if source.count(old_state) != 1 or source.count(old_keys) != 1:
    sys.exit('ERROR: unexpected menu source version. No files modified. Please share platform/3ds/original/cat3ds_sdl_launcher.cpp for review.')

modified = source.replace(old_state, new_state).replace(old_keys, new_keys)
backup = target.with_suffix('.cpp.bak-before-input-fix')
if not backup.exists():
    shutil.copy2(target, backup)
target.write_text(modified, encoding='utf-8')
print('SDL3 menu input fixed: hidKeysHeld() with independent key-down detection.')
print(f'Backup: {backup.relative_to(root)}')
