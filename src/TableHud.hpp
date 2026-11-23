#pragma once

#include "RadioEngine.hpp"
#include "TableLayout.hpp"

#include <array>
#include <cstddef>

namespace TableHud {
using TableLayout::Rect;

inline constexpr Rect KoiKoiButton{1010.0f, 20.0f, 145.0f, 40.0f};
inline constexpr Rect ShobuButton{1165.0f, 20.0f, 145.0f, 40.0f};
inline constexpr Rect RoundEndButton{1000.0f, 20.0f, 310.0f, 66.0f};

// Right-side Pause / Menu button ("II RESUME / J-POP RADIO" chip in the mock).
// Clear of all card slots and the top-right decision banners.
inline constexpr Rect PauseButton{1204.0f, 344.0f, 130.0f, 66.0f};

// Centered retro Pause & Internet Radio overlay in 1360x768 HUD space.
// Layout follows the pixel-art mock: white outer frame, navy title bar,
// orange/green stripe, light-grey body, divider, then the footer hint bar.
inline constexpr Rect PausePanel{254.0f, 100.0f, 852.0f, 553.0f};
inline constexpr float PanelRight = PausePanel.x + PausePanel.width;   // 1106
inline constexpr float PanelBottom = PausePanel.y + PausePanel.height; // 653
// Title bar interior (between frame and stripe).
inline constexpr Rect TitleBar{PausePanel.x + 3.0f, PausePanel.y + 3.0f,
                               PausePanel.width - 6.0f, 80.0f};         // 103..183
inline constexpr Rect StripeOrange{PausePanel.x + 3.0f, 183.0f, PausePanel.width - 6.0f, 9.0f};
inline constexpr Rect StripeOrangeShade{PausePanel.x + 3.0f, 192.0f, PausePanel.width - 6.0f, 1.0f};
inline constexpr Rect StripeGreen{PausePanel.x + 3.0f, 193.0f, PausePanel.width - 6.0f, 8.0f};
inline constexpr Rect StripeGreenShade{PausePanel.x + 3.0f, 201.0f, PausePanel.width - 6.0f, 1.0f};
// Light-grey body between stripe and footer divider.
inline constexpr Rect BodyArea{PausePanel.x + 3.0f, 202.0f, PausePanel.width - 6.0f, 374.0f}; // 202..576
// BGM row: power ("OFF"/"ON") toggle, MUTE toggle, segmented meter, percentage.
inline constexpr Rect RadioPowerButton{386.0f, 217.0f, 125.0f, 38.0f};
inline constexpr Rect RadioMuteButton{517.0f, 217.0f, 141.0f, 38.0f};
inline constexpr Rect VolumeMeter{722.0f, 231.0f, 284.0f, 17.0f};
inline constexpr Rect VolumeHitBox{716.0f, 217.0f, 296.0f, 40.0f};
// Footer divider: light highlight above a black groove, then the footer bar.
inline constexpr Rect FooterDividerWhite{PausePanel.x + 3.0f, 576.0f, PausePanel.width - 6.0f, 2.0f};
inline constexpr Rect FooterDividerBlack{PausePanel.x + 3.0f, 578.0f, PausePanel.width - 6.0f, 4.0f};
// Footer text columns. The first starts a new match, the last resumes; the
// middle two are static hotkey hints.
inline constexpr float FooterTextY1 = 596.0f;
inline constexpr float FooterTextY2 = 619.0f;
inline constexpr float FooterColX[4] = {302.0f, 530.0f, 742.0f, 944.0f};
inline constexpr Rect FooterNewMatch{290.0f, 584.0f, 178.0f, 64.0f};
inline constexpr Rect FooterResume{918.0f, 584.0f, 172.0f, 64.0f};

inline constexpr std::size_t StationSlotCount = 6;
inline constexpr Rect StationButton(std::size_t index) {
    const std::size_t col = index % 2;
    const std::size_t row = index / 2;
    return {280.0f + static_cast<float>(col) * 408.0f,
            343.0f + static_cast<float>(row) * 78.0f,
            393.0f,
            66.0f};
}
} // namespace TableHud

struct TableHudState {
    bool paused = false;
    RadioStatus radio{};
};
