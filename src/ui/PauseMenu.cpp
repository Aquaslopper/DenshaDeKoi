#include "ui/PauseMenu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace Ui {
namespace {
using TableLayout::Point2;
using TableLayout::Rect;
constexpr std::array Names{"Master Volume", "Sound Effects", "Card Hints", "Display Resolution", "Screen Mode", "Anti-Aliasing"};
constexpr std::array Modes{"Windowed", "Borderless", "Fullscreen"};
constexpr std::array Qualities{"2x", "3x", "4x"};
constexpr Color Ink{9, 17, 32, 255}, Silver{196, 207, 227, 255}, Yellow{255, 213, 25, 255};
constexpr Color DarkKey{27, 36, 53, 255}, Green{0, 145, 65, 255}, Amber{255, 171, 27, 255};
constexpr Rect ResumeBtn{397, 658, 196, 90};
constexpr Rect TitleBtn{611, 658, 196, 90};

constexpr Rect Stripe(OptionRow row) { return {36, 208 + 70.5f * static_cast<int>(row), 810, 63}; }
constexpr Rect Meter(OptionRow row) { return {484, Stripe(row).y + 19, 264, 26}; }
constexpr Rect Choice(OptionRow row, int i) {
    const bool two = row == OptionRow::Hints;
    return {412 + i * (two ? 216.0f : 141.0f), Stripe(row).y + 6, two ? 186.0f : 128.0f, 51};
}
constexpr Rect Arrow(int direction) {
    return {direction < 0 ? 402.0f : 786.0f, Stripe(OptionRow::Resolution).y + 2, 54, 59};
}
bool Contains(Rect r, Point2 p) {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}

void Step(GameOptions& o, OptionRow row, int step, Resolution display, Resolution desktop) {
    switch (row) {
        case OptionRow::Master: o.masterVolume = std::clamp(o.masterVolume + 5 * step, 0, 100); break;
        case OptionRow::Effects: o.sfxVolume = std::clamp(o.sfxVolume + 5 * step, 0, 100); break;
        case OptionRow::Hints: o.cardHints = !o.cardHints; break;
        case OptionRow::Resolution:
            if (o.screenMode != ScreenMode::Borderless) {
                // An exclusive mode can be rounded by the OS. Advance from the
                // request so an unsupported intermediate preset cannot trap the arrows.
                const auto from = o.screenMode == ScreenMode::Fullscreen ? o.resolution : display;
                o.resolution = StepResolution(from, desktop, step);
            }
            break;
        case OptionRow::Mode: o.screenMode = static_cast<ScreenMode>((int(o.screenMode) + step + 3) % 3); break;
        case OptionRow::AA: o.aaSamples = 2 + (o.aaSamples - 2 + step + 3) % 3; break;
        case OptionRow::Resume:
        case OptionRow::Title: break;
    }
}

void Blit(const Viewport& map, Texture2D texture, Rectangle source, Rect dest) {
    DrawTexturePro(texture, source, map.Rec(dest), {0, 0}, 0, WHITE);
}

void Label(const Viewport& map, Font font, const char* text, Rect box, float size, Color ink, bool left = false) {
    const auto r = map.Rec(box);
    size *= map.scale;
    const float spacing = 0.5f * map.scale;
    float width = MeasureTextEx(font, text, size, spacing).x;
    if (width > r.width) {
        size *= r.width / width;
        width = MeasureTextEx(font, text, size, spacing).x;
    }
    DrawTextEx(font, text, {r.x + (left ? 0 : (r.width - width) / 2), r.y + (r.height - size) / 2},
               size, spacing, ink);
}

Vector2 Point(const Viewport& map, float x, float y) {
    return {map.box.x + x * map.scale, map.box.y + y * map.scale};
}

void Key(const Viewport& map, Font font, const char* text, Rect r, bool selected, float typeSize, bool toggle = false) {
    const Color fill = !selected ? DarkKey : toggle ? Green : Yellow;
    DrawRectangleRounded(map.Rec(r), 0.25f, 2, selected && toggle ? Color{63, 239, 136, 255} : Silver);
    DrawRectangleRounded(map.Rec({r.x + 2, r.y + 2, r.width - 4, r.height - 4}), 0.2f, 2, fill);
    Label(map, font, text, {r.x + 7, r.y, r.width - 14, r.height}, typeSize,
          selected && !toggle ? Ink : WHITE);
}

void ActionButton(const Viewport& map, Font font, const char* text, Rect r, bool selected, Color accent) {
    const auto rec = map.Rec(r);
    DrawRectangleRounded(rec, 0.12f, 6, Color{10, 18, 35, 245});
    DrawRectangleRoundedLinesEx(rec, 0.12f, 6, std::max(1.0f, 2.5f * map.scale),
                                selected ? accent : Color{120, 136, 160, 220});
    if (selected) {
        DrawRectangleRounded(map.Rec({r.x + 4, r.y + 4, r.width - 8, r.height - 8}), 0.1f, 6,
                             Color{accent.r, accent.g, accent.b, 28});
    }
    Label(map, font, text, r, 38, selected ? accent : WHITE);
}

void Volume(const Viewport& map, Font font, OptionRow row, int percent) {
    const auto r = Meter(row);
    const float cy = r.y + r.height / 2;
    const Color icon = percent > 0 ? Silver : Color{123, 132, 149, 255};
    DrawRectangle(map.X(414), map.Y(cy - 7), map.W(11), map.H(14), icon);
    // Speaker cone, back to front, then two open sound waves.
    DrawTriangle(Point(map, 419, cy), Point(map, 434, cy + 20), Point(map, 434, cy - 20), icon);
    if (percent > 0) {
        for (float radius : {17.0f, 25.0f})
            DrawRing(Point(map, 429, cy), (radius - 2.5f) * map.scale, radius * map.scale, -49, 49, 8, icon);
    }
    constexpr int segments = 12;
    const int lit = static_cast<int>(std::lround(percent * segments / 100.0f));
    for (int i = 0; i < segments; ++i) {
        DrawRectangleGradientV(map.X(r.x + 22.3f * i), map.Y(r.y), map.W(18.5f), map.H(r.height),
                               i < lit ? Color{0, 194, 73, 255} : Color{98, 106, 122, 255},
                               i < lit ? Color{0, 149, 57, 255} : Color{69, 76, 90, 255});
    }
    Label(map, font, std::to_string(percent).c_str(), {762, Stripe(row).y, 65, 63}, 38, WHITE);
}

// Use an actual multiplication sign without depending on non-ASCII font glyphs.
void DisplaySize(const Viewport& map, Font font, Resolution display, Rect row) {
    const auto w = std::to_string(display.width), h = std::to_string(display.height);
    const float size = 38 * map.scale, spacing = 0.5f * map.scale;
    const float w1 = MeasureTextEx(font, w.c_str(), size, spacing).x;
    const float w2 = MeasureTextEx(font, h.c_str(), size, spacing).x;
    const auto center = Point(map, 621, row.y + row.height / 2);
    const float gap = 32 * map.scale, left = center.x - (w1 + gap + w2) / 2;
    DrawTextEx(font, w.c_str(), {left, center.y - size / 2}, size, spacing, WHITE);
    DrawTextEx(font, h.c_str(), {left + w1 + gap, center.y - size / 2}, size, spacing, WHITE);
    const float cx = left + w1 + gap / 2, d = 6.5f * map.scale;
    DrawLineEx({cx - d, center.y - d}, {cx + d, center.y + d}, 3.2f * map.scale, WHITE);
    DrawLineEx({cx - d, center.y + d}, {cx + d, center.y - d}, 3.2f * map.scale, WHITE);
}

void ResolutionArrow(const Viewport& map, int direction, bool enabled) {
    const auto r = Arrow(direction);
    const float x = r.x + r.width / 2, y = r.y + r.height / 2, dx = 11.5f * direction;
    DrawTriangle(Point(map, x + dx, y), Point(map, x - dx, y - 22 * direction),
                 Point(map, x - dx, y + 22 * direction), enabled ? Amber : DarkKey);
}
} // namespace

