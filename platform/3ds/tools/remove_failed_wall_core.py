#!/usr/bin/env python3
"""Undo only the experimental original-wall-core files, not DOS game data."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
targets = {
    'platform/3ds/source/c3_def.h': 'CAT3DS_ORIGINAL_C3_TRACE_SHIM_H',
    'platform/3ds/source/cat3ds_original_renderer.h': 'CAT3DS_ORIGINAL_RENDERER_H',
    'platform/3ds/source/cat3ds_original_renderer.cpp': 'Catacomb 3DS: original Catacomb 3-D wall geometry',
    'platform/3ds/source/cat3ds_engine_trace.c': 'FollowWalls',
    'tests/3ds_original_wall_core_host.cpp': 'cat3dsRenderOriginalWalls',
    'tests/3ds_original_wall_core_host.sh': 'original-wall-core',
    'docs/3ds-original-wall-core.md': 'original wall-following renderer',
}
for rel, marker in targets.items():
    path = root / rel
    if not path.exists():
        continue
    if marker not in path.read_text(errors='replace'):
        print(f'SKIPPED user-modified/unrecognized file: {rel}', file=sys.stderr)
        continue
    path.unlink()
    print('Removed failed experimental component:', rel)
print('Game data and other source files were left untouched.')
