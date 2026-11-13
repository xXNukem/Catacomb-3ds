#!/usr/bin/env python3
"""3DS stereo: off-axis projection like xXNukem/bstone-3ds; modal UI is flat.

Requires existing Catacomb v3 stereo. Keeps original game resources and logic.
Atomic source edits, idempotent, with backups. GPL-2.0-or-later applies.
"""
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
DRAW = ROOT / 'src/id91_11/cat_all/cat3d/c3_draw.c'
WIZ = ROOT / 'src/id91_11/cat_all/cat3d/c3_wiz.c'
DUAL = ROOT / 'platform/3ds/original/cat3ds_dualscreen.c'
HEADER = ROOT / 'platform/3ds/original/cat3ds_dualscreen.h'
BACKUP = ROOT / 'platform/3ds/.backup-before-bstone-stereo-projection'
TAG = 'CAT3DS_BSTONE_OFFAXIS_V4'


def once(src, old, new, msg):
    count = src.count(old)
    if count != 1:
        raise RuntimeError(f'{msg}: expected exactly one source match, found {count}')
    return src.replace(old, new, 1)


def patch_draw(s):
    s = once(s,
        'fixed scale;\n',
        '''fixed scale;
#ifdef __3DS__
/* CAT3DS_BSTONE_OFFAXIS_V4: Shift the PROJECTION, not the finished EGA
 * image. Parallel eye cameras + matching per-eye ray angles avoid stretching
 * the edge columns and make disparities change correctly with distance. */
static id0_int_t cat3dsStereoProjectionShift;
#endif
''', 'Stereo projection field')
    s = once(s,
        '  *screenx = CENTERX + ny/ratio;',
        '''  *screenx = CENTERX + ny/ratio;
#ifdef __3DS__
  *screenx += cat3dsStereoProjectionShift;
#endif''', 'Wall projection')
    s = once(s,
        '  ob->viewx = CENTERX + ny/ratio;',
        '''  ob->viewx = CENTERX + ny/ratio;
#ifdef __3DS__
  ob->viewx += cat3dsStereoProjectionShift;
#endif''', 'Actor projection')

    start = s.index('\t/* CAT3DS_STEREO_CONVERGENCE_V2: Shift ONLY the world image,')
    end = s.index('\n\tEGAWRITEMODE(0);', start)
    s = s[:start] + '''\t/* CAT3DS_BSTONE_OFFAXIS_V4: No post-render image shifting. The
\t * projection, ray angles and sprite positions were adjusted together
\t * BEFORE tracing and rasterizing this eye. */
#endif
''' + s[end:]

    marker = '#ifdef __3DS__\nstatic void Cat3DS_ThreeDRefreshEye(fixed eyeOffset, int lastEye)'
    if s.count(marker) != 1:
        raise RuntimeError('Stereo-eye entry changed')
    helper = '''#ifdef __3DS__
/* This follows the approach of the native stereo code in bstone-3ds:
 * parallel cameras, lens-shifted perspective and per-eye ray angles.
 * The legacy wall tracer still receives the original game geometry. */
static void Cat3DS_SetEyeProjection(int shift)
{
    static id0_int_t angles[2][VIEWWIDTH];
    static int cachedShift[2] = {32767, 32767};
    static fixed cachedScale[2] = {-1, -1};
    const int slot = shift < 0 ? 0 : 1;
    const double fineToAngle = (double)FINEANGLES / (2.0 * PI);
    const double angleUnit = (double)ANGLES / (2.0 * PI);
    const double xCenter = (VIEWWIDTH - 1) * 0.5 + shift;
    const double projectionScale = (double)scale / (double)FOCALLENGTH;
    if (cachedShift[slot] != shift || cachedScale[slot] != scale) {
        for (int x = 0; x < VIEWWIDTH; ++x) {
            angles[slot][x] = (id0_int_t)(
                atan((xCenter - x) * projectionScale) * fineToAngle);
        }
        cachedShift[slot] = shift;
        cachedScale[slot] = scale;
    }
    memcpy(pixelangle, angles[slot], sizeof pixelangle);
    /* Trace the left and right boundaries of THIS eye's frustum. */
    firstangle = (id0_int_t)(atan((VIEWWIDTH * 0.5 + shift) *
        projectionScale) * angleUnit) + 1;
    lastangle = -((id0_int_t)(atan((VIEWWIDTH * 0.5 - shift) *
        projectionScale) * angleUnit) + 1);
    prestep = GLOBAL1 * ((double)FOCALLENGTH / costable[firstangle]);
    cat3dsStereoProjectionShift = (id0_int_t)shift;
}

/* Fixed 16.16 world offsets to signed pixels on the original 264px EGA
 * projection. Focus plane is 0.75 tile ahead of the physical player.
 * Never translate the finished EGA framebuffer; doing so copies the edge
 * columns and causes the severe visual distortion reported in v3. */
static int Cat3DS_ProjectionShift(fixed eyeOffset)
{
    if (!eyeOffset) return 0;
    const int64_t planeDistance = (int64_t)FOCALLENGTH +
        3 * (int64_t)TILEGLOBAL / 4;
    const int64_t denominator = (int64_t)scale * planeDistance;
    const int64_t absolute = eyeOffset < 0 ? -(int64_t)eyeOffset : (int64_t)eyeOffset;
    int result = denominator > 0 ? (int)((absolute * FOCALLENGTH +
        denominator / 2) / denominator) : 0;
    if (result > 12) result = 12;
    return eyeOffset < 0 ? -result : result;
}
#endif

'''
    s = s.replace(marker, helper + marker, 1)

    old = '''    if (offset > 0) {
        Cat3DS_ThreeDRefreshEye(-offset, 0);
        Cat3DS_DualScreenCaptureLeftEGA(bufferofs);
        Cat3DS_ThreeDRefreshEye(+offset, 1);
    } else {
        Cat3DS_DualScreenForgetStereo();
        Cat3DS_ThreeDRefreshEye(0, 1);
    }
    Cat3DS_DualScreenReportRenderTicks(SDL_GetPerformanceCounter() - start);'''
    new = '''    if (offset > 0) {
        /* Preserve the unshifted DOS projection for menus and other code.
         * Changing only the camera and not pixelangle/clip angles is what
         * made the earlier stereo show almost no depth at the edges. */
        id0_int_t savedAngles[VIEWWIDTH];
        memcpy(savedAngles, pixelangle, sizeof savedAngles);
        const id0_int_t savedFirst = firstangle, savedLast = lastangle;
        const fixed savedPrestep = prestep;
        Cat3DS_SetEyeProjection(Cat3DS_ProjectionShift(-offset));
        Cat3DS_ThreeDRefreshEye(-offset, 0);
        Cat3DS_DualScreenCaptureLeftEGA(bufferofs);
        Cat3DS_SetEyeProjection(Cat3DS_ProjectionShift(+offset));
        Cat3DS_ThreeDRefreshEye(+offset, 1);
        memcpy(pixelangle, savedAngles, sizeof savedAngles);
        firstangle = savedFirst;
        lastangle = savedLast;
        prestep = savedPrestep;
        cat3dsStereoProjectionShift = 0;
    } else {
        cat3dsStereoProjectionShift = 0;
        Cat3DS_DualScreenForgetStereo();
        Cat3DS_ThreeDRefreshEye(0, 1);
    }
    Cat3DS_DualScreenReportRenderTicks(SDL_GetPerformanceCounter() - start);'''
    s = once(s, old, new, 'Eye projection switching')
    return s


