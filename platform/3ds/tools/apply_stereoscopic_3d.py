#!/usr/bin/env python3
"""Native New 3DS true stereo for ReflectionHLE Catacomb 3-D.

Two complete original renderer passes per gameplay frame, with parallel-camera
views and ONLY ONE game-clock update. An eye-buffer worker on core 2 still
converts both views to physical top LCD framebuffers. Reversible/strict patch.
"""
from pathlib import Path
import hashlib
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
DRAW = ROOT / 'src/id91_11/cat_all/cat3d/c3_draw.c'
DUAL = ROOT / 'platform/3ds/original/cat3ds_dualscreen.c'
HEADER = ROOT / 'platform/3ds/original/cat3ds_dualscreen.h'
ASSET = ROOT / 'platform/3ds/tools/assets/cat3ds_dualscreen_stereo.c'
BACKUP = ROOT / 'platform/3ds/.backup-before-true-stereo'
MARKER = 'CAT3DS_REAL_STEREO_V1'
EXPECTED_COMPOSITOR = '6e33fe44a7c315d7575b16ec1b16748c54531b8eabb43831813dacc744465304'


def replace_one(s, old, new, label):
    count = s.count(old)
    if count != 1:
        raise RuntimeError(f'{label}: expected exactly one anchor, found {count}. No files changed.')
    return s.replace(old, new, 1)


def make_draw(s):
    # In the regular gameplay path the original engine only calls ThreeDRefresh
    # once; this wrapper invokes the actual renderer twice ONLY for 3D LCD mode.
    s = replace_one(s,
        'void\tThreeDRefresh (void)\n{\n\tid0_int_t tracedir;\n#ifdef __3DS__\n'
        '\t/* CAT3DS_3D_RENDER_TIMING_V2: original wall and sprite pass only. */\n'
        '\tconst uint64_t cat3dsRenderStart = SDL_GetPerformanceCounter();\n#endif',
        '#ifdef __3DS__\n'
        'static void Cat3DS_ThreeDRefreshEye(fixed eyeOffset, int lastEye)\n'
        '#else\n'
        'void ThreeDRefresh (void)\n'
        '#endif\n'
        '{\n\tid0_int_t tracedir;', 'Original ThreeDRefresh function and profiling')
    s = replace_one(s,
        '\tviewx = player->x - FixedByFrac(FOCALLENGTH,viewcos);\n'
        '\tviewy = player->y + FixedByFrac(FOCALLENGTH,viewsin);',
        '\tviewx = player->x - FixedByFrac(FOCALLENGTH,viewcos);\n'
        '\tviewy = player->y + FixedByFrac(FOCALLENGTH,viewsin);\n'
        '#ifdef __3DS__\n'
        '\t/* Parallel eye cameras: same angle, physically separated viewpoints.\n'
        '\t * No toe-in/rotation, no gameplay position changes.\n'
        '\t * Suppress an eye shift that would put the original ray origin\n'
        '\t * inside a solid tile or outside the original level. */\n'
        '\tconst fixed ex = viewx + FixedByFrac(eyeOffset, viewsin);\n'
        '\tconst fixed ey = viewy + FixedByFrac(eyeOffset, viewcos);\n'
        '\tconst int etx = ex >> TILESHIFT, ety = ey >> TILESHIFT;\n'
        '\tif (etx >= 0 && etx < mapwidth && ety >= 0 && ety < mapheight &&\n'
        '\t    !tilemap[etx][ety]) { viewx = ex; viewy = ey; }\n'
        '#endif', 'Original view transform')
    s = replace_one(s,
        '\tDrawWallList();\n\tDrawScaleds();\n#ifdef __3DS__\n'
        '\tCat3DS_DualScreenReportRenderTicks(SDL_GetPerformanceCounter() - cat3dsRenderStart);\n#endif',
        '\tDrawWallList();\n\tDrawScaleds();', 'Original renderer profiling')
    s = replace_one(s,
        '\tif (fizzlein)\n\t{\n',
        '#ifdef __3DS__\n\tif (fizzlein && lastEye)\n#else\n\tif (fizzlein)\n#endif\n\t{\n',
        'Fade effect (only once per frame)')
    s = replace_one(s,
        '#ifdef __3DS__\n\tCat3DS_DualScreenObserve(gamestate.mapon, mapwidth, mapheight,\n'
        '\t\t&tilemap[0][0], &spotvis[0][0], player->x, player->y, player->angle);\n'
        '#endif\n\tBE_ST_SetScreenStartAddress(bufferofs);',
        '#ifdef __3DS__\n\tif (lastEye)\n\t\tCat3DS_DualScreenObserve(gamestate.mapon, mapwidth, mapheight,\n'
        '\t\t\t&tilemap[0][0], &spotvis[0][0], player->x, player->y, player->angle);\n'
        '\tif (lastEye)\n#endif\n\tBE_ST_SetScreenStartAddress(bufferofs);',
        'Autmap observation and EGA presenting')
    s = replace_one(s,
        '\tdisplayofs = bufferofs;\n\n\tCalcTics ();',
        '#ifdef __3DS__\n\tif (lastEye) {\n#endif\n'
        '\tdisplayofs = bufferofs;\n\n\tCalcTics ();\n'
        '#ifdef __3DS__\n\t}\n#endif',
        'Original clock update only once')
    s = replace_one(s,
        '\nREFKEEN_NS_E\n',
        '''
#ifdef __3DS__
/* CAT3DS_REAL_STEREO_V1
 * Render the ORIGINAL Catacomb scene from both eyes, with an unchanged
 * player and unchanged animation time. Left-eye pixels are captured from
 * the original EGA video page; only the final (right) eye updates the
 * screen-start address and the DOS game timer. Slider=0 is native 2D. */
void ThreeDRefresh(void)
{
    const uint64_t start = SDL_GetPerformanceCounter();
    const int offset = Cat3DS_DualScreenStereoOffset();
    if (offset > 0) {
        Cat3DS_ThreeDRefreshEye(-offset, 0);
        Cat3DS_DualScreenCaptureLeftEGA(bufferofs);
        Cat3DS_ThreeDRefreshEye(+offset, 1);
    } else {
        Cat3DS_DualScreenForgetStereo();
        Cat3DS_ThreeDRefreshEye(0, 1);
    }
    Cat3DS_DualScreenReportRenderTicks(SDL_GetPerformanceCounter() - start);
}
#endif

REFKEEN_NS_E
''', 'Stereo wrapper end of original c3_draw')
    return s


