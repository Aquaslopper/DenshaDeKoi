# Visual-polish implementation report

**Date:** 2026-11-26 · **Branch:** `arena/c1c516c2-denshadekoi` · **Base / `origin/main`:** `de4a466d5ab03f98096932b286fc12cd3dd760ae`

## Result

Implemented only the explicitly approved in-match visual treatment:

- RADIO / PAUSE / HOME are now small navy, bevel-edged signage plates with the existing hit rectangles.
- The current-round counter sits on a centered navy/bevel plate at logical `{574, 9, 212, 48}`; the anchor remains x=680.
- Koi-Koi, Shobu, round-end actions, and the dealer prompt use matching bilingual signage. Hover/pressed presentation is render-only.
- Card shadows use a faint warm ambient feather and a tighter cool contact feather. No card transform, outline, card art, camera, table layout, or projection was changed.
- Added the four HUD texture assets listed below. No C++ dependency, target, CMake, city-image, existing option-asset name, or atlas-slicing change was made.

PAUSE and RADIO menu implementation/art/layout/input were not changed. The HUD behind them changes because the approved round plate and entry plates are part of the in-match HUD.

## Before / after visual pairs

All files in the pair list are 1360×768. **Important capture limitation:** the repository has authentic baseline screenshots for the table, PAUSE, and RADIO, but this environment has no usable desktop X11 development files. Therefore no actual post-change Raylib window capture could be made. Every `*-after-mock.jpg` is a composited visual study, not a runtime screenshot. The dealer, decision, and round-end “before” images are likewise reconstructed overlays on the table reference, not recorded game states. These pairs are useful for layout review only; they do not verify final GPU rendering.

| State | Before | After / mock |
|---|---|---|
| Table / match HUD | [table-before.jpg](visual-polish/table-before.jpg) (repository screenshot) | [table-after-mock.jpg](visual-polish/table-after-mock.jpg) |
| Dealer selection | [dealer-before-mock.jpg](visual-polish/dealer-before-mock.jpg) (reconstruction) | [dealer-after-mock.jpg](visual-polish/dealer-after-mock.jpg) |
| Koi-Koi / Shobu decision | [decision-before-mock.jpg](visual-polish/decision-before-mock.jpg) (reconstruction) | [decision-after-mock.jpg](visual-polish/decision-after-mock.jpg) |
| Round end | [round-end-before-mock.jpg](visual-polish/round-end-before-mock.jpg) (reconstruction) | [round-end-after-mock.jpg](visual-polish/round-end-after-mock.jpg) |
| PAUSE | [pause-before.jpg](visual-polish/pause-before.jpg) (repository screenshot) | [pause-after-mock.jpg](visual-polish/pause-after-mock.jpg) |
| RADIO | [radio-before.jpg](visual-polish/radio-before.jpg) (repository screenshot) | [radio-after-mock.jpg](visual-polish/radio-after-mock.jpg) |

The PAUSE/RADIO composites retain the supplied menu screenshot pixels and replace only the dimmed top-center round-counter and right-side HUD regions. They are not captures of a changed pause/radio menu. The table reference already contains the old shadow rendering, so it is not a clean pixel-isolated shadow comparison; the supplemental [card contact-shadow detail mock](proposals/card-contact-shadow-detail-mock.jpg) is also only illustrative.

## Regression coverage and verification

### Headless build and tests

The pre-change baseline was built and tested before implementation; it also passed all 9 CTest tests and verified the 17-file protected contract.

Commands run for the post-change verification:

```text
/tmp/cmake-tool/cmake/data/bin/cmake --build build-headless --parallel 2
/tmp/cmake-tool/cmake/data/bin/ctest --test-dir build-headless --output-on-failure
```

Result: the protected-contract target verified 17 files; the headless build succeeded; CTest passed **9/9**:

```text
1/9  table         Passed
2/9  pause_options Passed
3/9  table_game    Passed
4/9  radio         Passed
5/9  rules         Passed
6/9  deck          Passed
7/9  atlas_math    Passed
8/9  atlas_frames  Passed
9/9  inspector     Passed
100% tests passed, 0 tests failed out of 9
```

Exact CTest output:

```text
Internal ctest changing into directory: /home/user/DenshaDeKoi/build-headless
Test project /home/user/DenshaDeKoi/build-headless
    Start 1: table
1/9 Test #1: table ............................   Passed    0.05 sec
    Start 2: pause_options
2/9 Test #2: pause_options ....................   Passed    0.02 sec
    Start 3: table_game
3/9 Test #3: table_game .......................   Passed    0.93 sec
    Start 4: radio
4/9 Test #4: radio ............................   Passed    0.03 sec
    Start 5: rules
5/9 Test #5: rules ............................   Passed    0.78 sec
    Start 6: deck
6/9 Test #6: deck .............................   Passed    0.00 sec
    Start 7: atlas_math
7/9 Test #7: atlas_math .......................   Passed    0.00 sec
    Start 8: atlas_frames
8/9 Test #8: atlas_frames .....................   Passed    2.09 sec
    Start 9: inspector
9/9 Test #9: inspector ........................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 9

Total Test time (real) =   3.92 sec
```

