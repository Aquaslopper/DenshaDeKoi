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
inline constexpr Rect DealerCardLeft{420.0f, 205.0f, 176.0f, 286.0f};
inline constexpr Rect DealerCardRight{764.0f, 205.0f, 176.0f, 286.0f};

// Right-side Pause / Menu button, clear of all card slots and top-right decision banners.
inline constexpr Rect PauseButton{1218.0f, 352.0f, 130.0f, 64.0f};

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

struct TableHudState {
    bool paused = false;
    RadioStatus radio{};
};
