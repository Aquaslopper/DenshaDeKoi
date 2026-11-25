#pragma once

#include "GameOptions.hpp"
#include "RadioEngine.hpp"
#include "TableGame.hpp"
#include "TableLayout.hpp"
#include "ui/UiViewport.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <vector>

namespace TableHud {
using TableLayout::Rect;

inline constexpr Rect KoiKoiButton{505.0f, 336.0f, 165.0f, 48.0f};
inline constexpr Rect ShobuButton{690.0f, 336.0f, 165.0f, 48.0f};
inline constexpr Rect RoundEndButton{520.0f, 326.0f, 320.0f, 72.0f};
// Screen-space hit areas around the projected 3D dealer cards on the table.
inline constexpr Rect DealerCardLeft{490.0f, 315.0f, 155.0f, 190.0f};
inline constexpr Rect DealerCardRight{715.0f, 315.0f, 175.0f, 190.0f};

// Separate right-side entry points. Icon artwork can replace the temporary labels later.
inline constexpr Rect RadioMenuButton{1266.0f, 322.0f, 76.0f, 58.0f};
inline constexpr Rect PauseButton{1266.0f, 390.0f, 76.0f, 58.0f};

// Centered Pause & Internet Radio overlay in 1360x768 HUD space.
// Dimensions follow the supplied blue/white arcade-style menu mock-up.
inline constexpr Rect PausePanel{255.0f, 128.0f, 850.0f, 548.0f};
inline constexpr Rect ResumeTopButton{0.0f, 0.0f, 0.0f, 0.0f}; // keyboard-only; retained for input compatibility
inline constexpr Rect RadioPowerButton{382.0f, 245.0f, 116.0f, 42.0f};
inline constexpr Rect RadioMuteButton{506.0f, 245.0f, 136.0f, 42.0f};
inline constexpr Rect VolumeBar{706.0f, 251.0f, 274.0f, 30.0f};
inline constexpr Rect VolumeHitBox{698.0f, 240.0f, 290.0f, 48.0f};
inline constexpr Rect NewMatchButton{278.0f, 607.0f, 190.0f, 54.0f};
inline constexpr Rect ResumeBottomButton{886.0f, 607.0f, 190.0f, 54.0f};

inline constexpr std::size_t StationSlotCount = 6;
[[nodiscard]] constexpr Rect StationButton(std::size_t index) {
    const std::size_t col = index % 2;
    const std::size_t row = index / 2;
    return {278.0f + static_cast<float>(col) * 400.0f,
            371.0f + static_cast<float>(row) * 73.0f,
            382.0f,
            64.0f};
}
} // namespace TableHud

namespace Ui { enum class OptionRow; }

enum class TableOverlay { NONE, RADIO, PAUSE };

struct TableHudState {
    TableOverlay overlay = TableOverlay::NONE;
    RadioStatus radio{};
    GameOptions options{};
    Ui::OptionRow focusedOption{}; // First row on opening the pause menu.
    TableLayout::Point2 pointer{-1, -1};
};

namespace Ui {

// In-canvas HUD overlays drawn around the main HUD pass.
void DrawBulletTimeDim(const TableGame& game, int canvasWidth, int canvasHeight);
void DrawCardHints(const TableGame& game, const std::vector<TableCard>& cards, TableLayout::Point2 pointer);
void DrawDealerSelection(const Viewport& viewport, const TableGame& game);

// In-game HUD: score/status bar, decision buttons, round intro, and overlay entry points.
// Drawn in screen space after the world blit, in the same 1360x768 design space as hit rects.
void DrawHud(const Viewport& viewport, const TableGame& game, const TableHudState& hud,
             Texture2D logo, Font headingFont, Font bodyFont);

// Screen-space modal overlays (radio/pause). Drawn after the magnify shader so UI stays crisp.
void DrawScreenOverlay(const Viewport& viewport, const TableHudState& hud, Resolution display,
                       Texture2D logo, Texture2D optionsPanel, Texture2D optionsStripe,
                       Font headingFont, Font bodyFont);

} // namespace Ui
