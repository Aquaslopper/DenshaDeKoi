#pragma once

#include "GameOptions.hpp"
#include "KoiKoiRules.hpp"
#include "RadioEngine.hpp"
#include "TableLayout.hpp"
#include "ui/UiViewport.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

class TableGame;
struct TableCard;

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
inline constexpr Rect HomeButton{1266.0f, 458.0f, 76.0f, 58.0f};

// Top match strip: one colour-coded driver badge and a 2x2 yaku grid per player.
// The artwork follows the supplied 1342px strip scaled into the 1360px HUD space.
inline constexpr Rect YouYakuBadge{4.0f, 8.0f, 66.0f, 92.0f};
inline constexpr Rect YouYakuPanel{70.0f, 8.0f, 336.0f, 92.0f};
inline constexpr Rect CpuYakuPanel{954.0f, 8.0f, 336.0f, 92.0f};
inline constexpr Rect CpuYakuBadge{1290.0f, 8.0f, 66.0f, 92.0f};
inline constexpr std::size_t YakuSlotCount = 4;

[[nodiscard]] constexpr Rect YakuSlotRect(bool cpu, std::size_t index) {
    const Rect panel = cpu ? CpuYakuPanel : YouYakuPanel;
    constexpr float pad = 4.0f;
    constexpr float gap = 4.0f;
    const float width = (panel.width - 2.0f * pad - gap) * 0.5f;
    const float height = (panel.height - 2.0f * pad - gap) * 0.5f;
    return {panel.x + pad + (index % 2) * (width + gap),
            panel.y + pad + (index / 2) * (height + gap), width, height};
}

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

namespace Ui {
enum class OptionRow;
class MatchResultMenu;
}

enum class TableOverlay { NONE, RADIO, PAUSE };

struct TableHudState {
    TableOverlay overlay = TableOverlay::NONE;
    RadioStatus radio{};
    GameOptions options{};
    Ui::OptionRow focusedOption{}; // First row on opening the pause menu.
    TableLayout::Point2 pointer{-1, -1};
    const Ui::MatchResultMenu* matchResult = nullptr;
};

namespace Ui {

// One miniature yaku card in a player's 2x2 grid. `month` supplies the tiny
// hanafuda-style chip; `overflow` marks how many further yaku did not fit.
struct HudYakuSlot {
    std::string name;
    int points = 0;
    int month = 0;
    int overflow = 0;
};

// Same priority as the round-result combos, capped at the four visible corners.
[[nodiscard]] std::array<HudYakuSlot, TableHud::YakuSlotCount> BuildHudYakuSlots(
    std::span<const CardId> captured);

// In-canvas HUD overlays drawn around the main HUD pass.
void DrawBulletTimeDim(const TableGame& game, int canvasWidth, int canvasHeight);
void DrawCardHints(const TableGame& game, const std::vector<TableCard>& cards, TableLayout::Point2 pointer);
void DrawDealerSelection(const Viewport& viewport, const TableGame& game);

// In-game HUD: score/status bar, decision buttons, round intro, and overlay entry points.
// Drawn in screen space after the world blit, in the same 1360x768 design space as hit rects.
void DrawHud(const Viewport& viewport, const TableGame& game, const TableHudState& hud,
             Texture2D logo, Font bodyFont);

// Screen-space modal overlays (radio/pause). Drawn after the magnify shader so UI stays crisp.
void DrawScreenOverlay(const Viewport& viewport, const TableHudState& hud, Resolution display,
                       Texture2D logo, Texture2D optionsPanel, Texture2D optionsStripe,
                       Font headingFont, Font bodyFont);

} // namespace Ui
