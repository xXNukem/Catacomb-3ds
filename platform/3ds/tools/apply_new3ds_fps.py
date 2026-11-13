#!/usr/bin/env python3
"""FPS + New 3DS clock boost + optional real core-1 automap worker.

Installs against the confirmed dual-screen Catacomb 3DS phase-1 source.
Does not change the original renderer, SDL, menu, controls or game data.
Preflight occurs before any file is modified; safe to run again.
"""
from pathlib import Path
import hashlib
import shutil
import sys

root = Path(__file__).resolve().parents[3]
original_main = root / 'platform/3ds/original/refkeen_original_main.c'
dual = root / 'platform/3ds/original/cat3ds_dualscreen.c'
asset = root / 'platform/3ds/tools/assets/cat3ds_dualscreen_new3ds.c'
backup = root / 'platform/3ds/.backup-before-new3ds-fps'
BASE_SHA256 = '6c66c2b63b0a420cf16648fc7848709f54177ce6428c21f7dc04d06677e2d30f'
new_marker = '/* On New 3DS the automap rasterizer can run on core 1'
entry_marker = '/* CAT3DS_NEW3DS_BOOST_V1 */'

try:
    for p in (original_main, dual, asset):
        if not p.is_file():
            raise RuntimeError(f'Missing file: {p.relative_to(root)}')
    src_main = original_main.read_text(encoding='utf-8')
    src_dual = dual.read_text(encoding='utf-8')
    new_dual = asset.read_text(encoding='utf-8')
    if new_marker not in new_dual:
        raise RuntimeError('Missing required performance code in packaged source')
    already_done = new_marker in src_dual
    if not already_done and hashlib.sha256(dual.read_bytes()).hexdigest() != BASE_SHA256:
        raise RuntimeError('cat3ds_dualscreen.c differs from the confirmed phase-1 revision. No files changed; send the file for a safe adapted patch.')
    if already_done and src_dual != new_dual:
        raise RuntimeError('Performance source is already modified separately. No files changed.')

    if entry_marker not in src_main:
        old_include = '#ifdef __3DS__\n#include <unistd.h>\n#include <sys/stat.h>\n#endif'
        new_include = '#ifdef __3DS__\n#include <3ds.h>\n#include <unistd.h>\n#include <sys/stat.h>\n#endif'
        if src_main.count(old_include) != 1:
            raise RuntimeError('Main header insertion point not found; no files changed')
        src_main = src_main.replace(old_include, new_include, 1)
        old_init = '    (void)argc;\n    (void)argv;\n    mkdir("sdmc:/3ds", 0777);'
        new_init = ('    (void)argc;\n    (void)argv;\n'
                    '    /* CAT3DS_NEW3DS_BOOST_V1 */\n'
                    '    /* Request 804 MHz / L2 on New 3DS before showing the launcher.\n'
                    '     * This does not create threads or change the game logic. */\n'
                    '    osSetSpeedupEnable(true);\n'
                    '    mkdir("sdmc:/3ds", 0777);')
        if src_main.count(old_init) != 1:
            raise RuntimeError('Main entry insertion point not found; no files changed')
        src_main = src_main.replace(old_init, new_init, 1)

    changes = {}
    if not already_done:
        changes[dual] = new_dual
    if original_main.read_text(encoding='utf-8') != src_main:
        changes[original_main] = src_main

    for file, new_content in changes.items():
        rel = file.relative_to(root)
        bak = backup / rel
        bak.parent.mkdir(parents=True, exist_ok=True)
        if not bak.exists():
            shutil.copy2(file, bak)
        file.write_text(new_content, encoding='utf-8')

    print('Catacomb 3DS - FPS / New3DS performance baseline installed')
    print(f'Changed files: {len(changes)}')
    print('FPS: bottom HUD, updated from actual presentation calls')
    print('New3DS: requests faster CPU/L2 and starts a core-1 map worker when allowed')
    print('Other consoles / failed worker: safe single-core fallback')
    print('Automap: reveal solid tiles adjacent to visible floor tiles')
    print('Unchanged: menu, saves, game engine, SDL3, original renderer, controls')
except Exception as e:
    print('ERROR - no changes applied:', e, file=sys.stderr)
    sys.exit(1)
