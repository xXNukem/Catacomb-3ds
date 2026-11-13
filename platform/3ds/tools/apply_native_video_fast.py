#!/usr/bin/env python3
"""One-step native 3DS EGA fast presentation for Catacomb 3-D.

Only gameplay frames bypass SDL3 software texture composition. All other
screens retain the existing engine renderer. Refuses unexpected source versions.
"""
from pathlib import Path
import hashlib
import shutil
import sys

root = Path(__file__).resolve().parents[3]
video = root / 'src/backend/video/be_video.c'
dual = root / 'platform/3ds/original/cat3ds_dualscreen.c'
asset = root / 'platform/3ds/tools/assets/cat3ds_dualscreen_fast.c'
backup = root / 'platform/3ds/.backup-before-native-video-fast'
old_hash = '2b38885db1f81fe442c9609a1be5b0029e9e15b75618701e9b7765522f0575f8'
marker = '/* CAT3DS_NATIVE_EGA_TOP_FAST_V1'
try:
    for path in (video, dual, asset):
        if not path.is_file():
            raise RuntimeError('Missing file: ' + str(path.relative_to(root)))
    src_video = video.read_text(encoding='utf-8')
    src_dual = dual.read_text(encoding='utf-8')
    fast_dual = asset.read_text(encoding='utf-8')
    if marker not in fast_dual:
        raise RuntimeError('Fast renderer asset is damaged')
    done = marker in src_dual
    if not done and hashlib.sha256(dual.read_bytes()).hexdigest() != old_hash:
        raise RuntimeError('The dual-screen source differs from the last confirmed patch; no files changed')
    if done and src_dual != fast_dual:
        raise RuntimeError('Native video patch already modified locally; no files changed')

    # After copying EGA VRAM to the existing indexed 320x200 mirror, return
    # BEFORE SDL locks and fills the intermediate 32-bit texture. All other
    # presentation modes still use the original ReflectionHLE/SDL3 path.
    old_lock = '''\tint pitchPix;
\tuint32_t *currPixPtr = (uint32_t *)BEL_ST_LockTexture(g_sdlTexture, &pitchPix),
\t         *currRowPtr = currPixPtr;
\tpitchPix /= 4;
\tcurrPalPixPtr = g_sdlHostScrMem.egaGfx;'''
    new_lock = '''#ifdef __3DS__
\t/* CAT3DS_NATIVE_EGA_UPLOAD_SKIP_V1: indexed pixels are already
\t * available. The native 3DS compositor consumes them directly. */
\tif (g_sdlScreenMode == 0x0D && Cat3DS_DualScreenHasGameplayFrame())
\t{
\t\tg_sdlDoRefreshGfxOutput = false;
\t\treturn true;
\t}
#endif
\tint pitchPix;
\tuint32_t *currPixPtr = (uint32_t *)BEL_ST_LockTexture(g_sdlTexture, &pitchPix),
\t         *currRowPtr = currPixPtr;
\tpitchPix /= 4;
\tcurrPalPixPtr = g_sdlHostScrMem.egaGfx;'''
    old_display = '''\tBEL_ST_SetDrawColor(0xFF000000);
\tBEL_ST_RenderClear();
#ifdef __3DS__
\t/* In gameplay, crop the original 3D viewport (x0..263/y0..143)
\t * away from the ORIGINAL EGA status art. No HUD appears above.
\t * The camera FOV itself remains original until its dedicated
\t * widescreen-renderer patch (no raycaster is introduced here). */
\tif (g_sdlScreenMode == 0x0D && Cat3DS_DualScreenHasGameplayFrame())
\t{
\t\tBEL_ST_RenderNative3DSViewport(g_sdlTexture);
\t\tCat3DS_DualScreenPresent(g_sdlHostScrMem.egaGfx, g_sdlTexWidth,
\t\t                           g_sdlEGACurrBGRAPalette);
\t\tBEL_ST_FinishHostDisplayUpdate();
\t\tg_be_sdlLastRefreshTicks = BEL_ST_GetTicksMS();
\t\treturn;
\t}
#endif
\tBEL_ST_SetDrawColor(g_sdlEGALastBGRABorderColor);'''
    new_display = '''#ifdef __3DS__
\t/* CAT3DS_NATIVE_EGA_PRESENT_V1: gameplay avoids SDL texture
\t * upload, software scaling, render target composition and present. */
\tif (g_sdlScreenMode == 0x0D && Cat3DS_DualScreenHasGameplayFrame())
\t{
\t\tCat3DS_DualScreenPresent(g_sdlHostScrMem.egaGfx, g_sdlTexWidth,
\t\t                           g_sdlEGACurrBGRAPalette);
\t\tg_sdlForceGfxControlUiRefresh = false;
\t\tg_be_sdlLastRefreshTicks = BEL_ST_GetTicksMS();
\t\treturn;
\t}
#endif
\tBEL_ST_SetDrawColor(0xFF000000);
\tBEL_ST_RenderClear();
\tBEL_ST_SetDrawColor(g_sdlEGALastBGRABorderColor);'''
    changes = {}
    if 'CAT3DS_NATIVE_EGA_UPLOAD_SKIP_V1' not in src_video:
        if src_video.count(old_lock) != 1 or src_video.count(old_display) != 1:
            raise RuntimeError('Expected ReflectionHLE gameplay display blocks not found (no files changed)')
        # Check that the lock block is inside the EGA update function, not elsewhere.
        pos = src_video.index(old_lock)
        func_start = src_video.index('static bool BEL_ST_UpdateTextureForEGAVGAMode(void)')
        func_end = src_video.index('static uint32_t g_be_sdlLastRefreshTicks',func_start)
        if not func_start < pos < func_end:
            raise RuntimeError('Unexpected texture upload location')
        src_video = src_video.replace(old_lock,new_lock,1).replace(old_display,new_display,1)
        changes[video] = src_video
    elif 'CAT3DS_NATIVE_EGA_PRESENT_V1' not in src_video:
        raise RuntimeError('Partially applied native video patch; no files changed')
    if not done:
        changes[dual] = fast_dual

    for path,new_contents in changes.items():
        rel = path.relative_to(root)
        bak = backup / rel
        bak.parent.mkdir(parents=True,exist_ok=True)
        if not bak.exists():
            shutil.copy2(path,bak)
        path.write_text(new_contents,encoding='utf-8')
    print('Native EGA gameplay presentation installed: skipped SDL software renderer')
    print('Top: original 264x144 EGA camera -> physical 400x240 framebuffer')
    print('Bottom: existing HUD, automap and FPS; menus/saves/controls unchanged')
    print('Changes:',len(changes),'(safe to run again)')
except Exception as exc:
    print('ERROR - no files changed:',exc,file=sys.stderr)
    sys.exit(1)
