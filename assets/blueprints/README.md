# Table blueprint measurements

Numbers measured from the blueprint images in this folder, so card placement can
be checked against the drawings instead of being eyeballed.

## Coordinate space

The 2D drawings (`2D example.png`, `2d colorized example.png`) are **839 x 680 px**.
The perspective drawings (`grid layout-2.png` 1168 x 447, `grid example-2.png`
1280 x 720) are renders of the same plan viewed from the table camera.

The code uses the same plan directly: `TableLayout::PlanWidth` = 840 and
`PlanHeight` = 680, so the coordinates below are the coordinates in
`src/TableLayout.hpp`.

## Card slots (2D plan, top-left origin, pixels)

| Element        | Count | x (first) | y   | width | height | x step |
|----------------|-------|-----------|-----|-------|--------|--------|
| Opponent hand  | 8     | 185       | 13  | 55    | 90     | 62     |
| Player hand    | 8     | 185       | 577 | 55    | 90     | 62     |
| Stock          | 1     | 50        | 263 | 75    | 123    | —      |
| Field row 0    | 8     | 165       | 184 | 76    | 124    | 81     |
| Field row 1    | 8     | 165       | 324 | 76    | 124    | 81     |

The hand and stock rectangles are the cyan fills, so those bounds are exact to
the pixel. The field slots are dashed outlines and only columns 2–5 are drawn, so
their bounds are within about 1–2 px. Rows are 140 px apart (124 px slot + 16 px
gutter); columns are 81 px apart (76 px slot + 5 px gutter).

Field slot index order in the blueprint is row-major, left to right:
`0..7` in row 0 (y = 184) and `8..15` in row 1 (y = 324).

## Code mapping

`src/TableLayout.hpp` now uses these values verbatim:

| Element       | `TableLayout` value                   |
|---------------|---------------------------------------|
| Opponent hand | 185, 13, 55 x 90, step 62             |
| Player hand   | 185, 577, 55 x 90, step 62            |
| Stock         | 50, 263, 75 x 123                     |
| Field         | 165, 184/324, 76 x 124, step 81 / 140 |

The initial deal puts the eight field cards in slots 2-5 of both rows, i.e. the
solid blue block in `2d colorized example.png`.

`cards-on-blueprint.png` in this folder shows a real deal composited onto the
blueprint at these coordinates.
