# Catacomb 3DS: first original level data

The current proof-of-concept decodes original Catacomb 3-D DOS map data
on the New Nintendo 3DS. This is a deliberately scoped step toward game
startup; the original Catacomb 3-D game engine is **not yet linked**.

## Format

- `CAT3D.EXE` is unpacked in memory with ReflectionHLE's `Unlzexe_unpack`.
- The original `MTEMP.TMP` header is embedded in the unpacked DOS image.
  For recognized v1.00/v1.22, the offsets follow ReflectionHLE's
  `be_gamedefs_cat3d.h`.
- The header provides map offsets and the RLEW tag, read as little-endian.
- Each map header in `GAMEMAPS.C3D` is 38 bytes (plane starts, plane lengths,
  dimensions, name). Planes 0 and 2 are decoded into temporary memory.
- Each plane is Carmack expanded into an RLEW stream, then RLEW expanded into
  an array of original 16-bit tile codes.
- The level browser displays these arrays through the existing native EGA
  backend; there is no new graphics renderer or asset conversion pipeline.

## Real-file tests

The supplied original Catacomb 3-D v1.22 installation yielded all 20 maps,
including `Approach` (40x28), `Labyrinth` (61x40) and `NemesissLair`
(64x63). Each had one recognized player start marker in plane 2.
No proprietary game data are packaged with the port.

## Scope and limitations

The map browser and white inspection cursor are **not gameplay** and do not
simulate the game's collision or enemy state. Wall colors correspond to the
`tile < NUMFLOORS` rule seen in `c3_game.c:SetupGameLevel`; player spawn
codes 1..4 follow `c3_game.c:ScanInfoPlane`. Other object codes are shown in
cyan for inspection without spawning in-game actors.

This does not change the structurally compatible resource probe for Abyss,
Armageddon and Apocalypse, and does not yet imply gameplay support for them.

The code never writes game files or extracts their resources to SD. To
advance next, connect the original engine initialization and input/audio/
timing backends, while preserving these independent resource tests.