def patch_dual(s):
    s = once(s, ' * At max slider each eye moves by 1/16 tile. Camera angles remain parallel. */',
        ' * At max slider each eye moves by ~0.0234 tile. Cameras remain parallel. */',
        'Stereo document')
    start = s.index('    /* CAT3DS_STEREO_WINDOW_V3: max 1/16 tile per eye.')
    end = s.index('\n}', start)
    s = s[:start] + '''    /* CAT3DS_BSTONE_OFFAXIS_V4: moderate baseline with true
     * off-axis ray projection, not the excessive v3 image shift. */
    return (int)(clamped * 1536.0f + 0.5f);''' + s[end:]
    start = s.index('/* CAT3DS_STEREO_WINDOW_V3\n * Shift the original EGA WORLD')
    end = s.index('void Cat3DS_DualScreenForgetStereo(void)', start)
    s = s[:start] + '''/* CAT3DS_BSTONE_OFFAXIS_V4: Convergence belongs to the render
 * projection. The old memmove of EGA columns was removed entirely. */
static int stereoModalActive;
void Cat3DS_DualScreenSetModalOverlay(int active)
{
    stereoModalActive = !!active;
    if (active) stereoLeftValid = 0;
}
''' + s[end:]
    s = once(s,
        '    const int wantStereo = stereoLeftValid && osGet3DSliderState() > 0.025f;',
        '    const int wantStereo = !stereoModalActive && stereoLeftValid && osGet3DSliderState() > 0.025f;',
        'Modal stereo mask')
    s = once(s,
        '    if (!enabled) {\n        gfxSet3D(false);',
        '    if (!enabled) {\n        stereoModalActive = 0;\n        gfxSet3D(false);',
        'Reset overlay flag on menu transition')
    return s


