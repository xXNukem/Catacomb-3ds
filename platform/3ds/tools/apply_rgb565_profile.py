#!/usr/bin/env python3
"""Catacomb 3DS native video optimisation #2 + engine/frame profiler.

Applies to the confirmed native-video-fast installation, never to the
preview-raycaster build. Validates every edit BEFORE writing any source file.
"""
from pathlib import Path
import hashlib
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
DUAL = ROOT / 'platform/3ds/original/cat3ds_dualscreen.c'
ASSET = ROOT / 'platform/3ds/tools/assets/cat3ds_dualscreen_rgb565.c'
HEADER = ROOT / 'platform/3ds/original/cat3ds_dualscreen.h'
DRAW = ROOT / 'src/id91_11/cat_all/cat3d/c3_draw.c'
VIDEO = ROOT / 'src/backend/video/be_video.c'
BACKUP = ROOT / 'platform/3ds/.backup-before-rgb565-profile'
EXPECTED_V1 = 'afdc1d492b52898e2ef33d344c8fdadd5da3ce8a19d7efc7fddfe9127a7d85d2'
MARKER = 'CAT3DS_RGB565_PROFILE_V2'

def replace_once(source, old, new, name):
    if new in source:
        return source
    count = source.count(old)
    if count != 1:
        raise RuntimeError(f'{name}: expected one insertion point, got {count}')
    return source.replace(old, new, 1)

try:
    for path in (DUAL, ASSET, HEADER, DRAW, VIDEO):
        if not path.is_file():
            raise RuntimeError('Missing ' + str(path.relative_to(ROOT)))

    new_dual = ASSET.read_text('utf-8')
    dual = DUAL.read_text('utf-8')
    if MARKER not in new_dual or 'GSP_RGB565_OES' not in new_dual:
        raise RuntimeError('Damaged or incorrect RGB565 asset')
    if MARKER not in dual:
        if hashlib.sha256(DUAL.read_bytes()).hexdigest() != EXPECTED_V1:
            raise RuntimeError('Native-video-fast source differs from expected version; files unchanged')
    elif dual != new_dual:
        raise RuntimeError('RGB565 compositor already edited locally; files unchanged')

    header = HEADER.read_text('utf-8')
    header = replace_once(header,
        'int Cat3DS_DualScreenHasGameplayFrame(void);',
        'int Cat3DS_DualScreenHasGameplayFrame(void);\n'
        '/* Time spent in the real ReflectionHLE 3D renderer (timer ticks). */\n'
        'void Cat3DS_DualScreenReportRenderTicks(uint64_t ticks);',
        'cat3ds_dualscreen.h')

    draw = DRAW.read_text('utf-8')
    if 'Cat3DS_DualScreenObserve(' not in draw:
        raise RuntimeError('Original 3D renderer is not connected to the dual-screen backend')
    draw = replace_once(draw,
        '#include "cat3ds_dualscreen.h"',
        '#include "cat3ds_dualscreen.h"\n#include <SDL3/SDL_timer.h>',
        'c3_draw.c SDL timer include')
    draw = replace_once(draw,
        '\tid0_int_t tracedir;\n\nrestart:',
        '\tid0_int_t tracedir;\n'
        '#ifdef __3DS__\n'
        '\t/* CAT3DS_3D_RENDER_TIMING_V2: original wall and sprite pass only. */\n'
        '\tconst uint64_t cat3dsRenderStart = SDL_GetPerformanceCounter();\n'
        '#endif\n\nrestart:',
        'c3_draw.c ThreeDRefresh')
    draw = replace_once(draw,
        '\tDrawWallList();\n\tDrawScaleds();',
        '\tDrawWallList();\n\tDrawScaleds();\n'
        '#ifdef __3DS__\n'
        '\tCat3DS_DualScreenReportRenderTicks(SDL_GetPerformanceCounter() - cat3dsRenderStart);\n'
        '#endif',
        'c3_draw.c original wall/sprite draw')

    video = VIDEO.read_text('utf-8')
    if 'CAT3DS_NATIVE_EGA_UPLOAD_SKIP_V1' not in video or 'CAT3DS_NATIVE_EGA_PRESENT_V1' not in video:
        raise RuntimeError('Previous native-video-fast patch missing from be_video.c')
    # V1 still compares and caches the full EGA screen every frame before
    # bypassing SDL3. Fast path may return immediately after the EGA copy.
    video = replace_once(video,
        '\tbool doUpdate = memcmp(g_sdlHostScrMemCache.egaGfx, g_sdlHostScrMem.egaGfx, sizeof(g_sdlHostScrMem.egaGfx));',
        '#ifdef __3DS__\n'
        '\t/* CAT3DS_NATIVE_EGA_READ_FAST_V2: avoid full EGA cache compare\n'
        '\t * and copy during gameplay; menu path remains unchanged. */\n'
        '\tif (g_sdlScreenMode == 0x0D && Cat3DS_DualScreenHasGameplayFrame())\n'
        '\t{\n'
        '\t\tg_sdlDoRefreshGfxOutput = false;\n'
        '\t\treturn true;\n'
        '\t}\n'
        '#endif\n'
        '\tbool doUpdate = memcmp(g_sdlHostScrMemCache.egaGfx, g_sdlHostScrMem.egaGfx, sizeof(g_sdlHostScrMem.egaGfx));',
        'be_video.c EGA cache')

    changed = {}
    if dual != new_dual:
        changed[DUAL] = new_dual
    for path, value in ((HEADER, header), (DRAW, draw), (VIDEO, video)):
        if path.read_text('utf-8') != value:
            changed[path] = value
    for path in changed:
        dest = BACKUP / path.relative_to(ROOT)
        dest.parent.mkdir(parents=True, exist_ok=True)
        if not dest.exists():
            shutil.copy2(path, dest)
    for path, value in changed.items():
        path.write_text(value, 'utf-8')
    print(f'RGB565 and real-frame profiler installed: {len(changed)} source files changed')
    print('Gameplay output: 16bpp, skipping redundant indexed framebuffer comparisons')
    print('Bottom HUD: FPS, R(renderer ms), V(video ms), P(max frame gap), T(top), B(bottom)')
    print('Launcher, game logic, automap, saves and input preserved')
except Exception as exc:
    print('ERROR (no files modified):', exc, file=sys.stderr)
    sys.exit(1)
