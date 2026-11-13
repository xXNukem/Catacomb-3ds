# Catacomb 3DS — first-person test: view-projection correction

This is a small refinement of the **temporary DDA first-person preview**,
not the original game's rendering or simulation loop.

## What caused the visible distortion?

The previous preview cast **80 rays**, each covering four 320×200 EGA pixels.
Moving slightly away from a wall rapidly lost lateral texture detail. It
also used a comparatively wide horizontal camera plane (`0.66`), corresponding
to approximately 67° horizontal FOV, and an independent short wall-height
scale of `150 / distance`. This caused the original 64×64 EGA textures to
shrink much faster than the game did.

ReflectionHLE's original Catacomb 3-D
`src/id91_11/cat_all/cat3d/c3_draw.c` `BuildTables()` derives its ray angles
from `VIEWWIDTH` (264), `FOCALLENGTH` (1 tile), and `MINDIST` (0.4 tile).
The half-angle of its horizontal view is approximately
`atan(1 / (2*(1+0.4)))` ≈ 19.65°, a 39.3° horizontal FOV.

This preview now uses:
- 320 independently sampled columns (one EGA pixel per ray);
- a camera plane of `1 / 2.8` (approx. the original horizontal FOV);
- an approximate 320-wide wall-height scale of `448 / (distance+1)`;
- optional **SELECT** switch to 160 rays with two-pixel columns if hardware
  performance is insufficient.

The projection is still *approximate*. The original renderer uses a
wall-following tracer, a focal point behind the player, fixed point math,
and different clipping and projection logic. The goal of this adjustment is
to address obvious minification, not to claim pixel-perfect accuracy.

## Ceiling and floor

Cat3D's `ClearScreen()` fills the upper viewport with EGA palette index 0
(black) and the lower viewport with index 8 (dark gray). The preview now
uses precisely these **flat colors** over the full 320×200 display and
removes the top/bottom debug text overlays. Controls and diagnostics remain
on the lower 3DS screen. No extra floor or ceiling textures are introduced.

## Validation

- The host tests validate ceiling/floor values on all 200 scanlines and
  screen bounds, and exercise 320-column and 160-column rendering.
- Existing movement and wall collision tests still pass.
- The preview was rendered on the host using the user's original
  Catacomb 3-D v1.22 graphics and map files.
- No DOS game assets are part of this patch; no write access to user files.
- Hardware build and performance still require testing on a New 3DS.
