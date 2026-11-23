#include "ui/PauseMenu.hpp"

#include "TableHud.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace Ui {
namespace {
void Box(const Viewport& map, TableLayout::Rect r, Color fill, Color border) {
    DrawRectangle(map.X(r.x), map.Y(r.y), map.W(r.width), map.H(r.height), fill);
    DrawRectangleLinesEx(map.Rec(r), std::max(1.0f, 2.0f * map.scale), border);
}

void Label(const Viewport& map, Font font, const char* text, float x, float y, float size, Color color) {
    DrawTextEx(font, text, {static_cast<float>(map.X(x)), static_cast<float>(map.Y(y))},
               static_cast<float>(map.Font(static_cast<int>(size))), 1.0f, color);
}

void Slider(const Viewport& map, Font font, const char* name, TableLayout::Rect bar, float value) {
    constexpr Color ink{27, 31, 40, 255};
    constexpr Color blue{20, 29, 176, 255};
    Label(map, font, name, bar.x, bar.y - 34.0f, 21.0f, ink);
    Box(map, bar, Color{128, 132, 138, 255}, ink);
    DrawRectangle(map.X(bar.x + 3.0f), map.Y(bar.y + 3.0f),
                  map.W((bar.width - 6.0f) * std::clamp(value, 0.0f, 1.0f)), map.H(bar.height - 6.0f), blue);
    const std::string percent = std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%";
    Label(map, font, percent.c_str(), bar.x + bar.width + 18.0f, bar.y - 2.0f, 18.0f, ink);
}
} // namespace

void DrawPauseMenu(const Viewport& map, float masterVolume, float sfxVolume,
                   const PauseMenuAssets& assets) {
    constexpr Color paper{194, 195, 191, 255};
    constexpr Color ink{27, 31, 40, 255};
    constexpr Color navy{8, 31, 122, 255};
    DrawRectangle(map.X(0), map.Y(0), map.W(TableLayout::ImageWidth), map.H(TableLayout::ImageHeight),
                  Color{8, 12, 18, 145});
    Box(map, TableHud::OptionsPanel, paper, WHITE);
    DrawRectangle(map.X(TableHud::OptionsPanel.x), map.Y(TableHud::OptionsPanel.y),
                  map.W(TableHud::OptionsPanel.width), map.H(88.0f), navy);
    Label(map, assets.headingFont, "PAUSE", TableHud::OptionsPanel.x + 35.0f,
          TableHud::OptionsPanel.y + 20.0f, 42.0f, WHITE);
    Slider(map, assets.bodyFont, "MASTER VOLUME", TableHud::MasterVolumeBar, masterVolume);
    Slider(map, assets.bodyFont, "SFX VOLUME", TableHud::SfxVolumeBar, sfxVolume);
    Box(map, TableHud::OptionsNewMatchButton, Color{48, 53, 63, 255}, ink);
    Box(map, TableHud::OptionsResumeButton, navy, ink);
    Label(map, assets.headingFont, "NEW MATCH", TableHud::OptionsNewMatchButton.x + 35.0f,
          TableHud::OptionsNewMatchButton.y + 17.0f, 20.0f, WHITE);
    Label(map, assets.headingFont, "RESUME", TableHud::OptionsResumeButton.x + 58.0f,
          TableHud::OptionsResumeButton.y + 17.0f, 20.0f, WHITE);
}
} // namespace Ui
