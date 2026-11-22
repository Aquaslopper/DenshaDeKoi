# DenshaDeKoi

A C++20 / Raylib atlas inspector for the 48-card Hanafuda deck. This is a rendering foundation only; there is no gameplay yet.

## Build and run

Install CMake 3.24 or newer, a C++20 compiler, Git, and the [Raylib platform build dependencies](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux) for your system. CMake fetches Raylib 5.5 automatically.

```sh
cmake -S . -B build
cmake --build build
cd build
./denshadekoi
```

On multi-configuration generators, run the executable from its configuration directory (for example, `build/Debug`). CMake copies `cards_atlas.png` alongside the executable. Run it from that directory so it can find the atlas. You can alternatively pass a path to another atlas as the first argument.

To verify the atlas and its shared rectangle math without Raylib, a window system, or network access:

```sh
cmake -S . -B build-headless -DBUILD_DESKTOP_APP=OFF -DBUILD_TESTING=ON
cmake --build build-headless
ctest --test-dir build-headless --output-on-failure
```

The inspector uses columns for months 1–12 and rows for card indices 0–3. Hover a card to see its identifiers. Close the window or press Escape to exit.

## Atlas

The supplied source image had headings, margins, and gutters rather than a tightly packed grid. The included `cards_atlas.png` has been normalized by cropping each card's 163×249-pixel artwork from that image and packing the crops directly into a 12×4 grid, without resizing. This makes the uniform source rectangle calculation accurate. Replacement atlases should likewise be tightly packed, with equal-sized cells and no gutters.