PauseAction PauseMenu::Update(GameOptions& o, const PauseInput& input, Resolution display, Resolution desktop) {
    const Point2 p{(input.canvas.x - PausePanel.x) / PauseArtScale, (input.canvas.y - PausePanel.y) / PauseArtScale};
    if (!input.down) dragging_ = false;
    if (!dragging_ && (input.moved || input.pressed)) {
        for (int i = 0; i < static_cast<int>(Names.size()); ++i)
            if (Contains(Stripe(static_cast<OptionRow>(i)), p)) focus_ = static_cast<OptionRow>(i);
        if (Contains(ResumeBtn, p)) focus_ = OptionRow::Resume;
        if (Contains(TitleBtn, p)) focus_ = OptionRow::Title;
    }
    if (input.pressed) {
        if (Contains(ResumeBtn, p)) return PauseAction::Resume;
        if (Contains(TitleBtn, p)) return PauseAction::Title;
        if (focus_ == OptionRow::Master || focus_ == OptionRow::Effects) {
            const auto bar = Meter(focus_);
            dragging_ = Contains({bar.x - 7, Stripe(focus_).y, bar.width + 14, 63}, p);
        } else if (focus_ == OptionRow::Resolution) {
            for (int step : {-1, 1}) if (Contains(Arrow(step), p)) Step(o, focus_, step, display, desktop);
        } else if (focus_ == OptionRow::Hints || focus_ == OptionRow::Mode || focus_ == OptionRow::AA) {
            for (int i = 0; i < (focus_ == OptionRow::Hints ? 2 : 3); ++i) {
                if (!Contains(Choice(focus_, i), p)) continue;
                if (focus_ == OptionRow::Hints) o.cardHints = i == 0;
                else if (focus_ == OptionRow::Mode) o.screenMode = static_cast<ScreenMode>(i);
                else o.aaSamples = 2 + i;
            }
        }
    }
    if (dragging_) {
        const auto r = Meter(focus_);
        const int value = static_cast<int>(std::lround(std::clamp((p.x - r.x) / r.width, 0.0f, 1.0f) * 100));
        (focus_ == OptionRow::Master ? o.masterVolume : o.sfxVolume) = value;
        return PauseAction::None;
    }
    constexpr int rows = static_cast<int>(OptionRow::Title) + 1;
    if (input.vertical) focus_ = static_cast<OptionRow>((int(focus_) + rows + input.vertical) % rows);
    if (input.horizontal) {
        if (focus_ == OptionRow::Resume) focus_ = OptionRow::Title;
        else if (focus_ == OptionRow::Title) focus_ = OptionRow::Resume;
        else Step(o, focus_, input.horizontal, display, desktop);
    }
    if (input.activate) {
        if (focus_ == OptionRow::Hints) o.cardHints = !o.cardHints;
        else if (focus_ == OptionRow::Resume) return PauseAction::Resume;
        else if (focus_ == OptionRow::Title) return PauseAction::Title;
    }
    return PauseAction::None;
}