The table test now records HUD draw calls and shadow vertices. It asserts the fixed side-button rectangles `{1266,322,76,58}`, `{1266,390,76,58}`, `{1266,458,76,58}`, the decision rectangles `{505,336,165,48}` / `{690,336,165,48}`, and round-end `{520,326,320,72}`. It also checks the centered plate at 680×384, visual states and labels, and each card’s expected top vertices against the shadow pass (zero card-outline vertex movement). The fake Raylib surface was extended only for mouse-button state and draw-call capture.

### Protected-file self-check

The protected-file grep self-check was restricted to the task’s approved Lane C allowlist. Its complete output was:

```text
.github/protected-contract.sha256
src/TableScene.cpp
src/ui/Hud.cpp
```

No other protected path was changed. `sha256sum -c .github/protected-contract.sha256` passed all 17 entries, and `git diff --check` passed.

Only the `src/TableScene.cpp` line in `.github/protected-contract.sha256` was refreshed: old digest `1988817cd277f63ea3358a65e5a5940b34f14da64a389432c37c4b90b4f1ff1e`; new digest `ce1873c58902f80ed73d1a0d4b8f97886a8fe9219d527696759d3a26c780fcbc`.

### Desktop build status

Attempted:

```text
/tmp/cmake-tool/cmake/data/bin/cmake -S . -B build-desktop-check -DBUILD_DESKTOP_APP=ON
```

Configuration stopped in Raylib/GLFW before the desktop target could build:

```text
Could NOT find X11 (missing: X11_X11_INCLUDE_PATH X11_X11_LIB)
```

This is an environment dependency failure, not a successful desktop build. No live runtime or genuine target-state screenshots were available.

### Fetch / base check

`git fetch origin main` completed. At verification time, both `HEAD` and `origin/main` resolved to `de4a466d5ab03f98096932b286fc12cd3dd760ae`.

## Protection mismatch noted, not changed

`.github/PROTECTED_AREAS.md` enumerates only `src/ui/UiViewport.hpp` and `src/ui/PauseRadioMenu.*` under `src/ui/`. However, `.github/CODEOWNERS` protects `/src/ui/`, and `.github/workflows/protected-areas.yml` also matches `src/ui/*`. All of `src/ui/` was therefore treated as locked; only the expressly approved `src/ui/Hud.cpp` edit was made. This policy mismatch was not resolved. `src/ui/Hud.hpp` and `src/TableScene.hpp` were not changed.

## Exact protected-file list

1. `src/ui/Hud.cpp` — approved HUD-only rendering changes.
2. `src/TableScene.cpp` — approved shadow-only rendering changes.
3. `.github/protected-contract.sha256` — only the `src/TableScene.cpp` digest entry refreshed as expressly approved.

No other protected files were edited.

## Every changed file

### Protected / approved

- `.github/protected-contract.sha256`
- `src/TableScene.cpp`
- `src/ui/Hud.cpp`

### Tests

- `tests/fake_raylib/raylib.h`
- `tests/test_table.cpp`

### New HUD art

- `assets/options/match-hud/home-icon.png`
- `assets/options/match-hud/hud-kana.png`
- `assets/options/match-hud/pause-icon.png`
- `assets/options/match-hud/radio-icon.png`

### Reports and visual files

- `docs/visual-polish-report.md`
- `docs/visual-polish-proposals.md`
- `docs/proposals/card-contact-shadow-detail-mock.jpg`
- `docs/proposals/dealer-after-mock.jpg`
- `docs/proposals/dealer-before-mock.jpg`
- `docs/proposals/decision-after-mock.jpg`
- `docs/proposals/decision-before-mock.jpg`
- `docs/proposals/lane-b-pause-chrome-study.jpg`
- `docs/proposals/lane-b-radio-status-study.jpg`
- `docs/proposals/pause-after-mock.jpg`
- `docs/proposals/radio-after-mock.jpg`
- `docs/proposals/round-end-after-mock.jpg`
- `docs/proposals/round-end-before-mock.jpg`
- `docs/proposals/table-after-mock.jpg`
- `docs/visual-polish/dealer-after-mock.jpg`
- `docs/visual-polish/dealer-before-mock.jpg`
- `docs/visual-polish/decision-after-mock.jpg`
- `docs/visual-polish/decision-before-mock.jpg`
- `docs/visual-polish/pause-after-mock.jpg`
- `docs/visual-polish/pause-before.jpg`
- `docs/visual-polish/radio-after-mock.jpg`
- `docs/visual-polish/radio-before.jpg`
- `docs/visual-polish/round-end-after-mock.jpg`
- `docs/visual-polish/round-end-before-mock.jpg`
- `docs/visual-polish/table-after-mock.jpg`
- `docs/visual-polish/table-before.jpg`

## Could not verify

- A real desktop compile/link/run, because X11 development headers and libraries are missing.
- Actual post-change 1360×768 captures and final visual rendering on Raylib/GPU, including the perceived shadow softness, kana/icon filtering, and hover/pressed appearance.
- Runtime loading/appearance of the new textures in a packaged desktop build; the headless test stubs validate the requested asset names and draw calls, but do not load them through a live Raylib context.

The Lane B studies in [`docs/visual-polish-proposals.md`](visual-polish-proposals.md) are deliberately unapproved and are not implemented. No Wiki work was in scope.
