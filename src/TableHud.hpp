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
inline constexpr Rect PausePanel{310.0f, 80.0f, 740.0f, 608.0f};
inline constexpr Rect ResumeTopButton{878.0f, 102.0f, 146.0f, 40.0f};
inline constexpr Rect RadioPowerButton{336.0f, 162.0f, 136.0f, 44.0f};
inline constexpr Rect RadioMuteButton{480.0f, 162.0f, 86.0f, 44.0f};
inline constexpr Rect RadioBufferButton{574.0f, 162.0f, 176.0f, 44.0f};
inline constexpr Rect VolumeBar{760.0f, 174.0f, 204.0f, 20.0f};
inline constexpr Rect VolumeHitBox{754.0f, 164.0f, 216.0f, 40.0f};
inline constexpr Rect PrebufferBar{456.0f, 224.0f, 380.0f, 18.0f};
inline constexpr Rect PrebufferHitBox{450.0f, 214.0f, 392.0f, 38.0f};
inline constexpr Rect PasteUrlButton{336.0f, 544.0f, 210.0f, 42.0f};
inline constexpr Rect NewMatchButton{336.0f, 612.0f, 170.0f, 44.0f};
inline constexpr Rect ResumeBottomButton{854.0f, 612.0f, 170.0f, 44.0f};

inline constexpr std::size_t StationSlotCount = 6;
[[nodiscard]] constexpr Rect StationButton(std::size_t index) {
    const std::size_t col = index % 2;
    const std::size_t row = index / 2;
    return {336.0f + static_cast<float>(col) * 352.0f,
            306.0f + static_cast<float>(row) * 76.0f,
            336.0f,
            64.0f};
}
} // namespace TableHud

struct TableHudState {
    bool paused = false;
    RadioSnapshot radio{};
    std::string feedback;
};
