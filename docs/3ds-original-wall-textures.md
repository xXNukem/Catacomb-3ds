# Catacomb 3DS — Original EGA Wall Textures (intermediate preview)

## Purpose

Render original **Catacomb 3-D** wall graphics inside the existing **temporary** DDA first-person walk test. It does not run the original game engine, introduce actors, or simulate doors. This work is intended to verify actual assets and the EGA display path on New 3DS before bringing up ReflectionHLE's runtime.

## Data path

- Only uses the user's own files: `CAT3D.EXE` (or the validated EXE name) and `EGAGRAPH.C3D` in the existing `cat3d` SD directory.
- ReflectionHLE's `Unlzexe_unpack()` reconstructs the DOS EXE image. The known Cat3D 1.00 and 1.22 embedded offsets provide EGAHEAD and EGADICT. No extraction is written to disk.
- Huffman chunk 0 contains 155 `{widthBytes,height}` picture descriptors. Chunks 134–158 contain 25 planar 64×64 EGA wall pictures (8 bytes/scanline/plane; four planes = 2,048 bytes each). Each picture is converted to 4-bit color indices in memory.
- The tile-to-picture mapping is based on `walllight1` / `walldark1` in `src/id91_11/cat_all/cat3d/c3_draw.c`; dark pictures are selected for Y-grid intersections. Special map codes without an ordinary wall picture use the previous flat preview color.
- EGA is drawn using `BE_ST_EGAUpdateGFXBitsFrom4bitsPixel()`. The view still uses 80 ray columns of four pixels each and the previous movement/collision logic.

## Validation and limitations

The 25 pictures decoded correctly against the user's Cat3D **v1.22** data. All had 64×64 pixels and nonconstant palette data; the picture mapping and textured renderer host tests passed under ASan and UBSan. DevkitARM compilation and real 3DS frame rate remain to be confirmed.

The **v1.00** offsets are based on ReflectionHLE's game definitions but have not been validated with v1.00 game data. Lighting direction and projection are approximations, not a faithful implementation of the original renderer. Actor, door, wall animations, floor, sky, and input game semantics remain pending. **Do not describe this preview as running the game.**

## Controls

Catacomb 3-D → A details → X original map → X first-person. D-pad/Circle Pad: walk + turn; L/R: strafe; Y: analog toggle; A: reset spawn; X/B: back to map. L/R in the map switches between all 20 original levels.

If texture loading fails, the app leaves you in the map and prints `WALL ART ERROR` on the lower screen. No game files are modified.
