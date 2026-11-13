#!/usr/bin/env python3
"""Make the dual-screen header visible to all ReflectionHLE translation units.

Only changes platform/3ds/original/CMakeLists.txt. Safe to rerun.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
cmake = root / 'platform/3ds/original/CMakeLists.txt'
header = root / 'platform/3ds/original/cat3ds_dualscreen.h'
unit = root / 'platform/3ds/original/cat3ds_dualscreen.c'

if not cmake.is_file() or not header.is_file() or not unit.is_file():
    sys.exit('ERROR: Install the dual-screen phase 1 ZIP first; expected CMakeLists.txt and cat3ds_dualscreen.[ch].')

text = cmake.read_text(encoding='utf-8')
needle = 'target_include_directories(cat3ds-original PRIVATE\n'
addition = '  "${CMAKE_CURRENT_LIST_DIR}" # 3DS-only dual-screen header\n'
if text.count(needle) != 1:
    sys.exit('ERROR: Unexpected CMake layout; nothing changed.')
# Only consider the relevant target's include block, not other target references.
block_start = text.index(needle) + len(needle)
block_end = text.find(')', block_start)
if block_end == -1:
    sys.exit('ERROR: Missing closing target_include_directories; nothing changed.')
block = text[block_start:block_end]
if '"${CMAKE_CURRENT_LIST_DIR}"' in block:
    print('OK: native 3DS include path already configured; nothing to change.')
    sys.exit(0)
updated = text[:block_start] + addition + text[block_start:]
cmake.write_text(updated, encoding='utf-8')
print('OK: added platform/3ds/original to the engine include directories.')
