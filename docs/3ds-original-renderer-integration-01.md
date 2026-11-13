# Catacomb 3DS — Original renderer integration, stage 01

## Why this is deliberately small

In ReflectionHLE, `ThreeDRefresh()` in `src/id91_11/cat_all/cat3d/c3_draw.c`
requires `BuildTables`, `TraceRay`, `FollowWalls`, `DrawWallList`, `DrawScaleds`,
the game's actor lists and shapes, the video page manager, and the 70 Hz
time base. It cannot safely replace the current raycaster with a simple
function call. This patch starts migrating that call graph, not another
parallel game renderer.

## Delivered

- `cat3ds_original_trace.{h,cpp}` — small, bounded C++ adaptation of the
  game's `BuildTables()` edge-camera geometry and `TraceRay()` tile stepping.
  Uses the original 264-pixel viewport geometry, 16.16 fixed-point tile
  convention, one-tile-behind view origin and view-plane prestep; converts
  the existing 3DS walk-preview heading to Cat3D screen coordinates.
- The original trace is run while drawing the **bottom-screen diagnostic**.
  `Orig ray: L(x,y) R(x,y)` shows the map tiles found by the *two edge rays*.
  These numbers change with camera position and direction. The current
  top-screen rendering is **unchanged** to keep a known-good fallback.
- Map bounds and step limits are checked to avoid hangs or illegal accesses
  if an unusual installation or malformed map is loaded.

This is **not a verbatim build of `c3_draw.c`**. The port deliberately adapts
signed trigonometry and bounds checks to the host; full behavioral equivalence
will require further tests against ReflectionHLE. It does not yet use
`FollowWalls`, its 3D wall drawing, actors, doors, combat, menus or audio.

## Verification

Host test with GCC, `-Wall -Wextra -Werror`, AddressSanitizer and
UndefinedBehaviorSanitizer: 360 headings in a synthetic enclosed map,
plus open-map and invalid camera cases. A separate **non-distributed**
read-only test loaded all 20 maps of the supplied Catacomb 3-D v1.22:
the original edge trace found both walls from every level's spawn and
was run at 24 additional angles per level. No game data is in the patch.

The new sources are picked up by the existing `platform/3ds/Makefile` using
its `source/*.cpp` wildcard, so no build-system change is needed. The new
native link still needs to be tested with devkitARM and a real 3DS.

## Roadmap (next integration stages)

1. Port the original `FollowWalls()` traced wall list on top of this adapter;
   compare its visible tiles with the current preview without replacing the
   screen output until the comparison is reliable.
2. Connect `DrawWallList()` / wall scaling to our existing EGA backend.
3. Replace preview movement with the original player logic, time and input.
4. Connect sprites, objects, interaction, weapons, audio, then menus/save.

**License**: upstream Catacomb 3-D source code is GPL-2.0-or-later.
The adapted module carries upstream attribution and the same license. Keep
both notices in public redistributions and retain the repository's license.
