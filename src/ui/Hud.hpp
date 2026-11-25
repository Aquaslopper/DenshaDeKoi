#pragma once

#include "TableGame.hpp"
#include "TableHud.hpp"
#include "ui/UiViewport.hpp"

#include <raylib.h>

namespace Ui {

// In-game HUD: score/status bar, decision buttons, round intro, and overlay entry points.
// Drawn in screen space after the world blit, in the same 1360x768 design space as hit rects.
void DrawHud(const Viewport& viewport, const TableGame& game, const TableHudState& hud,
             Texture2D logo, Font headingFont, Font bodyFont);

} // namespace Ui
