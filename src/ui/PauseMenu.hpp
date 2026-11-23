#pragma once

#include "ui/UiViewport.hpp"

#include <raylib.h>

namespace Ui {
struct PauseMenuAssets {
    Font headingFont{};
    Font bodyFont{};
};

void DrawPauseMenu(const Viewport& viewport, float masterVolume, float sfxVolume,
                   const PauseMenuAssets& assets);
} // namespace Ui
