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
for game in cat3d catabyss; do
    case "$game" in
        cat3d) defs=(-DREFKEEN_VER_CAT3D); main=c3_main ;;
        catabyss) defs=(-DREFKEEN_VER_CATABYSS -DREFKEEN_VER_CATADVENTURES); main=c4_main ;;
    esac
    engine=src/id91_11/cat_all/$game
    "${CXX:-c++}" "${flags[@]}" "${defs[@]}" -I"$engine" -x c++ tests/3ds_slots_host.cpp \
        "$engine/$main.c" src/id91_11/id_ca.c src/backend/filesystem/be_filesystem_file.c \
        src/be_cross.c src/crc32/crc32.c -o "$work/test"
    mkdir "$work/$game"
    "$work/test" "$work/$game"
    echo "$game save/load: PASS"
done
