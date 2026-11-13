#!/usr/bin/env python3
"""Catacomb 3DS: correct physical eye offsets and geometry-based stereo convergence.

Applies only on top of the existing true-stereo + convergence patches.
No game assets, menu controls, game saves, or core scheduling are changed.
"""
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
DRAW = ROOT / 'src/id91_11/cat_all/cat3d/c3_draw.c'
DUAL = ROOT / 'platform/3ds/original/cat3ds_dualscreen.c'
HEADER = ROOT / 'platform/3ds/original/cat3ds_dualscreen.h'
BACKUP = ROOT / 'platform/3ds/.backup-before-stereo-window-v3'
TAG = 'CAT3DS_STEREO_WINDOW_V3'


def once(src, before, after, desc):
    occurrences = src.count(before)
    if occurrences != 1:
        raise RuntimeError(f'{desc}: expected one matching source region, found {occurrences}; nothing changed')
    return src.replace(before, after, 1)


def patch_draw(src):
    src = once(src,
        '\t/* Parallel eye cameras: same angle, physically separated viewpoints.\n'
        '\t * No toe-in/rotation, no gameplay position changes.\n'
        '\t * Suppress an eye shift that would put the original ray origin\n'
        '\t * inside a solid tile or outside the original level. */\n'
        '\tconst fixed ex = viewx + FixedByFrac(eyeOffset, viewsin);\n'
        '\tconst fixed ey = viewy + FixedByFrac(eyeOffset, viewcos);\n'
        '\tconst int etx = ex >> TILESHIFT, ety = ey >> TILESHIFT;\n'
        '\tif (etx >= 0 && etx < mapwidth && ety >= 0 && ety < mapheight &&\n'
        '\t    !tilemap[etx][ety]) { viewx = ex; viewy = ey; }\n'
        '\telse eyeOffset = 0; /* Do not apply off-axis shift to an unshifted camera. */',
        '\t/* CAT3DS_STEREO_WINDOW_V3: eye safety is validated at the\n'
        '\t * PHYSICAL player position in the wrapper below, not at this\n'
        '\t * focal point, which is one whole tile BEHIND the player and\n'
        '\t * legitimately often lies in a solid tile. The old test here\n'
        '\t * cancelled eye separation and left only the image shift. */\n'
        '\tviewx += FixedByFrac(eyeOffset, viewsin);\n'
        '\tviewy += FixedByFrac(eyeOffset, viewcos);',
        'Eye projection anchor')

    src = once(src,
        '\tif (eyeOffset) Cat3DS_DualScreenConvergeWorld(bufferofs, eyeOffset);',
        '\tif (eyeOffset) {\n'
        '\t\t/* Projection equivalent at a plane 1.5 tiles IN FRONT of\n'
        '\t\t * the player (2.5 tiles from the original focal point).\n'
        '\t\t * TransformPoint uses ratio = nx*scale/FOCALLENGTH;\n'
        '\t\t * the required off-axis shift is baseline/ratio.\n'
        '\t\t * Convert once to whole EGA pixels, preserving sign. */\n'
        '\t\tconst int64_t focalToPlane = (int64_t)FOCALLENGTH +\n'
        '\t\t\t3 * (int64_t)TILEGLOBAL / 2;\n'
        '\t\tconst int64_t denominator =\n'
        '\t\t\t((int64_t)scale * focalToPlane) / FOCALLENGTH;\n'
        '\t\tconst int64_t magnitude = eyeOffset < 0\n'
        '\t\t\t? -(int64_t)eyeOffset : (int64_t)eyeOffset;\n'
        '\t\tint shift = denominator > 0\n'
        '\t\t\t? (int)((magnitude + denominator / 2) / denominator) : 0;\n'
        '\t\tif (shift > 16) shift = 16;\n'
        '\t\tCat3DS_DualScreenConvergeWorld(bufferofs,\n'
        '\t\t\teyeOffset < 0 ? -shift : shift);\n'
        '\t}',
        'Finite plane convergence from original projection')

    src = once(src,
        '    const int offset = Cat3DS_DualScreenStereoOffset();\n'
        '    if (offset > 0) {',
        '''    int offset = Cat3DS_DualScreenStereoOffset();
    if (offset > 0) {
        /* Keep the physical eye centers in open map cells. Both eyes MUST
         * use the same baseline, or stereo becomes asymmetric. The virtual
         * focal point behind the player is NOT a collision candidate.
         * A focal point outside the map, however, is never permitted. */
        const fixed perpX = sintable[player->angle];
        const fixed perpY = costable[player->angle];
        const fixed focX = player->x - FixedByFrac(FOCALLENGTH, perpY);
        const fixed focY = player->y + FixedByFrac(FOCALLENGTH, perpX);
        for (; offset >= 512; offset /= 2) {
            const fixed dx = FixedByFrac(offset, perpX);
            const fixed dy = FixedByFrac(offset, perpY);
            int safe = 1;
            for (int eye = -1; eye <= 1; eye += 2) {
                const int px = (player->x + eye * dx) >> TILESHIFT;
                const int py = (player->y + eye * dy) >> TILESHIFT;
                const int fx = (focX + eye * dx) >> TILESHIFT;
                const int fy = (focY + eye * dy) >> TILESHIFT;
                if (px < 0 || px >= mapwidth || py < 0 || py >= mapheight ||
                    tilemap[px][py] || fx < 0 || fx >= mapwidth ||
                    fy < 0 || fy >= mapheight) { safe = 0; break; }
            }
            if (safe) break;
        }
        if (offset < 512) offset = 0;
    }
    if (offset > 0) {''',
        'Physical camera validation in stereo wrapper')
    return src


