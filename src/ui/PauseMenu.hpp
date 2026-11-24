#pragma once

#include "GameOptions.hpp"
#include "ui/UiViewport.hpp"

namespace Ui {
struct PauseMenuAssets {
    Texture2D panel, stripe;
    Font headingFont, bodyFont;
};

// The supplied artwork is the coordinate system for both drawing and input.
// The outer viewport remains the game's established 1360x768 letterbox.
inline constexpr float PauseArtScale = 0.815f;
inline constexpr TableLayout::Rect PausePanel{
    (TableLayout::ImageWidth - 1205 * PauseArtScale) / 2, 78,
    1205 * PauseArtScale, 763 * PauseArtScale};

enum class OptionRow { Master, Effects, Hints, Resolution, Mode, AA, Resume, Title };
enum class PauseAction { None, Resume, Title };
struct PauseInput {
    TableLayout::Point2 canvas;
    bool moved, pressed, down;
    int vertical, horizontal; // Keyboard steps: -1, 0, +1.
    bool activate;
};

class PauseMenu {
public:
    // Returns Resume/Title when the user confirms those actions; None otherwise.
    // Option changes are applied by the host.
    PauseAction Update(GameOptions& options, const PauseInput& input, Resolution display, Resolution desktop);
    [[nodiscard]] OptionRow Focus() const { return focus_; }
private:
    OptionRow focus_ = OptionRow::Master;
    bool dragging_ = false;
};

void DrawPauseMenu(const Viewport& viewport, const GameOptions& options, Resolution display,
                   OptionRow focus, const PauseMenuAssets& assets);
} // namespace Ui