def make_header(s):
    return replace_one(s,
        'void Cat3DS_DualScreenReportRenderTicks(uint64_t ticks);',
        'void Cat3DS_DualScreenReportRenderTicks(uint64_t ticks);\n'
        '/* Real left/right 3D views using two passes of original ThreeDRefresh. */\n'
        'int Cat3DS_DualScreenStereoOffset(void);\n'
        'void Cat3DS_DualScreenCaptureLeftEGA(uint16_t egaPage);\n'
        'void Cat3DS_DualScreenForgetStereo(void);',
        'Stereo API in header')

try:
    for p in (DRAW, DUAL, HEADER, ASSET):
        if not p.is_file():
            raise RuntimeError('Missing required source: ' + str(p.relative_to(ROOT)))
    current_compositor = DUAL.read_bytes()
    new_compositor = ASSET.read_bytes()
    if MARKER.encode() not in new_compositor or b'gfxScreenSwapBuffers(GFX_TOP, stereo)' not in new_compositor:
        raise RuntimeError('Damaged stereo compositor asset; no files changed.')
    if MARKER.encode() not in current_compositor:
        if hashlib.sha256(current_compositor).hexdigest() != EXPECTED_COMPOSITOR:
            raise RuntimeError('Compositor differs from last working multicore patch. No files changed.')
    elif current_compositor != new_compositor:
        raise RuntimeError('Stereo compositor was modified locally. No files changed.')

    draw = DRAW.read_text('utf-8')
    header = HEADER.read_text('utf-8')
    if MARKER not in draw:
        draw = make_draw(draw)
    if 'Cat3DS_DualScreenStereoOffset(void);' not in header:
        header = make_header(header)

    updates = {DRAW: draw.encode(), DUAL: new_compositor, HEADER: header.encode()}
    changed = [(p, value) for p, value in updates.items() if p.read_bytes() != value]
    # Preflight complete before any user source is modified.
    for p, value in changed:
        backup = BACKUP / p.relative_to(ROOT)
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(p, backup)
    for p, value in changed:
        tmp = p.with_suffix(p.suffix + '.stereo-new')
        tmp.write_bytes(value)
        tmp.replace(p)
    print('Installed true New3DS stereoscopic Catacomb 3-D renderer:', len(changed), 'files changed')
    print('LCD slider controls separation; slider=0 retains one-pass rendering.')
    print('Core 2 continues to convert top EGA video; lower HUD/map and game run on core 0.')
    print('Only one clock update and one display presentation per complete stereo frame.')
    print('Existing menu, controls, audio, saves, game files and launcher retained.')
except Exception as exc:
    print('ERROR:', exc, file=sys.stderr)
    sys.exit(1)
