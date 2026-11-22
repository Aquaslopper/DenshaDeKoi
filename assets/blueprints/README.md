# Table layout blueprints

These three images are the reference for the table layout. They are the source of
truth for where cards go, how big they are, and how the flat plan is projected
onto the painted table. Nothing in the layout code should contradict them.

| File | Size | What it defines |
| --- | --- | --- |
| `2D example.png` | 839×680 | The flat plan. Every slot rectangle is read from here. |
| `grid example-2.png` | 1280×720 | The same layout projected onto the table art. This is the acceptance view. |
| `grid layout-2.png` | 1168×447 | The projected layout on black, for measuring geometry without the background. |

## What the plan (`2D example.png`) defines

Coordinates are pixels in the 839×680 image:

- **Opponent hand** — eight slots, `x = 185 + 62·i`, `y = 13`, each `55×90`.
- **Player hand** — eight slots on the same columns, `y = 577`, each `55×90`.
- **Stock** — one slot at `(49, 262)`, `77×125`.
- **Field** — the dashed grid in the middle: two rows of eight, on the same
  columns as the hands, between the two hand rows.

The hands are symmetric on purpose: neither one is larger, larger-typed or
pushed to the front. The board reads as one grid, not as "your hand plus a
background".

## How the plan is used

1. `src/TableLayout.hpp` holds the slot rectangles as `constexpr` data, copied
   from the plan above — same x, y, width and height values. Do not invent new
   numbers, do not "correct" the aspect of a slot to match the card art.
2. The plan is a design surface, not a screen: it is mapped onto the table
   through `TableLayout::OnPlane()` and `TableLayout::Project()`.
3. `Project()` is the forward mapping used for drawing and tests;
   `Unproject()` is its inverse and is what pointer hit-testing uses, so a click
   on a card lands on the plan rectangle the art was drawn from.

## How the projected images are used

- **Camera calibration.** `CameraHeight`, `PitchRadians`, `FovDegrees`,
  `PlaneHalfWidth` and the near/far depths are fitted so that the four corners
  of the plan land on the table's quad in `grid example-2.png`. If the corners
  move, the camera constants are wrong — the slot rectangles do not change.
- **Verification.** Rendering the scene and overlaying it on `grid example-2.png`
  should line the card slots up with the cyan placeholders within a couple of
  pixels. Tests in `tests/test_table.cpp` check the projected plan corners and
  the slot geometry.
- **Measurement.** `grid layout-2.png` is the same projection cropped to the
  table on black, which makes slot edges easy to measure when checking the fit.

## Rules for changing the layout

- The blueprint wins. If code and blueprint disagree, the code is wrong.
- The card art keeps its own aspect; the plan rectangle is whatever the
  blueprint draws. Never resize a slot to force the art to fit.
- Both hands stay the same size as each other and as the field slots.
- Card backs (opponent hand, stock, face-down) stay solid black. The blueprint
  placeholders are space reservations, not art.
