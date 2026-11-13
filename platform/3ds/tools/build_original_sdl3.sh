#!/usr/bin/env bash
# Build the *original* ReflectionHLE Catacomb 3-D engine for New 3DS.
# Does not modify or replace the existing catacomb-3ds executable or game files.
set -euo pipefail
cd "$(dirname "$0")/../../.."
ROOT="$PWD"
: "${DEVKITPRO:=/opt/devkitpro}"
export DEVKITPRO
if [[ ! -f "${DEVKITPRO}/cmake/3DS.cmake" ]]; then
  echo "ERROR: missing ${DEVKITPRO}/cmake/3DS.cmake (install devkitPro 3DS CMake support)." >&2
  exit 1
fi
if ! command -v cmake >/dev/null; then
  echo "ERROR: cmake is required." >&2
  exit 1
fi
if [[ ! -f src/id91_11/cat_all/cat3d/c3_main.c || ! -f src/backend/video/be_video_sdl.c ]]; then
  echo "ERROR: run this script from your existing Catacomb 3DS repository." >&2
  exit 1
fi
# The earlier source-only snapshot omitted non-source resource files. The
# original upstream checkout has these files; fail clearly if it does not.
if [[ ! -f rsrc/button_back.xpm || ! -f rsrc/reflection-cat3d-icon-32x32.h ]]; then
  echo "ERROR: ReflectionHLE's upstream resources are missing from this checkout." >&2
  echo "The original engine needs the upstream rsrc/*.xpm and rsrc/*.h files." >&2
  echo "Restore those from the original ReflectionHLE repository before building." >&2
  exit 1
fi

PREFIX="${ROOT}/platform/3ds/.original-sdl3-prefix"
SDL_SOURCE="${ROOT}/platform/3ds/.sdl3-source"
SDL_BUILD="${ROOT}/platform/3ds/.sdl3-build"
SDL_CONFIG="${PREFIX}/lib/cmake/SDL3/SDL3Config.cmake"
if [[ ! -f "${SDL_CONFIG}" ]]; then
  VER="3.4.18"
  ARCHIVE="${ROOT}/platform/3ds/.SDL3-${VER}.tar.gz"
  URL="https://github.com/libsdl-org/SDL/releases/download/release-${VER}/SDL3-${VER}.tar.gz"
  if [[ ! -f "${ARCHIVE}" ]]; then
    echo "Downloading SDL3 ${VER} (official source)..."
    curl -fL --retry 3 --connect-timeout 15 "${URL}" -o "${ARCHIVE}"
  fi
  EXPECTED="9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3"
  ACTUAL="$(shasum -a 256 "${ARCHIVE}" | awk '{print $1}')"
  if [[ "${ACTUAL}" != "${EXPECTED}" ]]; then
    echo "ERROR: SDL3 archive checksum mismatch. Delete ${ARCHIVE} and retry." >&2
    exit 1
  fi
  mkdir -p "${SDL_SOURCE}"
  tar -xzf "${ARCHIVE}" -C "${SDL_SOURCE}" --strip-components=1
  cmake -S "${SDL_SOURCE}" -B "${SDL_BUILD}" \
    -DCMAKE_TOOLCHAIN_FILE="${DEVKITPRO}/cmake/3DS.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
  cmake --build "${SDL_BUILD}" --parallel 4
  cmake --install "${SDL_BUILD}"
fi

ENGINE_BUILD="${ROOT}/platform/3ds/build-original"
cmake -S "${ROOT}/platform/3ds/original" -B "${ENGINE_BUILD}" \
  -DCMAKE_TOOLCHAIN_FILE="${DEVKITPRO}/cmake/3DS.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="${PREFIX}" \
  -DSDL3_DIR="${PREFIX}/lib/cmake/SDL3"
cmake --build "${ENGINE_BUILD}" --parallel 4
OUTPUT="${ENGINE_BUILD}/cat3ds-original.3dsx"
if [[ ! -f "${OUTPUT}" ]]; then
  echo "ERROR: engine build did not produce ${OUTPUT}" >&2
  exit 1
fi
cp "${OUTPUT}" "${ROOT}/platform/3ds/cat3ds-original.3dsx"
echo ""
echo "Original ReflectionHLE engine built successfully:"
echo "  platform/3ds/cat3ds-original.3dsx"
echo "To test: 3dslink platform/3ds/cat3ds-original.3dsx"
echo "The existing platform/3ds/3ds.3dsx and all game data were left untouched."
