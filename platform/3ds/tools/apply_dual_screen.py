#!/usr/bin/env python3
"""Install the 3DS dual-screen HUD + automap over the real ReflectionHLE engine.

Fails without modifying anything if this repository has unexpected source layout.
Preserves game files, launcher, native controls, and native saves.
"""
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
HEADERS = {
    'src/id91_11/cat_all/cat3d/c3_game.c': (
        '#include "c3_def.h"',
        '#include "c3_def.h"\n#ifdef __3DS__\n#include "cat3ds_dualscreen.h"\n#endif',
    ),
    'src/id91_11/cat_all/cat3d/c3_draw.c': (
        '#include "c3_def.h"',
        '#include "c3_def.h"\n#ifdef __3DS__\n#include "cat3ds_dualscreen.h"\n#endif',
    ),
    'src/backend/video/be_video.c': (
        '#include "be_video_ui.h"',
        '#include "be_video_ui.h"\n#ifdef __3DS__\n#include "cat3ds_dualscreen.h"\nextern bool BEL_ST_RenderNative3DSViewport(BE_ST_Texture *texture);\n#endif',
    ),
}
MARKER = '/* CAT3DS_DUALSCREEN_V1 */'

edits = {}
def replace_once(rel, old, new):
    p = ROOT / rel
    s = edits.get(p, p.read_text(encoding='utf-8'))
    if new in s:
        edits[p] = s
        return
    count = s.count(old)
    if count != 1:
        raise RuntimeError(f'{rel}: expected one insertion point, found {count}: {old[:95]!r}')
    edits[p] = s.replace(old, new, 1)

try:
    for rel, (old, new) in HEADERS.items():
        replace_once(rel, old, new)

    replace_once('src/id91_11/cat_all/cat3d/c3_game.c',
                 'void NormalScreen (void)\n{',
                 'void NormalScreen (void)\n{\n#ifdef __3DS__\n\tCat3DS_DualScreenSetGameplay(0);\n#endif')
    replace_once('src/id91_11/cat_all/cat3d/c3_game.c',
                 'void DrawPlayScreen (void)\n{',
                 'void DrawPlayScreen (void)\n{\n#ifdef __3DS__\n\tCat3DS_DualScreenSetGameplay(1);\n#endif')
    replace_once('src/id91_11/cat_all/cat3d/c3_game.c',
                 '\tCA_LoadAllSounds ();\n\n}',
                 '\tCA_LoadAllSounds ();\n#ifdef __3DS__\n\tCat3DS_DualScreenBeginLevel(gamestate.mapon, mapwidth, mapheight);\n#endif\n\n}')
    replace_once('src/id91_11/cat_all/cat3d/c3_draw.c',
                 '\tBE_ST_SetScreenStartAddress(bufferofs);',
                 '#ifdef __3DS__\n\tCat3DS_DualScreenObserve(gamestate.mapon, mapwidth, mapheight,\n\t\t&tilemap[0][0], &spotvis[0][0], player->x, player->y, player->angle);\n#endif\n\tBE_ST_SetScreenStartAddress(bufferofs);')

    rel = 'src/backend/video/be_video.c'
    old = '\tBEL_ST_SetDrawColor(0xFF000000);\n\tBEL_ST_RenderClear();\n\tBEL_ST_SetDrawColor(g_sdlEGALastBGRABorderColor);'
    new = ('\tBEL_ST_SetDrawColor(0xFF000000);\n\tBEL_ST_RenderClear();\n'
           '#ifdef __3DS__\n'
           '\t/* In gameplay, crop the original 3D viewport (x0..263/y0..143)\n'
           '\t * away from the ORIGINAL EGA status art. No HUD appears above.\n'
           '\t * The camera FOV itself remains original until its dedicated\n'
           '\t * widescreen-renderer patch (no raycaster is introduced here). */\n'
           '\tif (g_sdlScreenMode == 0x0D && Cat3DS_DualScreenHasGameplayFrame())\n'
           '\t{\n'
           '\t\tBEL_ST_RenderNative3DSViewport(g_sdlTexture);\n'
           '\t\tCat3DS_DualScreenPresent(g_sdlHostScrMem.egaGfx, g_sdlTexWidth,\n'
           '\t\t                           g_sdlEGACurrBGRAPalette);\n'
           '\t\tBEL_ST_FinishHostDisplayUpdate();\n'
           '\t\tg_be_sdlLastRefreshTicks = BEL_ST_GetTicksMS();\n'
           '\t\treturn;\n'
           '\t}\n'
           '#endif\n'
           '\tBEL_ST_SetDrawColor(g_sdlEGALastBGRABorderColor);')
    replace_once(rel, old, new)

    replace_once('src/backend/video/be_video_sdl.c',
                 'bool BEL_ST_RenderFromTexture(BE_ST_Texture *texture, const BE_ST_Rect *dst)',
                 '#ifdef __3DS__\n'
                 'bool BEL_ST_RenderNative3DSViewport(BE_ST_Texture *texture)\n'
                 '{\n'
                 '\t/* Original Catacomb 3-D 264x144 camera only; direct crop of the\n'
                 '\t * real ReflectionHLE texture, no reconstructed rendering. */\n'
                 '\tconst SDL_FRect source = {0.0f, 0.0f, 264.0f, 144.0f};\n'
                 '\tint outWidth = 400, outHeight = 240;\n'
                 '\tSDL_GetCurrentRenderOutputSize(g_sdlRenderer, &outWidth, &outHeight);\n'
                 '\tconst SDL_FRect destination = {0.0f, 0.0f, (float)outWidth, (float)outHeight};\n'
                 '\treturn SDL_RenderTexture(g_sdlRenderer, (SDL_Texture *)texture,\n'
                 '\t                         &source, &destination);\n'
                 '}\n'
                 '#endif\n\n'
                 'bool BEL_ST_RenderFromTexture(BE_ST_Texture *texture, const BE_ST_Rect *dst)')
    replace_once('platform/3ds/original/CMakeLists.txt',
                 '  "${CMAKE_CURRENT_LIST_DIR}/cat3ds_sdl_launcher.cpp"',
                 '  "${CMAKE_CURRENT_LIST_DIR}/cat3ds_sdl_launcher.cpp"\n'
                 '  "${CMAKE_CURRENT_LIST_DIR}/cat3ds_dualscreen.c"')

    # Ensure file layout exists before touching user files.
    for rel in ['platform/3ds/original/cat3ds_dualscreen.c',
                'platform/3ds/original/cat3ds_dualscreen.h']:
        if not (ROOT / rel).is_file():
            raise RuntimeError('Missing patch asset: ' + rel)

    changed = {p: new for p, new in edits.items() if p.read_text(encoding='utf-8') != new}
    for p in changed:
        backup = p.with_name(p.name + '.backup-pre-dualscreen')
        if not backup.exists():
            shutil.copy2(p, backup)
    for p, content in changed.items():
        p.write_text(content, encoding='utf-8')
    print(f'Dual-screen patch applied: {len(changed)} files updated.')
    print('Original engine, Catacomb 3DS selector, input and saves unchanged.')
    print('NOTE: 400x240 output uses the existing camera FOV; true widescreen FOV is a subsequent engine change.')
except Exception as e:
    print('ERROR (no changes applied):', e, file=sys.stderr)
    sys.exit(1)
