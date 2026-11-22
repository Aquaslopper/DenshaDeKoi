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

`DeckRegister.hpp` is the single source for all 48 cards, with frozen `stableId` strings such as `01-B` and their atlas coordinates. The existing numeric `Card::id` remains the month-major ordinal for compatibility. Rules and state APIs accept these ordinals; use `DeckRegister::Find("09-A")` to look up a stable ID. Card names describe traditional identities even where the art reinterprets them.

`KoiKoiRules.hpp` provides deal, misdeal/opening checks, capture resolution, detailed additive yaku scores, and settlement factors. `KoiKoiStateMachine.hpp` holds the round state. Call `Advance()` for automatic phases and use `PlayHandCard`, `ChooseFieldCard`, and `ChooseDecision` for input phases; wrong-phase or invalid actions throw without accepting the command. An uncommitted hand choice can be cancelled; a stock choice cannot. `ValidateConservation()` checks that all 48 physical cards belong to exactly one zone. `TryStartDeal()` accepts a complete predetermined permutation for tests or replays; rejected fields leave the machine in `DEAL_CHECK`, where `Advance()` reshuffles and redeals.

After the mandatory hand and stock stages, a decision is offered only when that player's base yaku score increases. Koi-Koi flags remain sticky for the round. Both hands are played out, the final turn is evaluated, and an exhausted round without Shoubu is a 0–0 draw. The round result includes the next dealer; `StartNextRound()` resets the round using that dealer.

The prior project's requested `std::mt19937` shuffle is retained. Thus the older PDF's xorshift32 seed/replay sequences are not reproduced; the frozen scoring and capture rules here follow the current prompt. There is no match ledger, first-dealer draw, AI, persistence, or graphical play in this core layer.
