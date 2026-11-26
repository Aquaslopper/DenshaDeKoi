# Densha de Koi headless viewer

Renders the Densha de Koi client to PNG on a machine with no display, no GPU
and no sound device. Adapted from `Aquaslopper/SPACESHIP/tools/viewer`. It
compiles the game's own rules, table renderer, HUD and menu code against
raylib's software renderer (`PLATFORM_MEMORY`), drives the real client state
from a small command script, and writes frames.

Nothing here is built by CMake or CI. No game file is copied or edited:
everything the viewer fetches, builds and renders goes to `build/` and `out/`
in this folder, which git ignores.

## Build

Needs `g++`, `gcc`, `make`, `curl`, `tar`, `patch`.
Network: `codeload.github.com`.

```sh
cd tool/viewer
./setup.sh          # once: fetch, patch and build raylib 6.0 (PLATFORM_MEMORY)
make -j$(nproc)     # build build/viewer
```

## Run

```sh
./run.sh scripts/s01.txt            # 1360 x 768 (design canvas)
./run.sh scripts/s07.txt            # seeded match table
./run.sh scripts/s11.txt            # full scripted match and summary
./run.sh scripts/s13.txt 680 384    # smallest window size
```

Frames go to `out/` (or `$VIEWER_OUT`). With `textlog on`, each `NAME.png` gets
a `NAME.text.tsv` listing every string painted through raylib (`size`, `x`, `y`,
`width`, `font`, `rgba`, `text`).

## Script commands

One command per line. `#` starts a comment.

- **Frames and reports:** `frames N [ms]`, `shot NAME [x y w h]`, `textlog on|off`, `stage`, `overlaps [x y w h]`, `hits [start|hud|result|all]`, `state`.
- **Input:** `move x y`, `click x y`, `rclick x y`, `press x y`, `rpress x y`, `release`, `clickhit KIND [TEXT]`, `rclickhit KIND [TEXT]`, `hoverhit KIND [TEXT]`, `drag x0 y0 x1 y1`, `hold x y N`, `scroll x y delta`, `key CODE`, `keydown CODE`, `keyup CODE`, `escape`.
- **Menus and gallery:** `title`, `variant 1..4`, `menu settings|options|radio|gallery`, `settingsrow 0..2`, `settingsrounds 0..3`, `optionsrow 0..5`, `option master|sfx|hints|mode|aa VALUE`, `radio power|mute|station|state|status|name VALUE`, `radiovolume V`, `gallery MONTH CARD`, `gallerymonth MONTH`, `viewer3d`, `viewerdrag x y dx dy`, `viewerclose`, `loading 1..4`.
- **Match:** `match [ROUNDS [SEED]]`, `matchdealer [ROUNDS]`, `pickdealer 0|1`, `await`, `playcard INDEX`, `playmatch`, `result round|match`.
