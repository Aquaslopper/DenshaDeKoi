# Match backgrounds

These five images are offered in the **MATCH SETUP** popup that appears after
pressing **PLAY** in the title menu.

| Slot | File | Shown as |
|---|---|---|
| 1 | `match-background-1.jpg` | TOKYO LOOP |
| 2 | `match-background-2.jpg` | SAKURA LINE |
| 3 | `match-background-3.jpg` | SEASIDE LOCAL |
| 4 | `match-background-4.jpg` | MOONLIGHT YARD |
| 5 | `match-background-5.jpg` | MOUNTAIN PASS |

Every file currently contains a copy of `assets/the-city.jpg`, so the shipped
build looks unchanged. **Replace the JPGs in place** — no code change and no
CMake change are needed; the build copies this directory beside the executable
as `options/match-backgrounds/`.

Use a 16:9 image. The popup previews it cropped to fit its 672×378 box, and the
chosen image is stretched over the baked 1360×768 table canvas exactly like the
default station artwork.

Keep the current file names, or update `Ui::MatchBackgrounds` in
`src/ui/MatchSetupMenu.hpp` (labels and file names are listed there). A missing
file is not fatal: the popup then shows `PREVIEW MISSING` and the match falls
back to `the-city.jpg`.
