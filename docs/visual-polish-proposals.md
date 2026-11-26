# Visual-polish proposals

## Status and guardrails

The implemented change is limited to the expressly approved in-match HUD plates and card shadows. The PAUSE and RADIO menu implementations, artwork, text, layout, and input behavior were not edited. The paired PAUSE/RADIO after-mockups only replace the dimmed in-match HUD behind the unchanged menu screenshot.

The condensed task record did not retain an item-by-item list for unapproved **Lane B**. To avoid presenting invented requirements as approved scope, the two `lane-b-*` images below are explicitly provisional, design-only explorations based on the existing PAUSE/RADIO surfaces. They are not product captures, not implementation, and require owner review before any code or assets are changed. If the original Lane B list names different items, these studies should be replaced rather than treated as that specification.

## Lane B — unapproved studies

1. **Optional PAUSE frame trim** — [lane-b-pause-chrome-study.jpg](proposals/lane-b-pause-chrome-study.jpg) explores a thin railway-signage rail and a small system-menu tag around the existing pause panel. The current pause screen remains unchanged in the game.
2. **Optional RADIO status capsule** — [lane-b-radio-status-study.jpg](proposals/lane-b-radio-status-study.jpg) explores a compact stream-status plate in the existing metadata gutter. It is only a visual treatment; it does not propose changes to radio state, playback, station ordering, layout, or input.

## Approved-scope visual studies

These are composited previews, not runtime screenshots. The actual in-game before/after capture could not be made because desktop configuration is blocked by missing X11 development files. Full-size 1360×768 pairs are in [`docs/visual-polish/`](visual-polish/).

- [Table/HUD and card-shadow mock-up](proposals/table-after-mock.jpg)
- [Dealer prompt mock-up](proposals/dealer-after-mock.jpg)
- [Koi-Koi/Shobu decision mock-up](proposals/decision-after-mock.jpg)
- [Round-end action mock-up](proposals/round-end-after-mock.jpg)
- [Card contact-shadow detail crop](proposals/card-contact-shadow-detail-mock.jpg) (supplemental, 790×290)
- [PAUSE after-mock](proposals/pause-after-mock.jpg) and [RADIO after-mock](proposals/radio-after-mock.jpg): existing menu pixels are sourced from the repository screenshots; only the changed dimmed HUD regions are composited from the table mock-up.

All mockups leave gameplay, card art, table/camera geometry, HUD coordinates and hit rectangles, timing, and input behavior out of scope. They should not be used as evidence of real rendering quality; see the verification limitations in [`docs/visual-polish-report.md`](visual-polish-report.md).
