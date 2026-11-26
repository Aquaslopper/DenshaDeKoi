#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
[ $# -eq 1 ] || { echo "Usage: $0 SCRIPT" >&2; exit 2; }
script="$(realpath "$1")"
out="${VIEWER_OUT:-$here/out}"
mkdir -p "$out"
export VIEWER_OUT="$(realpath "$out")"
# HUD assets are resolved relative to the executable by the game's renderer.
ln -sfn "$(realpath "$here/../../assets/options")" "$here/build/options"
ln -sfn "$(realpath "$here/../../cards_atlas.png")" "$here/build/cards_atlas.png"
exec "$here/build/viewer" "$script" "$here/../.." < /dev/null
