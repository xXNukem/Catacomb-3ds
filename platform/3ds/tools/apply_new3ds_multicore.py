#!/usr/bin/env python3
"""Catacomb 3DS: original engine + parallel New3DS top-screen conversion.

Changes only the 3DS dual-screen presentation implementation; the main game,
launcher, controller input and saved games are untouched. Includes a backup.
"""
from pathlib import Path
import hashlib
import shutil
import sys

root = Path(__file__).resolve().parents[3]
target = root / 'platform/3ds/original/cat3ds_dualscreen.c'
asset = root / 'platform/3ds/tools/assets/cat3ds_dualscreen_multicore.c'
backup = root / 'platform/3ds/.backup-before-new3ds-multicore/platform/3ds/original/cat3ds_dualscreen.c'
expected_sha = '9941ebc8f4e506a74f63614fd04dd88e0e1c568eb5134b104e71616c4306ea7a'
marker = 'CAT3DS_CORE2_RENDER_V1'

try:
    if not target.is_file() or not asset.is_file():
        raise RuntimeError('Missing original compositor or packaged source. Apply after RGB565/profile patch.')
    original = target.read_bytes()
    replacement = asset.read_bytes()
    if marker.encode() not in replacement or b'threadCreate(topWorkerMain' not in replacement:
        raise RuntimeError('Corrupted packaged multicore compositor.')
    if original == replacement:
        print('Already installed: multicore view output; no changes.')
        raise SystemExit(0)
    if hashlib.sha256(original).hexdigest() != expected_sha:
        raise RuntimeError('Current dual-screen code differs from the expected RGB565 revision. No files changed; keep local edits and share this file for adaptation.')
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(target, backup)
    tmp = target.with_name('cat3ds_dualscreen.c.multicore-new')
    tmp.write_bytes(replacement)
    tmp.replace(target)
    print('New3DS parallel rendering installed.')
    print('Core 0: original game engine + bottom HUD/map; core 2: upper 3D framebuffer conversion.')
    print('Fallback: limited core 1 worker if permitted, then single core.')
    print('On-screen core indicator: C2 / C1 / BST / C0 (actual worker processor).')
    print('Menu, controls, saves, FOV and original ReflectionHLE renderer unchanged.')
except Exception as exc:
    print('ERROR:', exc, file=sys.stderr)
    sys.exit(1)
