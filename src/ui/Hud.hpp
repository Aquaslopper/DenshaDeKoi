#pragma once

#include "TableGame.hpp" // Parse card enums before Raylib defines its BLUE color macro.
#include "TableHud.hpp"
#include "ui/UiViewport.hpp"

#include <raylib.h>

#include <vector>

namespace Ui {

void DrawBulletTimeDim(const TableGame& game, int canvasWidth, int canvasHeight);
void DrawCardHints(const TableGame& game, const std::vector<TableCard>& cards, TableLayout::Point2 pointer);
void DrawDealerSelection(const Viewport& viewport, const TableGame& game);

// Drawn in screen space after the world blit, in the same 1360x768 design space as hit rects.
void DrawHud(const Viewport& viewport, const TableGame& game, const TableHudState& hud,
             Texture2D logo, Font headingFont, Font bodyFont);

} // namespace Ui
