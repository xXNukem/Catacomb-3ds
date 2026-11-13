#!/usr/bin/env python3
"""Keep the EGA game-selection menu and start the original ReflectionHLE engine.

This applies narrow, idempotent edits to existing upstream files and backs
up each original file before the first modification. No game data is touched.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
backup_root = root / "platform/3ds/.backup-before-menu-engine-fix"


def require_replace(data, old, new, label):
    if new in data:
        return data
    if old not in data:
        raise ValueError(f"{label}: expected unmodified source block not found; not overwriting")
    if data.count(old) != 1:
        raise ValueError(f"{label}: source block occurs multiple times; not overwriting")
    return data.replace(old, new)


changes = {}

path = "platform/3ds/original/refkeen_original_main.c"
data = (root / path).read_text()
old = '''    if (chdir("sdmc:/3ds/catacomb-3ds/games/cat3d") != 0)
    {
        /* Do not perform game initialization without the original data. */
        return 1;
    }
'''
new = '''    /* Keep the original Catacomb 3DS EGA game-selection menu. The menu is
     * displayed before starting the ReflectionHLE engine, and is shut down
     * before BE_ST_InitCommon initializes SDL for the game itself. */
    extern int cat3dsSelectGame(void);
    if (cat3dsSelectGame() != 0)
        return 0; /* User chose Exit; later games remain selectable but unported. */

    if (chdir("sdmc:/3ds/catacomb-3ds/games/cat3d") != 0)
    {
        /* Do not perform game initialization without the original data. */
        return 1;
    }
'''
changes[path] = require_replace(data, old, new, path)

path = "platform/3ds/original/CMakeLists.txt"
data = (root / path).read_text()
data = require_replace(data, 'project(Catacomb3DSOriginalEngine LANGUAGES C)',
                       'project(Catacomb3DSOriginalEngine LANGUAGES C CXX)', path)
data = require_replace(
    data,
    'set(ENGINE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/refkeen_original_main.c")',
    '''# The SDL3 menu keeps the original four-game Catacomb 3DS selector.
# On launch it shuts down SDL and hands over to ReflectionHLE's normal entry.
set(ENGINE_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/refkeen_original_main.c"
  "${CMAKE_CURRENT_LIST_DIR}/cat3ds_sdl_launcher.cpp"
  "${ROOT}/platform/3ds/source/cat3ds_game_files.cpp"
  "${ROOT}/platform/3ds/source/cat3ds_resource_probe.cpp")''', path)
data = require_replace(
    data,
    '  "${ROOT}/platform/3ds/include"',
    '  "${ROOT}/platform/3ds/include"\n  "${ROOT}/platform/3ds/source"', path)
# The SDL3 target alone may not export libctru's headers to consumers.
data = require_replace(
    data,
    'add_executable(cat3ds-original ${ENGINE_SOURCES})',
    'add_executable(cat3ds-original ${ENGINE_SOURCES})\n'+
    '''if(EXISTS "$ENV{DEVKITPRO}/libctru/include/3ds.h")
  target_include_directories(cat3ds-original PRIVATE "$ENV{DEVKITPRO}/libctru/include")
endif()''', path)
# The menu uses libctru for the bottom screen and D-pad. SDL3 already
# depends on libctru; declare the dependency explicitly as well.
data = require_replace(
    data,
    'target_link_libraries(cat3ds-original PRIVATE SDL3::SDL3 m)',
    'target_link_libraries(cat3ds-original PRIVATE SDL3::SDL3 m ctru)', path)
changes[path] = data

path = "src/id91_11/id_ca.c"
data = (root / path).read_text()
old = '''id0_boolean_t CA_FarRead (BE_FILE_T handle, id0_byte_t id0_far *dest, id0_long_t length)
{
\tif (length>0xffffl)
\t\tQuit ("CA_FarRead doesn't support 64K reads yet!");
\t// Ported from ASM
\tint bytesread = BE_Cross_readInt8LEBuffer(handle, dest, length);
\tif (bytesread == 0)
\t{
\t\t// Keep errno as set by read
\t\treturn false;
\t}

\tif (bytesread != length)
\t{
\t\t/* FIXME (REFKEEN) - Is that the right way? */
\t\terrno = 11;
\t\t//errno = EINVFMT; // user manager knows this is bad read
\t\treturn false;
\t}
\treturn true;
}'''
new = '''id0_boolean_t CA_FarRead (BE_FILE_T handle, id0_byte_t id0_far *dest, id0_long_t length)
{
\t/* Catacomb 3DS: The old 64-KiB limit comes from segmented DOS memory.
\t * On ARM, dest is a normal contiguous buffer. Read in bounded chunks so
\t * large graphics/audio resources work without truncated or unchecked IO. */
\tif (length < 0)
\t{
\t\terrno = EINVAL;
\t\treturn false;
\t}
\twhile (length > 0)
\t{
\t\tsize_t requested = (length > 32768) ? 32768 : (size_t)length;
\t\tsize_t bytesread = BE_Cross_readInt8LEBuffer(handle, dest, requested);
\t\tif (bytesread != requested)
\t\t{
\t\t\tif (!ferror(handle))
\t\t\t\terrno = EIO;
\t\t\treturn false;
\t\t}
\t\tdest += bytesread;
\t\tlength -= (id0_long_t)bytesread;
\t}
\treturn true;
}'''
changes[path] = require_replace(data, old, new, path)

path = "src/id91_11/cat_all/cat3d/c3_game.c"
data = (root / path).read_text()
old = '''void FizzleFade (id0_unsigned_t source, id0_unsigned_t dest,
\tid0_unsigned_t width,id0_unsigned_t height, id0_boolean_t abortable)
{
'''
new = old + '''#ifdef __3DS__
\t/* SDL3 currently presents the EGA framebuffer in software on 3DS.
\t * The DOS pixel-by-pixel fade costs seconds per transition on hardware.
\t * Keep the exact final frame while bypassing only the transitional effect. */
\t(void)abortable;
\tVW_SetScreen (dest,0);
\tVW_ScreenToScreen (source,dest,width/8,height);
\treturn;
#endif
'''
changes[path] = require_replace(data, old, new, path)

if not (root / "platform/3ds/original/cat3ds_sdl_launcher.cpp").is_file():
    sys.exit("Missing cat3ds_sdl_launcher.cpp: unzip the whole patch first")

# Preflight all files before changing any.
for path in changes:
    if not (root / path).is_file():
        sys.exit(f"Missing file: {path}")

modified = []
for path, data in changes.items():
    dest = root / path
    original = dest.read_text()
    if data == original:
        continue
    backup = backup_root / path
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        backup.write_bytes(dest.read_bytes())
    dest.write_text(data)
    modified.append(path)

print("Catacomb 3DS: menu + ReflectionHLE integration applied")
for path in modified:
    print("  updated:", path)
if not modified:
    print("  already applied (no changes)")
print("  source backups:", backup_root.relative_to(root))
print("  original standalone launcher remains untouched: platform/3ds/3ds.3dsx")
print("  commercial game files: untouched")
