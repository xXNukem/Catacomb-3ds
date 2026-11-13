#!/usr/bin/env python3
"""Use ReflectionHLE's original scale tables with native 3DS wall writes.

This patch edits only src/id91_11/cat_all/c3_asm.c, and creates a
portable wall-scaler header in platform/3ds/original. Other platforms keep
ScaleWalls() byte-for-byte in the #else branch. No game data required.
"""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
target = root / 'src/id91_11/cat_all/c3_asm.c'
header = root / 'platform/3ds/original/cat3ds_native_wall_scaler.h'
cmake = root / 'platform/3ds/original/CMakeLists.txt'
cmake_backup = root / 'platform/3ds/.backup-before-native-wall-scaler/platform/3ds/original/CMakeLists.txt'
entry = root / 'platform/3ds/original/refkeen_original_main.c'
entry_backup = root / 'platform/3ds/.backup-before-native-wall-scaler/platform/3ds/original/refkeen_original_main.c'
backup = root / 'platform/3ds/.backup-before-native-wall-scaler/src/id91_11/cat_all/c3_asm.c'
text = target.read_text()
marker = 'CAT3DS_NATIVE_WALL_SCALER_V1'
if marker in text:
    print('Native wall scaler already installed; no changes.')
    raise SystemExit(0)

cmake_text = cmake.read_text()
assert 'target_include_directories(cat3ds-original PRIVATE' in cmake_text, 'Unsupported engine CMake file'


# The source file is identical across the user's working branch and the
# original source snapshot; check distinctive anchors before writing.
start_tag = 'void ScaleWalls(void)\n{\n'
end_tag = '\n#if 0\nPROC\tScaleWalls'
assert text.count(start_tag) == 1, 'Unrecognized ScaleWalls implementation'
assert text.count(end_tag) == 1, 'Missing original ASM documentation after ScaleWalls'
assert text.count('REFKEEN_NS_B') == 1, 'Unexpected namespace markers'

prelude = '''/* CAT3DS_NATIVE_WALL_SCALER_V1: draw ReflectionHLE walls without the
 * interpreted DOS x86 scaler. Only compiled on the Nintendo 3DS target. */
#ifdef __3DS__
#include "cat3ds_native_wall_scaler.h"
#include "be_st_egavga_lookup_tables.h"
#include "backend/video/be_video_emu.h"
#include "backend/video/be_video.h"
extern const id0_unsigned_t screenbwide;
static Cat3DS_WallRowMap cat3ds_wall_rows[MAXSCALE + 1];
static const t_compscale *cat3ds_cached_scalers[MAXSCALE + 1];
#endif

'''
# Insert just before 'void ScaleWalls', leaving the original preserved.
where = text.index(start_tag)
text = text[:where] + prelude + text[where:]

begin = text.index(start_tag) + len(start_tag)
end = text.index(end_tag, begin)
original_body = text[begin:end]
assert 'ExecuteCompScale(' in original_body and 'wallwidth[colNum]' in original_body
assert original_body.endswith('}\n'), 'ScaleWalls end changed unexpectedly'

native = '''#ifdef __3DS__
    unsigned colNum = 0;
    while (colNum < VIEWWIDTH) {
        const unsigned pixwidth = wallwidth[colNum];
        if (!pixwidth) { ++colNum; continue; }
        /* Identical wall/offset selection and vanilla bug workaround. */
        const unsigned scale = wallheight[colNum];
        if (scale > MAXSCALE || !scaledirectory[scale]) {
            colNum += pixwidth;
            continue;
        }
        const t_compscale *scaler = scaledirectory[scale];
        if (cat3ds_cached_scalers[scale] != scaler) {
            Cat3DS_BuildWallRows(&cat3ds_wall_rows[scale],
                scaler->start, scaler->width, (int)scale * 2, VIEWHEIGHT);
            cat3ds_cached_scalers[scale] = scaler;
        }
        const uint8_t *wallSrcPtr =
            (wallseg[colNum] ? wallseg[colNum] : g_be_cross_dosZeroSeg) + wallofs[colNum];
        const unsigned egaDestOff = screenbyte[colNum] + bufferofs;
        const unsigned masksPair = bitmasks[screenbit[colNum]/2 + pixwidth - 1];
        Cat3DS_DrawWallRows(&cat3ds_wall_rows[scale],
            g_sdlVidMem.A000.egaGfx, egaDestOff, screenbwide,
            wallSrcPtr, (uint8_t)masksPair, (uint8_t)(masksPair >> 8),
            g_be_st_lookup_egaplane_repeat,
            g_be_st_lookup_egaplane_bitsmask);
        colNum += pixwidth;
    }
    g_sdlDoRefreshGfxOutput = true;
#else
'''

# Include original code as-is in else branch, without its final closing brace.
new_body = native + original_body[:-2] + '#endif /* __3DS__ */\n}\n'
text = text[:begin] + new_body + text[end:]

# Fail safely: back up once and write atomically after all assertions.
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists(): shutil.copy2(target, backup)
newfile = target.with_suffix('.c.cat3ds-new')
newfile.write_text(text)
newfile.replace(target)
assert header.exists(), 'Native wall scaler header missing'
if '"${CMAKE_CURRENT_LIST_DIR}" # CAT3DS_NATIVE_WALL_SCALER_V1' not in cmake_text:
    cmake_backup.parent.mkdir(parents=True, exist_ok=True)
    if not cmake_backup.exists(): shutil.copy2(cmake, cmake_backup)
    cmake.write_text(cmake_text.replace(
        'target_include_directories(cat3ds-original PRIVATE\n',
        'target_include_directories(cat3ds-original PRIVATE\n  "${CMAKE_CURRENT_LIST_DIR}" # CAT3DS_NATIVE_WALL_SCALER_V1\n', 1))
# SDL3 closes and reinitializes its platform services between the EGA game
# selector and the actual ReflectionHLE engine. Reassert the New3DS 804MHz/L2
# request after the selector; earlier patches already request it before the UI.
main_text = entry.read_text()
boost_marker = 'CAT3DS_BOOST_AFTER_LAUNCHER_V1'
if boost_marker not in main_text:
    assert 'osSetSpeedupEnable(true);' in main_text, 'Expected existing New3DS boost request not found'
    anchor = '    if (cat3dsSelectGame() != 0)\n        return 0;'
    assert anchor in main_text, 'SDL3-to-engine transition not found'
    entry_backup.parent.mkdir(parents=True, exist_ok=True)
    if not entry_backup.exists(): shutil.copy2(entry, entry_backup)
    entry.write_text(main_text.replace(anchor, anchor +
        '\n    /* ' + boost_marker + ': renew 804MHz/L2 request after SDL3 launcher exits. */\n'
        '    osSetSpeedupEnable(true);', 1))
print('Installed native 3DS wall scaler and reaffirmed New3DS CPU boost after the selector.')
