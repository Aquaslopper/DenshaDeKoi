#!/usr/bin/env bash
# ============================================================================
#  setup.sh - fetch, patch and build the third-party parts of the viewer.
#  ---------------------------------------------------------------------------
#  Everything lands in build/, beside this script, which git ignores. Nothing
#  is installed on the machine and no tracked file is changed.
#
#  Needs: g++, gcc, make, curl, tar, patch.
#  Network: codeload.github.com.
#
#  Safe to run again: finished steps are skipped.
# ============================================================================
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
deps="$here/build/deps"
mkdir -p "$deps"

fetch() {  # fetch <directory the tarball unpacks to> <url>
  local dir="$1" url="$2"
  if [ -d "$deps/$dir" ]; then echo "have   $dir"; return; fi
  echo "fetch  $dir"
  curl -fsSL "$url" -o "$deps/$dir.tar.gz"
  tar -xzf "$deps/$dir.tar.gz" -C "$deps"
  rm -f "$deps/$dir.tar.gz"
  [ -d "$deps/$dir" ] || { echo "setup: $url did not unpack to $dir" >&2; exit 1; }
}

fetch raylib-6.0 https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/6.0

apply() {  # apply <source root> <patch file>; a patch already in place is left alone
  local root="$1" file="$2"
  if patch -p1 -d "$root" --dry-run -R -s -f < "$file" > /dev/null 2>&1; then
    echo "have   $(basename "$file")"
  else
    echo "patch  $(basename "$file")"
    patch -p1 -d "$root" -s < "$file"
  fi
}

# Two corrections to the private copy of rlsw.h, without which the software
# renderer draws clipped regions at the wrong height and drops texture alpha.
# README.md says what each one does.
apply "$deps/raylib-6.0" "$here/patches/raylib-6.0-rlsw.patch"

# raylib for PLATFORM_MEMORY: frames are rasterized into memory, no display.
# rlsw.h is a header inside rcore.c and the raylib Makefile does not track it.
if [ ! -f "$deps/raylib-6.0/src/libraylib.a" ] ||
   [ "$deps/raylib-6.0/src/external/rlsw.h" -nt "$deps/raylib-6.0/src/libraylib.a" ]; then
  echo "build  raylib (PLATFORM_MEMORY)"
  touch "$deps/raylib-6.0/src/rcore.c"
  make -C "$deps/raylib-6.0/src" -j"$(nproc)" PLATFORM=PLATFORM_MEMORY \
       CUSTOM_CFLAGS="-DSUPPORT_FILEFORMAT_JPG=1 -O2" > "$here/build/raylib-build.log" 2>&1 ||
    { tail -30 "$here/build/raylib-build.log" >&2; exit 1; }
else
  echo "have   libraylib.a"
fi

echo "done.  next: make && ./run.sh scripts/s01.txt"
