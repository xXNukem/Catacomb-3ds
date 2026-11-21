#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp platform/3ds/refkeen_config.h.in "$work/refkeen_config.h"
flags=(-std=c++11 -fsanitize=address,undefined -fno-sanitize-recover=all -ffunction-sections -fdata-sections -D__3DS__
       -DREFKEEN_UNIFIED_BUILD -DREFKEEN_HAS_VER_CAT3D -DREFKEEN_VER_CATACOMB_ALL
       -I"$work" -Itests/stereo_stubs -Iplatform/3ds/original -Isrc -Isrc/id91_11 -Isrc/id91_11/cat_all)
if [[ $(uname) == Darwin ]]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
"${CC:-cc}" -fsanitize=address,undefined -fno-sanitize-recover=all tests/3ds_stereo_host.c -lm -o "$work/optics"
"$work/optics"
for game in cat3d catabyss catarm catapoc; do
    upper=$(echo "$game" | tr '[:lower:]' '[:upper:]')
    defs=(-DREFKEEN_VER_"$upper" -DREFKEEN_HAS_VER_"$upper" -DTEST_GAME="$game")
    trace=(-x c++)
    if [[ $game != cat3d ]]; then
        defs+=(-DREFKEEN_VER_CATADVENTURES -DREFKEEN_HAS_VER_CATADVENTURES)
        trace=(-x c++ src/id91_11/cat_all/c3_trace.c)
    fi
    "${CXX:-c++}" "${flags[@]}" "${defs[@]}" -Isrc/id91_11/cat_all/"$game" -Isrc/id91_11/cat_all/cat_late \
        tests/3ds_stereo_frame_host.cpp "${trace[@]}" -o "$work/frame"
    "${CXX:-c++}" "${flags[@]}" "${defs[@]}" -Isrc/id91_11/cat_all/"$game" \
        tests/3ds_stereo_sprite_host.cpp -o "$work/sprite"
    "$work/sprite"
    "$work/frame"
    echo "$game mono/stereo frame: PASS"
done
"${CXX:-c++}" -std=c++11 -fsanitize=address,undefined -fno-sanitize-recover=all -Itests/stereo_stubs -Iplatform/3ds/original \
    tests/3ds_stereo_present_host.cpp -o "$work/display"
"$work/display"