def patch_dual(src):
    src = once(src,
        ' * At max slider each eye moves by 1/32 tile. Camera angles remain parallel. */',
        ' * At max slider each eye moves by 1/16 tile. Camera angles remain parallel. */',
        'Stereo baseline documentation')
    src = once(src,
        '    /* Less aggressive separation: half of V1, to reduce ghosting.\n'
        '     * 1/64 tile per eye at the top of the hardware slider. */\n'
        '    return (int)(clamped * 1024.0f + 0.5f);',
        '    /* CAT3DS_STEREO_WINDOW_V3: max 1/16 tile per eye. The old\n'
        '     * 1/64 tile eye offsets were largely erased by EGA quantization\n'
        '     * and produced too little distance-dependent disparity.\n'
        '     * The physical 3D slider controls the entire useful range. */\n'
        '    return (int)(clamped * 4096.0f + 0.5f);',
        'Eye separation slider mapping')
    head = src.index('/* CAT3DS_STEREO_CONVERGENCE_V2\n')
    tail = src.index('\nvoid Cat3DS_DualScreenForgetStereo(void)', head)
    original = src[head:tail]
    revised = '''/* CAT3DS_STEREO_WINDOW_V3
 * Shift the original EGA WORLD before DrawHand. The signed shift IN PIXELS
 * is computed from the actual Catacomb projection scale in c3_draw.c,
 * rather than mapping a world offset to a fixed 2 px at all times.
 * Negative is left eye / leftwards; positive is right eye / rightwards.
 * This puts the zero-parallax plane ~1.5 tiles ahead of the player:
 * nearer objects protrude, farther scenery recedes behind the LCD window.
 */
void Cat3DS_DualScreenConvergeWorld(uint16_t egaPage, int32_t signedShiftPixels)
{
    if (!gameplay || !signedShiftPixels || g_sdlScreenMode != 0x0D ||
        g_sdlPixLineWidth < STEREO_VIEW_W || g_sdlPixLineWidth > 640)
        return;
    int pixels = signedShiftPixels < 0 ? -signedShiftPixels : signedShiftPixels;
    if (pixels > 16) pixels = 16;
    const size_t base = (size_t)egaPage * 8u;
    const size_t last = base + (STEREO_VIEW_H - 1u) *
                        (size_t)g_sdlPixLineWidth + STEREO_VIEW_W;
    if (last > sizeof(g_sdlVidMem.A000.gfxByByte)) return;
    uint8_t *page = g_sdlVidMem.A000.gfxByByte + base;
    for (int y = 0; y < STEREO_VIEW_H; ++y) {
        uint8_t *row = page + (size_t)y * g_sdlPixLineWidth;
        if (signedShiftPixels > 0) {
            const uint8_t edge = row[0];
            memmove(row + pixels, row, STEREO_VIEW_W - pixels);
            memset(row, edge, pixels);
        } else {
            const uint8_t edge = row[STEREO_VIEW_W - 1];
            memmove(row, row + pixels, STEREO_VIEW_W - pixels);
            memset(row + STEREO_VIEW_W - pixels, edge, pixels);
        }
    }
}'''
    return src.replace(original, revised, 1)


try:
    for path in (DRAW, DUAL, HEADER):
        if not path.is_file():
            raise RuntimeError('Missing required file: ' + str(path.relative_to(ROOT)))
    source_draw = DRAW.read_text('utf-8')
    source_dual = DUAL.read_text('utf-8')
    source_header = HEADER.read_text('utf-8')
    if TAG in source_draw or TAG in source_dual:
        if TAG in source_draw and TAG in source_dual:
            print('Stereo window calibration already installed; no changes made.')
            sys.exit(0)
        raise RuntimeError('Partial stereo window installation detected; no changes made')
    if 'CAT3DS_REAL_STEREO_V1' not in source_draw or 'CAT3DS_STEREO_CONVERGENCE_V2' not in source_dual:
        raise RuntimeError('Requires existing true-stereo and convergence patches; no changes made')
    revised_header = once(source_header,
        'void Cat3DS_DualScreenConvergeWorld(uint16_t egaPage, int32_t eyeOffset);',
        'void Cat3DS_DualScreenConvergeWorld(uint16_t egaPage, int32_t signedShiftPixels);',
        'Stereo converge API parameter')
    updates = {DRAW: patch_draw(source_draw), DUAL: patch_dual(source_dual),
               HEADER: revised_header}
    for path in updates:
        b = BACKUP / path.relative_to(ROOT)
        b.parent.mkdir(parents=True, exist_ok=True)
        if not b.exists():
            shutil.copy2(path, b)
    for path, contents in updates.items():
        tmp = path.with_suffix(path.suffix + '.window3ds-new')
        tmp.write_text(contents, encoding='utf-8')
        tmp.replace(path)
    print('Stereo window v3 installed: physical eye validation, 1/16 tile eye baseline,')
    print('projection-derived convergence at 1.5 tiles in front of the player.')
    print('Native menu, 2D mode, multicore, game saves and controls unchanged.')
except Exception as exc:
    print('ERROR:', exc, file=sys.stderr)
    sys.exit(1)
