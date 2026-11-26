# Densha de Koi headless viewer

Adapted from [SPACESHIP's headless viewer](https://github.com/Aquaslopper/SPACESHIP/tree/main/tools/viewer). This version renders Densha de Koi's own start menu, match settings, options, radio, card gallery and a seeded match using raylib's software renderer, without a display, GPU or audio device. It reads the game's source and assets without modifying them. The SPACESHIP-specific simulation, commands and text recorder are not applicable and are not included.

## Build and run

On Linux, install `g++` (C++20), `gcc`, `make`, `curl`, `tar` and `patch`, then:

```sh
cd tools/viewer
./setup.sh
make -j2
VIEWER_OUT=../../screenshots ./run.sh scripts/wiki.txt
```

Setup downloads raylib 6.0 from GitHub and applies the SPACESHIP viewer's software-renderer patch to the private copy in `build/`. The game itself uses raylib 5.5. Build files and the default `out/` are ignored. `VIEWER_OUT` is relative to the calling directory; existing files with the same names are overwritten.

Scripts contain one command per line; comments start with `#`:

- `scene start|settings|options|radio|gallery|match` selects a scene.
- `play` advances the seeded match to the player’s turn and drags a matching hand card onto the field. `scripts/match-card-play.txt` captures its landing.
- `variant 1..4` selects a start-menu background.
- `month 1..12` selects a gallery month.
- `shot NAME` renders a 1360 × 768 PNG named `NAME.png`. Names may contain letters, numbers, hyphens and underscores.

The script exits nonzero on invalid commands or failed writes. The match uses the actual rules and table controller, driven by a deterministic scripted move; it is not a recording of a desktop session. Menu scenes are representative states, not interactive gameplay. They use the actual menu drawing code and original assets, but software-rendered output is not pixel-identical to the desktop game: GPU mipmaps, antialiasing and shader effects may differ. No radio connection is made. Gallery transitions are allowed to settle before a capture.

The software renderer cannot compile the magnification shader. `prepare-table.py` makes a private build copy of the table renderer without that shader; the capture uses the default, unmagnified view.
