#pragma once

#include "RadioPlayer.hpp"
#include "TableLayout.hpp"

#include <array>
#include <cstddef>

namespace TableHud {
using TableLayout::Rect;

inline constexpr Rect KoiKoiButton{1010.0f, 20.0f, 145.0f, 40.0f};
inline constexpr Rect ShobuButton{1165.0f, 20.0f, 145.0f, 40.0f};
inline constexpr Rect RoundEndButton{1000.0f, 20.0f, 310.0f, 66.0f};

// Right-side Pause / Menu button, clear of all card slots and top-right decision banners.
inline constexpr Rect PauseButton{1218.0f, 352.0f, 130.0f, 64.0f};

// Centered Pause & Internet Radio overlay in 1360x768 HUD space.
inline constexpr Rect PausePanel{310.0f, 96.0f, 740.0f, 576.0f};
inline constexpr Rect ResumeTopButton{878.0f, 118.0f, 146.0f, 40.0f};
inline constexpr Rect RadioPowerButton{336.0f, 184.0f, 150.0f, 44.0f};
inline constexpr Rect RadioMuteButton{498.0f, 184.0f, 96.0f, 44.0f};
inline constexpr Rect VolumeBar{608.0f, 196.0f, 356.0f, 20.0f};
inline constexpr Rect VolumeHitBox{602.0f, 186.0f, 368.0f, 40.0f};
inline constexpr Rect PasteUrlButton{336.0f, 524.0f, 210.0f, 42.0f};
inline constexpr Rect NewMatchButton{336.0f, 594.0f, 170.0f, 44.0f};
inline constexpr Rect ResumeBottomButton{854.0f, 594.0f, 170.0f, 44.0f};

inline constexpr std::size_t StationSlotCount = 6;
[[nodiscard]] constexpr Rect StationButton(std::size_t index) {
    const std::size_t col = index % 2;
    const std::size_t row = index / 2;
    return {336.0f + static_cast<float>(col) * 352.0f,
            286.0f + static_cast<float>(row) * 76.0f,
            336.0f,
            64.0f};
}
} // namespace TableHud

struct TableHudState {
    bool paused = false;
    RadioSnapshot radio{};
    std::string feedback;
};
