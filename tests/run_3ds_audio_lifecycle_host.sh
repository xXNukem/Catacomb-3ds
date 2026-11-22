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

# The installed host SDL uses its own driver. Exercise the actual patched
# close function separately, with instrumented DSP calls and allocations.
driver=platform/3ds/.sdl3-source/src/audio/n3ds/SDL_n3dsaudio.c
if [[ -f "$driver" ]]; then
    sed -n -e '/^static bool N3DSAUDIO_OpenDevice(/,/^}/p' \
        -e '/^static bool N3DSAUDIO_PlayDevice(/,/^}/p' \
        -e '/^static void N3DSAUDIO_CloseDevice(/,/^}/p' "$driver" > "$work/n3ds_audio_close.inc"
    "${CXX:-c++}" -std=c++11 -fsanitize=address,undefined -fno-sanitize-recover=all \
        -I"$work" tests/3ds_ndsp_close_host.cpp $sdl_flags -o "$work/ndsp"
    "$work/ndsp"
    echo 'PASS: N3DS driver close, partial opens, cancellation and linear buffer release (mock DSP)'
else
    echo 'SKIP: N3DS driver close test requires the SDL sources prepared by build_original_sdl3.sh'
fi
