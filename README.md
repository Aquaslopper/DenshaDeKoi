# DenshaDeKoi

A C++20 / Raylib atlas inspector with a GUI-independent Hanafuda database, deck, and Koi-Koi rules engine. A playable UI and AI are not implemented yet.

## Build and run

Install CMake 3.24 or newer, a C++20 compiler, Git, and the [Raylib platform build dependencies](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux) for your system. CMake fetches Raylib 5.5 automatically.

```sh
cmake -S . -B build
cmake --build build
cd build
./denshadekoi
```

On multi-configuration generators, run the executable from its configuration directory (for example, `build/Debug`). CMake copies `cards_atlas.png` alongside the executable. The application loads that copy regardless of the current working directory, so an unrelated file in that directory cannot be picked up accidentally. You can alternatively pass a path to another atlas as the first argument (relative paths are resolved from the current working directory).

To verify the rules, deck, atlas, rectangle math, asset lookup, grid layout, and hover selection without Raylib, a window system, or network access (the inspector test uses a Raylib test double):

```sh
cmake -S . -B build-headless -DBUILD_DESKTOP_APP=OFF -DBUILD_TESTING=ON
cmake --build build-headless
ctest --test-dir build-headless --output-on-failure
```

When Python 3 is available, CTest also checks the black frame pixels on every card using only the Python standard library.

## Continuous integration

`.github/workflows/build.yml` builds and tests every push to `main` and every pull request on Windows with both MSVC and MinGW-w64, and uploads the compiled `denshadekoi.exe` as a build artifact.

## Usage

The inspector uses columns for months 1–12 and rows for card indices 0–3. Hover a card to see its identifiers; the white outline is drawn outside the card so its original black frame remains visible. Close the window or press Escape to exit.

## Atlas

The supplied source image had headings, margins, and gutters rather than a tightly packed grid. The included `cards_atlas.png` has been normalized by cropping each complete 179×264-pixel card, including its original black border, from that image and packing the crops directly into a 12×4 grid, without resizing. This makes the uniform source rectangle calculation accurate. Replacement atlases should likewise be tightly packed, with equal-sized cells and no gutters. The loader rejects dimensions that are not divisible by 12 columns and 4 rows; this catches the original unnormalized image but cannot detect gutters in every possible image.

## Card database and deck

`hanafuda_core` is a pure C++20 library with no Raylib dependency. `CardData.hpp` defines the card and ribbon types; `Deck.hpp` provides reset, shuffle, draw, and remaining-card operations. The default deck uses `std::random_device` to seed `std::mt19937`; a seed can also be supplied for repeatable tests. `Reset()` restores month/row order without reseeding, while `Shuffle()` rearranges only undrawn cards. `Draw()` returns an empty optional when exhausted.

Numeric card IDs are month-major: `(month - 1) * 4 + cardIndex`, where `cardIndex` is the atlas row. The card names and classifications use traditional Hanafuda identities even where the railway-themed artwork reinterprets a motif. The September row-0 sake cup is a Tane card and is marked with `isSakeCup`. `Deck::ValidateCardDatabase()` is a headless invariant check callable from tests or a debugger.

## KK-ADD-10 rules engine

This core implements the KK-ADD-10 rules from [Xeno-koi at commit `f422bacf2519cdfd1b7582a647015356478ce66b`](https://github.com/Aquaslopper/Xeno-koi/tree/f422bacf2519cdfd1b7582a647015356478ce66b), not its other selectable rulesets. `DeckRegister.hpp` is the single source for all 48 cards: the numeric `Card::id` remains the month-major ordinal and stable IDs such as `01-B` and `09-A` are unchanged.

`KoiKoiRules.hpp` provides Oya-gime card comparison (a tie requires a redraw), dealing, misdeal/opening checks, capture resolution, detailed additive yaku scores, per-yaku snapshots, and settlement. The shuffled permutation deals eight cards to the human, eight to the opponent, eight to the field, then the remainder to stock; the last card in the permutation is the first stock card revealed. The dealer plays first. A field with a four-of-a-month or four pairs is redealt; a single lucky opening wins a flat six points, while two lucky openings draw.

The sake cup counts as both an animal and one phantom plain. Ino-shika-cho stacks with Tane; the individual ribbon sets are replaced by one 10-point Akatan+Aotan yaku when both are present, and Tanzaku stacks from **four** ribbons. After the mandatory hand and stock stages, a decision appears only for a new or grown **individual yaku** since that player's last Koi-Koi call. A new or grown yaku with the current player's hand empty is automatically banked as Shoubu, even when the other player has a card left. Otherwise exhaustion of both hands is a 0–0 draw. Shoubu awards `base × (base >= 7 ? 2 : 1) × (opponent called ? 2 : 1) × min(6, 1 + own calls)`; the winner deals next and a draw retains the dealer.

`KoiKoiStateMachine.hpp` holds one round. Call `Advance()` for automatic phases and use `PlayHandCard`, `ChooseFieldCard`, and `ChooseDecision` for input phases; wrong-phase or invalid actions throw. An uncommitted hand choice can be cancelled; a stock choice cannot. `KoiCalls()` exposes the round call count, `ValidateConservation()` checks all 48 physical cards, and `TryStartDeal()` accepts a complete predetermined permutation for tests or replays. A rejected field leaves the machine in `DEAL_CHECK` for reshuffling. `StartNextRound()` uses the result's next dealer and resets round state.

Xeno-koi's speed minigame and match-end speed bonus are intentionally excluded from DenshaDeKoi.

The existing `std::mt19937` shuffle remains instead of Xeno-koi's xorshift32, so seeds and replay sequences are not interchangeable. The caller chooses or draws the initial dealer; there is no automatic dealer-selection UI, match ledger, match-total integration, AI, persistence, or graphical gameplay yet. The executable remains an atlas inspector.