void DrawPauseMenu(const Viewport& outer, const GameOptions& o, Resolution display,
                   OptionRow focus, const PauseMenuAssets& assets) {
    DrawRectangle(outer.X(0), outer.Y(0), outer.W(TableLayout::ImageWidth), outer.H(TableLayout::ImageHeight),
                  Color{5, 8, 17, 142});
    const Viewport map{outer.Rec(PausePanel), outer.scale * PauseArtScale};
    Blit(map, assets.panel, {0, 0, 1205, 763}, {0, 0, 1205, 763});
    for (int i = 0; i < static_cast<int>(Names.size()); ++i) {
        const auto row = static_cast<OptionRow>(i);
        const auto r = Stripe(row);
        if (focus == row) Blit(map, assets.stripe, {0, 0, 810, 63}, r);
        else {
            // Reuse the actual navy texture and blue number tile, without the yellow focus band.
            Blit(map, assets.stripe, {343, 0, 467, 63}, r);
            Blit(map, assets.stripe, {4, 3, 67, 54}, {r.x + 2, r.y + 2, 69, 59});
            DrawRectangleLinesEx(map.Rec(r), std::max(1.0f, 1.5f * map.scale), Silver);
        }
        Label(map, assets.headingFont, std::to_string(i + 1).c_str(), {r.x + 3, r.y + 3, 64, 62}, 52, WHITE);
        const char* name = row == OptionRow::AA && o.aaSamples == 1 ? "Anti-Aliasing: Off" : Names[i];
        Label(map, assets.bodyFont, name, {r.x + 88, r.y, 247, r.height}, 47, focus == row ? Ink : WHITE, true);
        if (row == OptionRow::Master || row == OptionRow::Effects)
            Volume(map, assets.headingFont, row, row == OptionRow::Master ? o.masterVolume : o.sfxVolume);
        else if (row == OptionRow::Hints) {
            Key(map, assets.bodyFont, "ON", Choice(row, 0), o.cardHints, 47, true);
            Key(map, assets.bodyFont, "OFF", Choice(row, 1), !o.cardHints, 47, true);
        } else if (row == OptionRow::Resolution) {
            const bool adjustable = o.screenMode != ScreenMode::Borderless;
            for (int dir : {-1, 1}) ResolutionArrow(map, dir, adjustable);
            DisplaySize(map, assets.headingFont, display, r);
        } else {
            const bool mode = row == OptionRow::Mode;
            for (int k = 0; k < 3; ++k)
                Key(map, assets.bodyFont, mode ? Modes[k] : Qualities[k], Choice(row, k),
                    k == (mode ? int(o.screenMode) : o.aaSamples - 2), mode ? 35 : 47);
        }
    }
    // Cover the baked resume text and draw two explicit action buttons.
    DrawRectangleRounded(map.Rec({384, 650, 436, 106}), 0.08f, 4, Color{16, 27, 44, 255});
    ActionButton(map, assets.headingFont, "RESUME", ResumeBtn, focus == OptionRow::Resume, Yellow);
    ActionButton(map, assets.headingFont, "MAIN MENU", TitleBtn, focus == OptionRow::Title, Color{255, 171, 27, 255});
}
} // namespace Ui
