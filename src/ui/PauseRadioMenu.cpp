#include "ui/PauseRadioMenu.hpp"

#include "RadioPlaylist.hpp"
#include "TableHud.hpp"
#include "TableLayout.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace Ui {
namespace {
void DrawBox(const Viewport& map, TableLayout::Rect rect, Color fill, Color border,
             float borderPx = 1.5f) {
    DrawRectangle(map.X(rect.x), map.Y(rect.y), map.W(rect.width), map.H(rect.height), fill);
    DrawRectangleLinesEx(map.Rec(rect), std::max(1.0f, borderPx * map.scale), border);
}

void DrawRaisedBox(const Viewport& map, TableLayout::Rect r, Color fill, Color border,
                   Color highlight, Color shadow) {
    // PS1-era controls used a restrained bevel rather than a glossy gradient.
    DrawRectangle(map.X(r.x + 3.0f), map.Y(r.y + 4.0f), map.W(r.width), map.H(r.height),
                  Color{18, 20, 25, 105});
    DrawBox(map, r, fill, border, 2.0f);
    DrawRectangle(map.X(r.x + 2.0f), map.Y(r.y + 2.0f), map.W(r.width - 4.0f), map.H(2.0f), highlight);
    DrawRectangle(map.X(r.x + 2.0f), map.Y(r.y + 2.0f), map.W(2.0f), map.H(r.height - 4.0f), highlight);
    DrawRectangle(map.X(r.x + 2.0f), map.Y(r.y + r.height - 4.0f),
                  map.W(r.width - 4.0f), map.H(2.0f), shadow);
    DrawRectangle(map.X(r.x + r.width - 4.0f), map.Y(r.y + 2.0f),
                  map.W(2.0f), map.H(r.height - 4.0f), shadow);
}

std::string FormatTenths(float seconds) {
    const int tenths = std::max(0, static_cast<int>(std::lround(seconds * 10.0f)));
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "s";
}

void DrawPauseRadioMenuImpl(const Viewport& map, const RadioStatus& radio,
                        const PauseRadioMenuAssets& assets) {
    const Texture2D logo = assets.logo;
    const Font headingFont = assets.headingFont;
    const Font bodyFont = assets.bodyFont;
    constexpr Color navy{8, 31, 122, 255};
    constexpr Color blue{20, 29, 176, 255};
    constexpr Color ink{27, 31, 40, 255};
    constexpr Color paper{194, 195, 191, 255}; // warm PlayStation-era console grey
    constexpr Color button{48, 53, 63, 255};
    constexpr Color green{0, 111, 67, 255};
    constexpr Color orange{242, 103, 25, 255};

    const auto textWith = [](Font font, const char* text, int x, int y, int size, Color color) {
        DrawTextEx(font, text, {static_cast<float>(x), static_cast<float>(y)},
                   static_cast<float>(size), std::max(1.0f, size * 0.025f), color);
    };
    const auto MenuText = [&](const char* text, int x, int y, int size, Color color) {
        textWith(bodyFont, text, x, y, size, color);
    };
    const auto HeadingText = [&](const char* text, int x, int y, int size, Color color) {
        textWith(headingFont, text, x, y, size, color);
    };

    DrawRectangle(map.X(0), map.Y(0), map.W(TableLayout::ImageWidth), map.H(TableLayout::ImageHeight),
                  Color{8, 12, 18, 145});

    // Heavy shadow and double outline make the panel read like an arcade cabinet UI.
    const auto panel = TableHud::PausePanel;
    DrawRectangle(map.X(panel.x + 8.0f), map.Y(panel.y + 9.0f), map.W(panel.width), map.H(panel.height),
                  Color{4, 7, 13, 185});
    DrawBox(map, panel, paper, Color{245, 245, 239, 255}, 4.0f);
    DrawRectangleLinesEx(map.Rec({panel.x + 5.0f, panel.y + 5.0f, panel.width - 10.0f, panel.height - 10.0f}),
                         std::max(1.0f, 2.0f * map.scale), ink);
    // Fine horizontal variation keeps the large grey surface from looking flat.
    for (int y = 234; y < 670; y += 4) {
        DrawRectangle(map.X(261), map.Y(static_cast<float>(y)), map.W(838), map.H(1.0f),
                      Color{211, 212, 208, 120});
    }

    // Blue masthead, then the orange/white/green Densha stripes from the mock-up.
    DrawRectangle(map.X(261), map.Y(134), map.W(838), map.H(82), navy);
    DrawRectangle(map.X(261), map.Y(216), map.W(838), map.H(6), orange);
    DrawRectangle(map.X(261), map.Y(222), map.W(838), map.H(4), Color{248, 248, 241, 255});
    DrawRectangle(map.X(261), map.Y(226), map.W(838), map.H(6), green);
    HeadingText("RADIO", map.X(282), map.Y(148), map.Font(50), WHITE);
    HeadingText("MENU", map.X(465), map.Y(178), map.Font(24), WHITE);
    MenuText("PAUSED", map.X(580), map.Y(183), map.Font(18), Color{171, 178, 207, 255});
    if (logo.id != 0) {
        DrawTexturePro(logo, {0, 0, static_cast<float>(logo.width), static_cast<float>(logo.height)},
                       map.Rec({890.0f, 137.0f, 174.0f, 76.0f}), {0, 0}, 0, WHITE);
    }

    const bool radioOn = radio.enabled;
    MenuText("BGM", map.X(286), map.Y(257), map.Font(21), navy);
    DrawRaisedBox(map, TableHud::RadioPowerButton, radioOn ? green : blue,
                  radioOn ? Color{0, 58, 34, 255} : Color{7, 13, 76, 255},
                  radioOn ? Color{49, 157, 107, 255} : Color{75, 91, 220, 255},
                  radioOn ? Color{0, 62, 37, 255} : Color{5, 9, 79, 255});
    HeadingText(radioOn ? "ON" : "OFF", map.X(417), map.Y(255), map.Font(24), WHITE);

    DrawRaisedBox(map, TableHud::RadioMuteButton,
                  radio.muted ? Color{142, 48, 44, 255} : Color{126, 127, 130, 255}, ink,
                  radio.muted ? Color{202, 90, 82, 255} : Color{188, 189, 190, 255},
                  radio.muted ? Color{80, 25, 23, 255} : Color{68, 70, 75, 255});
    HeadingText(radio.muted ? "MUTED" : "MUTE", map.X(541), map.Y(255), map.Font(23), ink);

    // Segmented volume meter.
    const float vol = std::clamp(radio.volume, 0.0f, 1.0f);
    constexpr int segments = 18;
    for (int i = 0; i < segments; ++i) {
        const bool active = static_cast<float>(i + 1) / segments <= vol + 0.001f;
        DrawRectangle(map.X(TableHud::VolumeBar.x + i * 15.0f), map.Y(TableHud::VolumeBar.y),
                      map.W(9.0f), map.H(TableHud::VolumeBar.height),
                      active ? (radio.muted ? Color{117, 121, 124, 255} : green)
                             : Color{139, 143, 146, 255});
    }
    const int volPct = static_cast<int>(std::lround(vol * 100.0f));
    MenuText((std::to_string(volPct) + "%").c_str(), map.X(1007), map.Y(255), map.Font(21), navy);

    const std::string stationLabel = radio.stationName.empty() ? "J-Pop Powerplay" : radio.stationName;
    const std::string statusBadge = radio.statusText.empty() ? "Radio off" : radio.statusText;
    Color badgeColor = Color{73, 74, 78, 255};
    if (radio.state == RadioState::PLAYING) badgeColor = green;
    else if (radio.state == RadioState::CONNECTING || radio.state == RadioState::BUFFERING)
        badgeColor = Color{178, 115, 0, 255};
    else if (radio.state == RadioState::FAILED) badgeColor = Color{173, 43, 38, 255};

    MenuText("STATION", map.X(286), map.Y(309), map.Font(18), navy);
    MenuText(stationLabel.c_str(), map.X(386), map.Y(306), map.Font(22), ink);
    MenuText(("[" + statusBadge + "]").c_str(), map.X(386), map.Y(336), map.Font(16), badgeColor);
    if (radioOn) {
        MenuText(("BUFFER " + FormatTenths(radio.bufferedSec)).c_str(), map.X(887), map.Y(336),
                 map.Font(13), Color{92, 94, 98, 255});
    }

    const auto& stations = DefaultRadioStations();
    for (std::size_t i = 0; i < stations.size() && i < TableHud::StationSlotCount; ++i) {
        const auto box = TableHud::StationButton(i);
        const bool selected = radio.stationIndex == static_cast<int>(i);
        DrawRaisedBox(map, box, selected ? blue : button,
                      selected ? Color{7, 13, 76, 255} : Color{20, 23, 29, 255},
                      selected ? Color{73, 87, 224, 255} : Color{91, 96, 108, 255},
                      selected ? Color{5, 9, 78, 255} : Color{21, 24, 31, 255});
        if (selected) {
            DrawRectangle(map.X(box.x + 2.0f), map.Y(box.y + 19.0f), map.W(12.0f), map.H(26.0f),
                          Color{255, 204, 25, 255});
        }
        HeadingText(stations[i].name.c_str(), map.X(box.x + 39.0f), map.Y(box.y + 8.0f), map.Font(21), WHITE);
        MenuText((stations[i].subtitle + "  [pls]").c_str(), map.X(box.x + 39.0f), map.Y(box.y + 36.0f),
                 map.Font(16), Color{225, 226, 228, 255});
    }

    DrawRectangle(map.X(261), map.Y(597), map.W(838), map.H(2.0f), ink);
    MenuText("START NEW MATCH", map.X(296), map.Y(617), map.Font(12), ink);
    MenuText("NEW MATCH", map.X(296), map.Y(640), map.Font(16), ink);
    MenuText("OPEN MENU", map.X(520), map.Y(617), map.Font(12), ink);
    MenuText("ESC / P: PAUSE", map.X(520), map.Y(640), map.Font(15), ink);
    MenuText("TOGGLE VOLUME", map.X(722), map.Y(617), map.Font(12), ink);
    MenuText("M: MUTE", map.X(722), map.Y(640), map.Font(15), ink);
    MenuText("RETURN TO GAME", map.X(922), map.Y(617), map.Font(12), ink);
    MenuText("RESUME GAME", map.X(922), map.Y(640), map.Font(15), ink);
}

} // namespace

void DrawPauseRadioMenu(const Viewport& viewport, const RadioStatus& radio,
                        const PauseRadioMenuAssets& assets) {
    DrawPauseRadioMenuImpl(viewport, radio, assets);
}

} // namespace Ui
