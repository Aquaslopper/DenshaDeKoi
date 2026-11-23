# Protected areas

The following files form the locked product contract for **DenshaDeKoi**. They must not change without
explicit approval from the repository owner (`@Aquaslopper`).

## Game and Koi-Koi rules

- `src/KoiKoiRules.cpp`, `src/KoiKoiRules.hpp`
- `src/KoiKoiProfile.hpp`
- `src/KoiKoiStateMachine.cpp`, `src/KoiKoiStateMachine.hpp`
- `src/TableGame.cpp`, `src/TableGame.hpp`
- `src/Deck.cpp`, `src/Deck.hpp`
- `src/DeckRegister.hpp`

This protects dealing, captures, scoring, combinations, turns, Koi-Koi decisions, and added/custom card rules.

## Card identities and card design

- `src/CardData.hpp`
- `src/AtlasLayout.hpp`
- `src/AtlasManager.cpp`, `src/AtlasManager.hpp`
- `cards_atlas.png`
- `assets/koidenscha.png`
- `assets/fonts/`

This protects every card's identity, atlas coordinates, dimensions, artwork, rendering lookup, and the supplied game logo.

## Table layout and camera

- `src/TableLayout.hpp`
- `src/TableScene.cpp`, `src/TableScene.hpp`
- `src/TableHud.hpp`

This protects where cards lie on the table, hand/field/stock/capture positions, projection, hit testing,
camera calibration, and the established table presentation.

## Guardrails themselves

- `AGENTS.md`
- `.github/PROTECTED_AREAS.md`
- `.github/CODEOWNERS`
- `.github/workflows/protected-areas.yml`
- `.github/protected-contract.sha256`
- `cmake/VerifyProtectedContract.cmake`
- `CMakeLists.txt` (contains the mandatory build-time contract check)

## Build-time integrity check

Every build verifies SHA-256 fingerprints for the approved rules and card/table arrangement before compiling
the executable. If one of those files differs from the owner-approved baseline, the build stops with
`PROTECTED CONTRACT CHANGED`. Updating the baseline is itself a protected change and requires the same
explicit owner approval.

## Approval procedure

1. The owner states exactly what protected behavior may change.
2. The proposed pull request lists every protected file and the approved reason.
3. The owner applies the `protected-code-approved` label and approves the pull request.
4. Required tests and protection checks pass before merge.

The label is a CI acknowledgement, not permission by itself. Automated agents may not create or apply it.
Repository administrators retain the emergency ability to override GitHub rules, but ordinary contributions
must follow this procedure.
