# DenshaDeKoi sound effects

Place your own **WAV** files in this directory. All files are optional; a missing
file simply stays silent. Names are lowercase and must match exactly:

| File | Sound slot |
|---|---|
| `card_deal.wav` | a card leaving the stock |
| `card_flip.wav` | revealing a card |
| `card_place.wav` | a card landing on the table |
| `card_match.wav` | a successful month match |
| `card_capture.wav` | cards moving to a capture pile |
| `koi_koi.wav` | choosing Koi-Koi |
| `shobu.wav` | choosing Shobu |
| `round_win.wav` | player wins a round |
| `round_lose.wav` | CPU wins a round |

Use WAV files with the exact names above. The build copies this directory beside the game executable as `sfx/`.

The sounds are triggered by their matching gameplay events. SFX volume remains separate from the radio volume.
