# Catacomb 3DS — Native selector and original-resource preview

This development milestone adds an EGA text-based graphical launcher,
non-destructive game-directory creation, original DOS file verification and
an authentic Catacomb 3-D `TITLEPIC` preview.

## Copy the original DOS files via FTP

Run the `.3dsx` once. It will automatically create these directories if missing:

| Game | Path on 3DS microSD |
| --- | --- |
| Catacomb 3-D | `/3ds/catacomb-3ds/games/cat3d/` |
| Catacomb Abyss | `/3ds/catacomb-3ds/games/catabyss/` |
| Catacomb Armageddon | `/3ds/catacomb-3ds/games/catarm/` |
| Catacomb Apocalypse | `/3ds/catacomb-3ds/games/catapoc/` |

Only missing folders are created. The app **never** removes, empties, renames,
or overwrites files in any existing game folder. If SD access fails, the
launcher reports the problematic directory.

Transfer the *original, unmodified DOS game files* to each corresponding
folder. For the current title preview, `cat3d/` needs at least its complete
supported installation, including `CAT3D.EXE`, `EGAGRAPH.C3D`,
`GAMEMAPS.C3D`, and `AUDIO.C3D`. Filenames and sizes/CRC32 must match a
recognized ReflectionHLE variant (Catacomb 3-D v1.00 or v1.22).

`CAT3D.EXE` supplies the embedded `EGADICT` and `EGAHEAD` resources; the
launcher uses ReflectionHLE's existing LZEXE unpacker and decodes the
original chunk `TITLEPIC` (index 21) from `EGAGRAPH.C3D` in memory. Nothing
is extracted onto or rewritten on the SD card.

## Controls

- Up/down: select one of four games (top screen)
- A: game-file details (bottom screen)
- A again, when Catacomb 3-D is fully verified: show original TITLEPIC
- B: return from details/title preview
- Y: check installations again after FTP transfer
- X from main menu: previously verified EGA video diagnostic
- START: exit

Top-screen menu labels use ReflectionHLE's included public-domain CGA 8x8
font, drawn via its EGA bitplane writing API. No SDL dependency is added.

## Build

From the repository root after extracting the patch ZIP:

```sh
python3 platform/3ds/tools/gen_game_manifest.py
make -B -C platform/3ds
3dslink platform/3ds/3ds.3dsx
```

The Makefile additionally builds **existing** upstream source files:
`src/unlzexe/unlzexe.c`, `src/be_cross.c` and
`src/backend/filesystem/be_filesystem_file.c`. These remain unmodified.

## Limits

**This is not a playable game yet.** The title-preview path is implemented for
Catacomb 3-D v1.00/v1.22 only. Abyss, Armageddon, and Apocalypse currently
have validated install directories and menu entries; their original title
preview and gameplay are future work. Other unsupported or modified game
revisions may show a version/CRC mismatch. The complete original input,
audio, engine and game resource-cache paths are not yet integrated.

This ZIP contains only new/modified port source files and no proprietary game
content. All original source attributions are retained.
