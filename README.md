<div align="center">

<img src="assets/catacomb-3ds-banner.png" alt="Catacomb 3DS" width="100%">

**Catacomb 3-D and Catacomb Abyss on New Nintendo 3DS. Armageddon and Apocalypse are planned.**

![Platform](https://img.shields.io/badge/NEW_NINTENDO-3DS-253a2b?style=for-the-badge&labelColor=102016&color=3fbd6a)
![Status](https://img.shields.io/badge/STATUS-WORK%20IN%20PROGRESS-151515?style=for-the-badge&labelColor=501119&color=fe4457)

</div>

---

<div align="center">

## ☕ Support the project

If you enjoy the port and want to buy me a coffee, you can do it here. Every bit helps with future updates and projects. No pressure, of course.

<a href="https://ko-fi.com/xxnukem"><img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="Support xXNukem on Ko-fi" height="48"></a>

**[ko-fi.com/xxnukem](https://ko-fi.com/xxnukem)**

</div>

---

## What is Catacomb 3DS?

Catacomb 3DS is an unofficial homebrew project for **New Nintendo 3DS**, based on [ReflectionHLE](https://github.com/ReflectionHLE/ReflectionHLE). It currently brings **Catacomb 3-D** and **Catacomb Abyss** to the console, with **Catacomb Armageddon** and **Catacomb Apocalypse** planned for later.

The port runs the original game engines with adaptations for the 3DS's controls and two screens. Stereoscopic 3D is also being developed. The aim is to keep the games close to their DOS versions while making them comfortable to play on the console.

## Current status

This is an early development version, **not a finished release**.

- **Catacomb 3-D:** playable using the original ReflectionHLE engine. The current launcher starts the v1.22 edition.
- **Catacomb Abyss:** runs through the original engine, with compatibility, saving and menu behaviour still being worked on.
- **Catacomb Armageddon / Apocalypse:** shown in the game selector, but their engines are not integrated yet.
- **Stereoscopic 3D:** experimental and still being adjusted.

The port has a game selector, native 3DS input and a dual-screen presentation. Some menus and save/load behaviour still need work.

## Installation

### 1. Get the 3DS executable

You need a **New Nintendo 3DS / New Nintendo 3DS XL** capable of running homebrew through the Homebrew Launcher.

There is **no packaged public release yet**. For now, build the project using the instructions below. The executable for the original-engine version is **`cat3ds-original.3dsx`**. The older `3ds.3dsx` is an experimental test frontend and is **not** the executable to install for normal gameplay.

Copy `cat3ds-original.3dsx` to:

```text
sdmc:/3ds/catacomb-3ds/cat3ds-original.3dsx
```

### 2. Copy the original DOS game files

**Game files are not included in this repository.** You need your own original DOS copies of Catacomb 3-D and/or Catacomb Abyss.

Run the `.3dsx` once: the launcher creates the game folders automatically. Then copy the original DOS files into the appropriate folders on the SD card, without mixing editions.

```text
sdmc:/3ds/catacomb-3ds/
├── cat3ds-original.3dsx
├── games/
│   ├── cat3d/
│   │   ├── CAT3D.EXE
│   │   ├── AUDIO.C3D
│   │   ├── EGAGRAPH.C3D
│   │   └── GAMEMAPS.C3D
│   ├── catabyss/
│   │   ├── CATABYSS.EXE    (Abyss v1.13)
│   │   │   OR ABYSGAME.EXE (Abyss v1.24)
│   │   ├── AUDIO.ABS
│   │   ├── EGAGRAPH.ABS
│   │   ├── GAMEMAPS.ABS
│   │   └── ...other files from your original DOS copy
│   ├── catarm/           (not playable yet)
│   └── catapoc/          (not playable yet)
├── saves/                (created by the port)
├── saves-abyss/          (created when Abyss starts)
└── config/               (created by the port)
```

For **Catacomb 3-D**, use the matching files from **v1.22** for the current playable path. The launcher can recognize v1.00 data, but gameplay with that edition is not yet confirmed.

For **Catacomb Abyss**, the port supports the resource layouts of **v1.13** (`CATABYSS.EXE`) and **v1.24** (`ABYSGAME.EXE`). The four main files above are needed to start the game, but copying the entire original DOS installation is recommended. `SHP01.ABS` through `SHP12.ABS` belong to the separate DOS intro and are not required by the current 3DS gameplay launcher.

Keep the actual game files **directly inside** `cat3d/` or `catabyss/`, rather than nesting another DOS game directory within them. Do not rename or mix files from different releases.

### 3. Start the game

Open **Catacomb 3DS** from the Homebrew Launcher, select a game and press **A** to start a supported installation. Press **X** for file details. Press **Y** to rescan the folders after copying files.

If a game is not recognized, check that the files are in the correct folder and belong to a compatible version. The selector checks game-data contents, not just filenames.

## Launcher controls

| Button | Action |
|:--|:--|
| D-Pad Up / Down | Select a game |
| A | Start a supported game |
| X | View file details |
| B | Back |
| Y | Rescan game files |
| START | Exit |

The in-game control scheme is being refined and may change before the first public release.

## Building from source

Requirements:

- [devkitPro](https://devkitpro.org/) with the Nintendo 3DS toolchain and libctru
- CMake 3.20 or newer
- A working build environment for devkitARM
- Internet access on the first build to fetch the SDL3 source used by the original-engine frontend

From the repository root:

```sh
export DEVKITPRO=/opt/devkitpro   # Change if devkitPro is installed elsewhere
bash platform/3ds/tools/build_original_sdl3.sh
```

The script downloads and builds its required SDL3 version if necessary, then builds the ReflectionHLE-based 3DS executable at:

```text
platform/3ds/cat3ds-original.3dsx
```

For local hardware testing with `3dslink`:

```sh
3dslink platform/3ds/cat3ds-original.3dsx
```

The separate `platform/3ds/Makefile` builds the older experimental `3ds.3dsx` frontend. It is not the main original-engine build described above.

## Known limitations

- Save/load behaviour, particularly in Abyss, still needs verification and fixes.
- Menus and some modal screens are still being adapted to the 3DS layout.
- Stereoscopic depth and performance are not final.
- Armageddon and Apocalypse cannot be launched through the original engines yet.
- Only the listed game editions and compatible original DOS data have been targeted; other releases are not guaranteed to work.

Please report reproducible problems through [GitHub Issues](https://github.com/xXNukem/Catacomb-3ds/issues), including the game and DOS edition you used.

## Credits

**Nintendo 3DS port:** Carlos de la Barrera Pérez ([xXNukem](https://github.com/xXNukem)).

**Original games:** id Software and the original Catacomb creators and publishers.

**Source port foundation:** [ReflectionHLE](https://github.com/ReflectionHLE/ReflectionHLE) and its contributors.

This is an independent, unofficial homebrew project, not affiliated with Nintendo or the original games' rights holders.

## License and game data

The project retains ReflectionHLE's original license and attribution material. See [README_UPSTREAM.md](README_UPSTREAM.md), [AUTHORS.md](AUTHORS.md) and [LICENSES/](LICENSES/).

**Original game data is not distributed here.** You must supply your own game files; their rights remain with their respective owners.
