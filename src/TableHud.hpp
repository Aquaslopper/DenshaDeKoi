#pragma once

#include "RadioEngine.hpp"
#include "TableLayout.hpp"

#include <array>
#include <cstddef>

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

// Simple pause/options panel. This intentionally remains separate from the radio menu.
inline constexpr Rect OptionsPanel{390.0f, 170.0f, 580.0f, 430.0f};
inline constexpr Rect MasterVolumeBar{500.0f, 330.0f, 300.0f, 28.0f};
inline constexpr Rect MasterVolumeHitBox{490.0f, 315.0f, 390.0f, 58.0f};
inline constexpr Rect SfxVolumeBar{500.0f, 420.0f, 300.0f, 28.0f};
inline constexpr Rect SfxVolumeHitBox{490.0f, 405.0f, 390.0f, 58.0f};
inline constexpr Rect OptionsNewMatchButton{430.0f, 515.0f, 220.0f, 58.0f};
inline constexpr Rect OptionsResumeButton{710.0f, 515.0f, 220.0f, 58.0f};

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

enum class TableOverlay { NONE, RADIO, PAUSE };

struct TableHudState {
    TableOverlay overlay = TableOverlay::NONE;
    RadioStatus radio{};
    float masterVolume = 1.0f;
    float sfxVolume = 1.0f;
};
