#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
flags=(-std=c++11 -fsanitize=address,undefined -fno-sanitize-recover=all)
"${CXX:-c++}" "${flags[@]}" tests/3ds_pad_host.cpp $(pkg-config --cflags --libs sdl3) -o "$work/pad"
"$work/pad"
echo 'PASS: physical SDL button mapping'
cp platform/3ds/refkeen_config.h.in "$work/refkeen_config.h"
flags+=(-ffunction-sections -fdata-sections -D__3DS__ -DREFKEEN_UNIFIED_BUILD
        -DREFKEEN_VER_CATACOMB_ALL -DREFKEEN_HAS_VER_CATACOMB_ALL
        -DREFKEEN_HAS_VER_CAT3D -DREFKEEN_HAS_VER_CATABYSS -DREFKEEN_HAS_VER_CATADVENTURES
        -DREFKEEN_HAS_VER_CATARM -DREFKEEN_HAS_VER_CATAPOC
        -I"$work" -Isrc -Isrc/id91_11 -Isrc/id91_11/cat_all -Iplatform/3ds/original)
if [[ $(uname) == Darwin ]]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
for game in cat3d catabyss catarm catapoc; do
    upper=$(echo "$game" | tr '[:lower:]' '[:upper:]')
    defs=(-DTEST_GAME="$game" -DREFKEEN_VER_"$upper" -DREFKEEN_HAS_VER_"$upper")
    if [[ $game != cat3d ]]; then defs+=(-DREFKEEN_VER_CATADVENTURES -DREFKEEN_HAS_VER_CATADVENTURES); fi
    "${CXX:-c++}" "${flags[@]}" "${defs[@]}" -Isrc/id91_11/cat_all/"$game" -x c++ \
        tests/3ds_bindings_host.cpp src/id91_11/cat_all/altcontroller.c \
        src/backend/input/be_input_tables.c -o "$work/bindings"
    "$work/bindings"
    "${CXX:-c++}" "${flags[@]}" "${defs[@]}" -Itests/stereo_stubs tests/3ds_menu_host.cpp -o "$work/menu"
    "$work/menu"
    if [[ $game == cat3d ]]; then
        "${CXX:-c++}" "${flags[@]}" "${defs[@]}" -Isrc/id91_11/cat_all/cat3d -x c++ \
            tests/3ds_update_blocks_host.cpp src/id91_11/id_vw_ae.c -o "$work/blocks"
        "$work/blocks"
    fi
    deathSources=(src/id91_11/cat_all/"$game"/*_wiz.c)
    sed -n -e '/^#define[[:space:]]*PLAYERSPEED/p' -e '/^#define[[:space:]]*RUNSPEED/p' -e '/^void ControlMovement (objtype \*ob)$/,/^}/p' "${deathSources[0]}" > "$work/movement.inc"
    "${CXX:-c++}" "${flags[@]}" "${defs[@]}" -Isrc/id91_11/cat_all/"$game" tests/3ds_turn_host.cpp -o "$work/turn"
    "$work/turn"
    if [[ $game == cat3d ]]; then deathSources+=(src/id91_11/cat_all/cat3d/c3_game.c); fi
    "${CXX:-c++}" "${flags[@]}" "${defs[@]}" -Itests/stereo_stubs -Isrc/id91_11/cat_all/"$game" \
        -Isrc/id91_11/cat_all/cat_late -x c++ tests/3ds_death_host.cpp "${deathSources[@]}" src/be_cross.c -o "$work/death"
    "$work/death"
    echo "$game lethal damage and death presentation: PASS"
    echo "$game native bindings, modal dispatch, frozen clock and session navigation: PASS"
done
"${CXX:-c++}" -std=c++11 -fsanitize=address,undefined -fno-sanitize-recover=all \
    $(pkg-config --cflags sdl3) -Itests/stereo_stubs -Iplatform/3ds/original -Iplatform/3ds/source \
    tests/3ds_selector_host.cpp $(pkg-config --libs sdl3) -o "$work/selector"
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy "$work/selector" > "$work/selector.log"
echo 'PASS: selector navigation, cancel exit, unavailable data, same and different game, SDL restart'
"${CXX:-c++}" -std=c++11 -fsanitize=address,undefined -fno-sanitize-recover=all \
    tests/3ds_installation_host.cpp -o "$work/installation"
"$work/installation" "$work"
echo 'PASS: renamed executables, archive CRCs and edition-specific table fingerprints'
"${CXX:-c++}" -std=c++11 -fsanitize=address,undefined -fno-sanitize-recover=all tests/3ds_mz_host.cpp -o "$work/mz"
"$work/mz"
echo 'PASS: bounded uncompressed MZ image and malformed headers'
"${CXX:-c++}" -std=c++11 tests/3ds_wall_clearance_host.cpp -o "$work/clearance"
"$work/clearance"
echo 'PASS: solid walls and four corners, approach and retreat'