def patch_header(s):
    s = once(s,
        '''/* Off-axis convergence, invoked between world and hand rendering. */
void Cat3DS_DualScreenConvergeWorld(uint16_t egaPage, int32_t signedShiftPixels);''',
        '''/* 2D overlays (scrolls, modal text) must not use stale stereo eyes. */
void Cat3DS_DualScreenSetModalOverlay(int active);''',
        'Stereo interface')
    return s


def patch_wiz(s):
    s = once(s, '#include "c3_def.h"\n',
        '#include "c3_def.h"\n#ifdef __3DS__\n#include "cat3ds_dualscreen.h"\n#endif\n',
        'Modal include')
    s = once(s,
        '''\tCA_CacheGrChunk (SCROLLTOPPIC);
\tCA_CacheGrChunk (SCROLL1PIC + scroll);''',
        '''#ifdef __3DS__
    /* The scroll is a 2D modal document, NOT part of either eye's world.
     * Prevent the previous left-eye world image from being composited
     * with the new right-eye scroll and causing painful double vision. */
    Cat3DS_DualScreenSetModalOverlay(1);
#endif
\tCA_CacheGrChunk (SCROLLTOPPIC);
\tCA_CacheGrChunk (SCROLL1PIC + scroll);''',
        'Scroll starts in flat mode')
    s = once(s,
        '''\tIN_Ack();
\t// REFKEEN - Alternative controllers support
\tBE_ST_AltControlScheme_Pop();''',
        '''\tIN_Ack();
\t// REFKEEN - Alternative controllers support
\tBE_ST_AltControlScheme_Pop();
#ifdef __3DS__
    Cat3DS_DualScreenSetModalOverlay(0);
#endif''',
        'Scroll restores stereo mode')
    return s


try:
    files = (DRAW, WIZ, DUAL, HEADER)
    for file in files:
        if not file.is_file():
            raise RuntimeError(f'Missing file: {file.relative_to(ROOT)}')
    sources = {file: file.read_text(encoding='utf-8') for file in files}
    tags = [TAG in sources[DRAW], TAG in sources[DUAL]]
    if all(tags):
        print('BStone-style projection and modal protection already installed.')
        sys.exit(0)
    if any(tags):
        raise RuntimeError('Partial installation detected; source files untouched')
    if 'CAT3DS_STEREO_WINDOW_V3' not in sources[DRAW] or 'CAT3DS_STEREO_WINDOW_V3' not in sources[DUAL]:
        raise RuntimeError('Requires the existing Stereo Window v3 patch; source files untouched')
    updated = {
        DRAW: patch_draw(sources[DRAW]),
        WIZ: patch_wiz(sources[WIZ]),
        DUAL: patch_dual(sources[DUAL]),
        HEADER: patch_header(sources[HEADER]),
    }
    # Validate every replacement BEFORE changing any source file.
    for file, content in updated.items():
        if content == sources[file]:
            raise RuntimeError(f'No change detected: {file.relative_to(ROOT)}')
    for file in files:
        save = BACKUP / file.relative_to(ROOT)
        save.parent.mkdir(parents=True, exist_ok=True)
        if not save.exists():
            shutil.copy2(file, save)
    # Make temporary files before replacing originals.
    for file, content in updated.items():
        staging = file.with_suffix(file.suffix + '.bs-stereo-new')
        staging.write_text(content, encoding='utf-8')
    for file in files:
        file.with_suffix(file.suffix + '.bs-stereo-new').replace(file)
    print('Installed: per-eye off-axis projection and ray tables, no image stretching.')
    print('Scroll modal displays in 2D; 3D resumes on the next real scene frame.')
    print('BStone stereo reference: xXNukem/bstone-3ds/src/bstone/src/3d_draw.cpp')
except Exception as exc:
    print('ERROR: ' + str(exc), file=sys.stderr)
    sys.exit(1)
