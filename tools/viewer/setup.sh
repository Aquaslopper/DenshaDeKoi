#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build/deps
if [ ! -d build/deps/raylib-6.0 ]; then
  curl -fsSL https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/6.0 | tar -xz -C build/deps
fi
if ! patch -p1 -d build/deps/raylib-6.0 --dry-run -R -s -f < patches/raylib-6.0-rlsw.patch >/dev/null 2>&1; then
  patch -p1 -d build/deps/raylib-6.0 < patches/raylib-6.0-rlsw.patch
fi
make -C build/deps/raylib-6.0/src -j"${JOBS:-2}" PLATFORM=PLATFORM_MEMORY CUSTOM_CFLAGS='-DSUPPORT_FILEFORMAT_JPG=1 -O2'
