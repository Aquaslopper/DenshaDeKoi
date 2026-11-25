#pragma once

#include "GameOptions.hpp"
#include "RadioEngine.hpp"
#include "TableGame.hpp"
#include "TableHud.hpp"
#include "TableLayout.hpp"
#include "ui/UiViewport.hpp"

#include <raylib.h>

#include <filesystem>
#include <string>

namespace Ui {

struct TableHudRendererAssets {
    Texture2D logo;
    Font headingFont;
    Font bodyFont;
};

class TableHudRenderer {
public:
    explicit TableHudRenderer(const TableHudRendererAssets& assets)
        : logo_(assets.logo), headingFont_(assets.headingFont), bodyFont_(assets.bodyFont) {}

    void Draw(const TableGame& game, const TableHudState& hud, const Viewport& viewport, float scale) const;

private:
    Texture2D logo_;
    Font headingFont_;
    Font bodyFont_;
};

} // namespace Ui