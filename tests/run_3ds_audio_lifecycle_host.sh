#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp platform/3ds/refkeen_config.h.in "$work/refkeen_config.h"
sdl_flags=$(pkg-config --cflags --libs sdl3)
"${CXX:-c++}" -std=c++11 -fsanitize=address,undefined -fno-sanitize-recover=all \
    -DREFKEEN_CONFIG_THREADS -I"$work" -Isrc -x c++ tests/3ds_audio_lifecycle_host.cpp \
    src/backend/audio/be_audio_{init,sdl,main_thread,resampling,pcspkr,opl}.c src/nukedopl/opl3.c -x none \
    $sdl_flags -o "$work/audio"
"$work/audio"
