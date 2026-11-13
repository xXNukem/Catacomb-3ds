# Catacomb 3DS — first-person walk preview

**Scope:** A temporary software raycasting test using decompressed original DOS level geometry. It is *not* the original id Software / ReflectionHLE Catacomb 3-D renderer or gameplay runtime.

This patch extends `cat3ds_level_map.cpp`, which already loads authentic DOS map planes, with a stand-alone player/camera and DDA raycaster (`cat3ds_walk_preview.cpp`). Each frame is drawn as 80 four-pixel-wide vertical wall columns into the existing emulated EGA VRAM, which the platform's 3DS video backend displays. The same video backend continues to support the launcher and original graphics previews.

## Where to find it

Launcher → Catacomb 3-D → A (Details) → X (Original Map) → X (First-Person Test).

## Controls

| Button | Action |
|---|---|
| Circle Pad Y / D-pad Up/Down | Walk forward/backward |
| Circle Pad X / D-pad Left/Right | Turn |
| L/R | Strafe |
| A | Reset camera at the map's original player spawn |
| X or B | Return to original map |
| START | Exit app |

The map inspector still uses D-pad to inspect tiles and L/R to switch among its 20 levels. Entering first-person view after changing maps starts from that map's original player spawn.

## Intentionally provisional

- Wall geometry comes from original map plane 0 and spawn/orientation from plane 2 (spawn codes 1–4).
- `tile > 0 && tile < 36` is treated as solid for the prototype, following the basic `NUMFLOORS` distinction in Catacomb 3-D's `SetupGameLevel`. Special behaviors of doors, walls and actors are **not simulated**.
- Flat EGA colors vary by tile/face only to help inspect geometry. Original texture mapping, lighting, sprites, HUD and game simulation remain to be connected.
- Player speed, collision, and field of view are provisional — not a claim of exact DOS gameplay parity.
- The patch reads the already-deployed original game files from the SD card; it neither writes nor deletes any game files.

## Validation and limitations

A host test (`tests/3ds_walk_preview_host.cpp`) checks facing from a DOS-style spawn, movement against boundary walls, rotation and the EGA write path. C++ syntax of the changed launcher has been checked using stubbed libctru APIs. Compile and runtime on New 3DS are still to be tested.

The following engineering stage is to link actual `id91_11` game startup, memory/cache, input, timing, audio, and the original 3D renderer, replacing this isolated preview rather than treating it as a final renderer.
