#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp platform/3ds/refkeen_config.h.in "$work/refkeen_config.h"
flags=(-std=c++11 -ffunction-sections -fdata-sections -D__3DS__ -DREFKEEN_UNIFIED_BUILD
       -DREFKEEN_HAS_VER_CAT3D -DREFKEEN_HAS_VER_CATABYSS -DREFKEEN_HAS_VER_CATACOMB_ALL
       -DREFKEEN_HAS_VER_CATADVENTURES -DREFKEEN_VER_CATACOMB_ALL
       -I"$work" -Iplatform/3ds/original -Isrc -Isrc/id91_11 -Isrc/id91_11/cat_all)
if [[ $(uname) == Darwin ]]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
for game in cat3d catabyss catarm catapoc; do
    extra=(tests/3ds_slots_host.cpp)
    case "$game" in
        cat3d) defs=(-DREFKEEN_VER_CAT3D); main=c3_main ;;
        catarm) defs=(-DREFKEEN_HAS_VER_CATARM -DREFKEEN_VER_CATARM -DREFKEEN_VER_CATADVENTURES -Isrc/id91_11/cat_all/cat_late); main=c5_main; extra+=(tests/3ds_arm_resources_host.cpp src/backend/startup/be_startup_embedded_data.c platform/3ds/source/cat3ds_game_files.cpp) ;;
        catapoc) defs=(-Iplatform/3ds/source -DREFKEEN_HAS_VER_CATAPOC -DREFKEEN_VER_CATAPOC -DREFKEEN_VER_CATADVENTURES -Isrc/id91_11/cat_all/cat_late); main=c6_main; extra+=(tests/3ds_apocalypse_install_host.cpp src/backend/filesystem/be_filesystem_gameinst.c tests/3ds_apocalypse_controls_host.cpp src/id91_11/cat_all/altcontroller.c src/backend/input/be_input_tables.c tests/3ds_arm_resources_host.cpp src/backend/startup/be_startup_embedded_data.c platform/3ds/source/cat3ds_game_files.cpp) ;;
        catabyss) defs=(-DREFKEEN_VER_CATABYSS -DREFKEEN_VER_CATADVENTURES); main=c4_main ;;
    esac
    engine=src/id91_11/cat_all/$game
    if [[ $game == catapoc ]]; then
        # Provide storage for actors without linking their simulation into the host harness.
        { echo '#include "def.h"'; echo 'REFKEEN_NS_B'
          sed -n 's/^extern statetype \(s_[A-Za-z0-9_]*;\)$/statetype \1/p' "$engine/statetype_ptr_conversion.c"
          echo 'REFKEEN_NS_E'
        } > "$work/apoc_states.cpp"
        extra+=("$work/apoc_states.cpp" "$engine/statetype_ptr_conversion.c")
    fi
    "${CXX:-c++}" "${flags[@]}" "${defs[@]}" -I"$engine" -x c++ \
        "${extra[@]}" "$engine/$main.c" src/id91_11/id_ca.c src/backend/filesystem/be_filesystem_file.c \
        src/be_cross.c src/crc32/crc32.c -o "$work/test"
    mkdir "$work/$game"
    "$work/test" "$work/$game"
    echo "$game save/load: PASS"
    "${CXX:-c++}" "${flags[@]}" "${defs[@]}" -I"$engine" -x c++ tests/3ds_arm_audio_host.cpp \
            src/id91_11/id_sd.c -o "$work/audio"
    "$work/audio"
done
