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

enum class OptionRow { Master, Effects, Hints, Resolution, Mode, AA, Resume };
struct PauseInput {
    TableLayout::Point2 canvas;
    bool moved, pressed, down;
    int vertical, horizontal; // Keyboard steps: -1, 0, +1.
    bool activate;
};

// Smooth volume display with interpolation for arcade-style meter feel.
struct VolumeDisplay {
    float displayed = 0.0f;
    float target = 0.0f;
    void SetTarget(int value) { target = static_cast<float>(value); }
    void Update(float dt) {
        const float diff = target - displayed;
        if (std::fabs(diff) < 0.5f) { displayed = target; return; }
        displayed += diff * 15.0f * dt;
        displayed = std::clamp(displayed, 0.0f, 100.0f);
    }
    [[nodiscard]] int Get() const { return static_cast<int>(std::lround(displayed)); }
};

class PauseMenu {
public:
    PauseMenu();
    // Returns true only for Resume. Changes to options are applied by the host.
    bool Update(GameOptions& options, const PauseInput& input, Resolution display, Resolution desktop, float dt);
    [[nodiscard]] OptionRow Focus() const { return focus_; }
    // Accessors for drawing.
    [[nodiscard]] int MasterDisplay() const { return masterDisplay_.Get(); }
    [[nodiscard]] int EffectsDisplay() const { return effectsDisplay_.Get(); }
    [[nodiscard]] OptionRow HoverRow() const { return hoverRow_; }
    [[nodiscard]] bool IsDragging() const { return dragging_; }
private:
    VolumeDisplay masterDisplay_;
    VolumeDisplay effectsDisplay_;
    float focusAnim_ = 0.0f;
    OptionRow focusTarget_ = OptionRow::Master;
    OptionRow hoverRow_ = OptionRow::Master;
    OptionRow focus_ = OptionRow::Master;
    bool dragging_ = false;
};

void DrawPauseMenu(const Viewport& viewport, const GameOptions& options, Resolution display,
                   OptionRow focus, const PauseMenu& menu, const PauseMenuAssets& assets);
// Backward-compatible overload for TableScene (protected code).
void DrawPauseMenu(const Viewport& viewport, const GameOptions& options, Resolution display,
                   OptionRow focus, const PauseMenuAssets& assets);
} // namespace Ui
