#!/usr/bin/env bash
# ============================================================================
#  run.sh - render one viewer script.
#
#    ./run.sh SCRIPT [WIDTH HEIGHT]
#
#  Defaults: 1360 x 768 (the client's design canvas). Frames and text logs go
#  to out/ (or $VIEWER_OUT).
#
#  The game reads its assets relative to the executable directory (Hud.cpp's
#  static textures) and to the working directory (the table background). The
#  viewer runs inside build/run, which is staged to look like the folder the
#  client's CMake copies beside its .exe.
#
#  To look at another tree: DENSHADEKOI_REPO is the tree, VIEWER_BIN the viewer
#  built from it (make B=... REPO=...).
# ============================================================================
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="${DENSHADEKOI_REPO:-$here/../..}"

[ $# -ge 1 ] || { sed -n '3,5p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,2\}//' >&2; exit 2; }
viewer="${VIEWER_BIN:-$here/build/viewer}"
[ -x "$viewer" ] || { echo "run: $viewer is missing; run ./setup.sh and make" >&2; exit 1; }
viewer="$(realpath "$viewer")"
[ -d "$repo/assets" ] || { echo "run: no assets folder in $repo (set DENSHADEKOI_REPO)" >&2; exit 1; }

script="$(realpath "$1")"
shift

run="$here/build/run"
mkdir -p "$run" "$run/saves"

# The client's CMake POST_BUILD step: these sit beside the executable.
ln -sfn "$(realpath "$repo/assets/options")"   "$run/options"
ln -sfn "$(realpath "$repo/assets/fonts")"     "$run/fonts"
ln -sfn "$(realpath "$repo/assets/sfx")"       "$run/sfx"
ln -sfn "$(realpath "$repo/cards_atlas.png")"  "$run/cards_atlas.png"
ln -sfn "$(realpath "$repo/assets/the-city.jpg")" "$run/the-city.jpg"
ln -sfn "$(realpath "$repo/assets/koidenscha.png")" "$run/koidenscha.png"

# A relative VIEWER_OUT is taken from where run.sh was called, not from build/run.
out="${VIEWER_OUT:-$here/out}"
mkdir -p "$out"
export VIEWER_OUT="$(realpath "$out")"
export VIEWER_SAVES="$run/saves"
export VIEWER_RUN_DIR="$run"
# Keep the client's options file inside the viewer's folder, never in $HOME.
export XDG_CONFIG_HOME="$run/saves/config"

cd "$run"
# stdin is closed: the memory platform polls it for the escape key.
exec "$viewer" "$script" "$@" < /dev/null
