# Bundled fonts

The pause menu and HUD are drawn with these open fonts (all SIL Open Font License 1.1):

## Silkscreen (SIL OFL 1.1)
Copyright 2001 The Silkscreen Project Authors (https://github.com/googlefonts/silkscreen).
Files: `Silkscreen-Regular.ttf`, `Silkscreen-Bold.ttf`. License text: `Silkscreen-OFL.txt`.
Used for the PAUSE / MENU / PAUSED title bar, the OFF/MUTE buttons, BGM / STATION / 70% labels.

## VT323 (SIL OFL 1.1)
Copyright 2011 The VT323 Project Authors (peter.hull@oikoi.com).
File: `VT323.ttf` (converted from the upstream WOFF2 build, https://github.com/googlefonts/vt323).
License: SIL Open Font License 1.1, http://scripts.sil.org/OFL.
Used for the station readout, station buttons, and footer hints.

## densha-logo.png
The DENSHA DE KOI wordmark used in the pause-menu title bar (190x66 RGBA,
project design asset supplied for this redesign). Embedded into the binary
like the fonts; regenerate via `tools/embed_assets.py`.

## Regenerating `src/EmbeddedAssets.hpp`

The fonts and the wordmark are compiled straight into the game (the pause menu
must render even when no asset folder ships with the executable). After
changing any of these files, regenerate the embedded arrays with:

    python3 tools/embed_assets.py

The generated `src/EmbeddedAssets.hpp` is committed on purpose; the header
documents what it contains.
