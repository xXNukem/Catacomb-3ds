#!/usr/bin/env python3
"""Off-axis stereo convergence for the original Catacomb 3-D renderer on New 3DS.

Changes only the already-installed true-stereo path. The real world is shifted
before DrawHand, keeping foreground weapon zero-parallax. Safe to apply twice.
"""
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
DRAW = ROOT / 'src/id91_11/cat_all/cat3d/c3_draw.c'
DUAL = ROOT / 'platform/3ds/original/cat3ds_dualscreen.c'
HEADER = ROOT / 'platform/3ds/original/cat3ds_dualscreen.h'
BACKUP = ROOT / 'platform/3ds/.backup-before-stereo-convergence'
MARKER = 'CAT3DS_STEREO_CONVERGENCE_V2'


def replace_one(src, old, new, name):
    found = src.count(old)
    if found != 1:
        raise RuntimeError(f'{name}: expected one anchor; found {found}; no files changed.')
    return src.replace(old, new, 1)


try:
    for path in (DRAW, DUAL, HEADER):
        if not path.is_file():
            raise RuntimeError('Missing ' + str(path.relative_to(ROOT)))
    draw = DRAW.read_text('utf-8')
    dual = DUAL.read_text('utf-8')
    header = HEADER.read_text('utf-8')
    if 'CAT3DS_REAL_STEREO_V1' not in draw or 'CAT3DS_REAL_STEREO_V1' not in dual:
        raise RuntimeError('Requires the installed true-stereo patch. No files changed.')

    if MARKER not in draw:
        draw = replace_one(draw,
            '\tif (etx >= 0 && etx < mapwidth && ety >= 0 && ety < mapheight &&\n'
            '\t    !tilemap[etx][ety]) { viewx = ex; viewy = ey; }',
            '\tif (etx >= 0 && etx < mapwidth && ety >= 0 && ety < mapheight &&\n'
            '\t    !tilemap[etx][ety]) { viewx = ex; viewy = ey; }\n'
            '\telse eyeOffset = 0; /* Do not apply off-axis shift to an unshifted camera. */',
            'Camera collision check')
        draw = replace_one(draw,
            '\tDrawWallList();\n\tDrawScaleds();\n\n\tEGAWRITEMODE(0);',
            '\tDrawWallList();\n\tDrawScaleds();\n'
            '#ifdef __3DS__\n'
            '\t/* CAT3DS_STEREO_CONVERGENCE_V2: Shift ONLY the world image,\n'
            '\t * after original walls+actors but BEFORE the monoscopic hand.\n'
            '\t * Parallel cameras + off-axis image planes, never toe-in. */\n'
            '\tif (eyeOffset) Cat3DS_DualScreenConvergeWorld(bufferofs, eyeOffset);\n'
            '#endif\n\n\tEGAWRITEMODE(0);',
            'Original wall and sprite drawing')

    if MARKER not in dual:
        dual = replace_one(dual,
            '    return (int)(clamped * 2048.0f + 0.5f);',
            '    /* Less aggressive separation: half of V1, to reduce ghosting.\n'
            '     * 1/64 tile per eye at the top of the hardware slider. */\n'
            '    return (int)(clamped * 1024.0f + 0.5f);',
            'Stereo baseline')
        hook = 'void Cat3DS_DualScreenForgetStereo(void) { stereoLeftValid = 0; }'
        converge = '''/* CAT3DS_STEREO_CONVERGENCE_V2
 * Parallel-camera stereo with a finite zero-parallax plane: at full slider
 * each camera moves 1/64 world tile and its image plane shifts 2 original
 * EGA pixels toward the outer edge. This places the convergence plane at
 * roughly 2-3 tiles in front of the player with the original projection.
 *
 * Shift the world BEFORE DrawHand, so the weapon stays at zero parallax.
 * Direct EGA pixels are valid on 3DS; this remains a native stereo path.
 */
#define CAT3DS_CONVERGENCE_PX_PER_EYE 2
#define CAT3DS_HALF_EYE_OFFSET_MAX 1024
void Cat3DS_DualScreenConvergeWorld(uint16_t egaPage, int32_t eyeOffset)
{
    if (!gameplay || !eyeOffset || g_sdlScreenMode != 0x0D ||
        g_sdlPixLineWidth < STEREO_VIEW_W || g_sdlPixLineWidth > 640)
        return;
    int magnitude = eyeOffset < 0 ? -eyeOffset : eyeOffset;
    if (magnitude > CAT3DS_HALF_EYE_OFFSET_MAX)
        magnitude = CAT3DS_HALF_EYE_OFFSET_MAX;
    const int pixels = (magnitude * CAT3DS_CONVERGENCE_PX_PER_EYE +
                        CAT3DS_HALF_EYE_OFFSET_MAX / 2) /
                        CAT3DS_HALF_EYE_OFFSET_MAX;
    if (!pixels) return;

    const size_t base = (size_t)egaPage * 8u;
    const size_t last = base + (STEREO_VIEW_H - 1u) *
                        (size_t)g_sdlPixLineWidth + STEREO_VIEW_W;
    if (last > sizeof(g_sdlVidMem.A000.gfxByByte)) return;
    uint8_t *page = g_sdlVidMem.A000.gfxByByte + base;
    for (int y = 0; y < STEREO_VIEW_H; ++y) {
        uint8_t *row = page + (size_t)y * g_sdlPixLineWidth;
        if (eyeOffset > 0) {
            const uint8_t edge = row[0];
            memmove(row + pixels, row, STEREO_VIEW_W - pixels);
            memset(row, edge, pixels);
        } else {
            const uint8_t edge = row[STEREO_VIEW_W - 1];
            memmove(row, row + pixels, STEREO_VIEW_W - pixels);
            memset(row + STEREO_VIEW_W - pixels, edge, pixels);
        }
    }
}
'''
        dual = replace_one(dual, hook, converge + '\n' + hook, 'Dual-screen stereo functions')

    if 'Cat3DS_DualScreenConvergeWorld(' not in header:
        header = replace_one(header,
            'void Cat3DS_DualScreenForgetStereo(void);',
            'void Cat3DS_DualScreenForgetStereo(void);\n'
            '/* Off-axis convergence, invoked between world and hand rendering. */\n'
            'void Cat3DS_DualScreenConvergeWorld(uint16_t egaPage, int32_t eyeOffset);',
            'Stereo API header')
    elif MARKER not in dual:
        raise RuntimeError('Unexpected convergence API already exists; no files changed.')

    updates = {DRAW: draw.encode('utf-8'), DUAL: dual.encode('utf-8'), HEADER: header.encode('utf-8')}
    changed = [(path, body) for path, body in updates.items() if path.read_bytes() != body]
    for path, body in changed:
        backup = BACKUP / path.relative_to(ROOT)
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(path, backup)
    for path, body in changed:
        temporary = path.with_suffix(path.suffix + '.convergence-new')
        temporary.write_bytes(body)
        temporary.replace(path)
    print('New 3DS stereo convergence installed; updated', len(changed), 'file(s).')
    print('Parallel cameras, reduced eye separation, original world shifted before weapon.')
    print('No changes to the menu, gameplay, controls, saves or lower HUD/map.')
except Exception as ex:
    print('ERROR:', ex, file=sys.stderr)
    sys.exit(1)
