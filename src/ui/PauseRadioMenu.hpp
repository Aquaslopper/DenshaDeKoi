#pragma once

#include "RadioEngine.hpp"
#include "ui/UiViewport.hpp"

#include <raylib.h>

namespace Ui {

// Rendering resources are owned by TableScene; this view only borrows them.
struct PauseRadioMenuAssets {
    Texture2D logo{};
    Font headingFont{};
    Font bodyFont{};
};

// Draws the complete PSX-inspired pause/radio overlay. Input hit boxes remain
// in TableHud so rendering and interaction share the same fixed design space.
void DrawPauseRadioMenu(const Viewport& viewport, const RadioStatus& radio,
                        const PauseRadioMenuAssets& assets);

} // namespace Ui
