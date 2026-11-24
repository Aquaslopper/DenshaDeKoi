#pragma once

#include "ui/UiViewport.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <string>

namespace Ui {

enum class MatchSettingsAction { NONE, BACK, START };

struct MatchSettingsAssets {
    Texture2D logo{};
    Font headingFont{};
    Font bodyFont{};
};

struct MatchSettingsState {
    int rounds = 3;
    int routeIndex = 0;
};

// Match setup popup shown on the title screen before a match starts.
// Layout is authored in the established 1360x768 UI space.
class MatchSettingsMenu {
public:
    static constexpr std::array<int, 4> RoundOptions{{1, 3, 6, 12}};
    static constexpr std::array<const char*, 1> Routes{{"The City"}};

    [[nodiscard]] MatchSettingsAction Update(TableLayout::Point2 pointer, bool moved, bool pressed,
                                             int vertical, int horizontal, bool activate, bool back) {
        if (back) return MatchSettingsAction::BACK;

        if (moved) {
            const int hit = Hit(pointer);
            if (hit != -1) focus_ = hit;
        }

        if (vertical != 0) focus_ = std::clamp(focus_ + vertical, 0, 3);
        if (horizontal != 0) {
            if (focus_ == 0) StepRounds(horizontal);
            else if (focus_ == 1) StepRoute(horizontal);
            else focus_ = focus_ == 2 ? 3 : 2;
        }

        if (activate) return Activate();

        if (pressed) {
            const int hit = Hit(pointer);
            if (hit == -1) return MatchSettingsAction::NONE;
            focus_ = hit;
            if (hit == 0) {
                for (int i = 0; i < static_cast<int>(RoundOptions.size()); ++i) {
                    if (Contains(RoundButton(i), pointer)) {
                        state_.rounds = RoundOptions[static_cast<std::size_t>(i)];
                        return MatchSettingsAction::NONE;
                    }
                }
            } else if (hit == 1) {
                if (Contains(RouteArrow(-1), pointer)) StepRoute(-1);
                else if (Contains(RouteArrow(1), pointer)) StepRoute(1);
                return MatchSettingsAction::NONE;
            }
            return Activate();
        }
        return MatchSettingsAction::NONE;
    }

    void Draw(const Viewport& viewport, const MatchSettingsAssets& assets) const {
        DrawDim(viewport);
        DrawPanel(viewport);
        DrawBanner(viewport, assets);
        DrawRoundsRow(viewport, assets);
        DrawRouteRow(viewport, assets);
        DrawBottomButtons(viewport, assets);
    }

    [[nodiscard]] const MatchSettingsState& State() const { return state_; }
    [[nodiscard]] MatchSettingsState& State() { return state_; }
    void ResetFocus() { focus_ = 3; }

private:
    // Focus rows: 0 = rounds, 1 = route, 2 = back, 3 = start.
    int focus_ = 3;
    MatchSettingsState state_{};

    static constexpr TableLayout::Rect Panel{48, 40, 1264, 688};
    static constexpr TableLayout::Rect Banner{48, 40, 1264, 112};
    static constexpr TableLayout::Rect Body{56, 152, 1248, 568};

    static constexpr TableLayout::Rect RoundsLabel{104, 236, 360, 64};
    static constexpr TableLayout::Rect RouteLabel{104, 392, 220, 64};

    static constexpr TableLayout::Rect BackButton{104, 584, 320, 92};
    static constexpr TableLayout::Rect StartButton{936, 584, 320, 92};

    static bool Contains(TableLayout::Rect r, TableLayout::Point2 p) {
        return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
    }

    static TableLayout::Rect RoundButton(int index) {
        return {556.0f + index * 128.0f, 222.0f, 104.0f, 92.0f};
    }

    static TableLayout::Rect RouteArrow(int direction) {
        return direction < 0 ? TableLayout::Rect{556, 378, 96, 92}
                             : TableLayout::Rect{1020, 378, 96, 92};
    }

    static TableLayout::Rect RouteValueBox() { return {660, 378, 352, 92}; }

    [[nodiscard]] int Hit(TableLayout::Point2 p) const {
        for (int i = 0; i < static_cast<int>(RoundOptions.size()); ++i) {
            if (Contains(RoundButton(i), p)) return 0;
        }
        if (Contains(RouteArrow(-1), p) || Contains(RouteArrow(1), p) || Contains(RouteValueBox(), p)) return 1;
        if (Contains(BackButton, p)) return 2;
        if (Contains(StartButton, p)) return 3;
        return -1;
    }

    void StepRounds(int direction) {
        int idx = 0;
        for (int i = 0; i < static_cast<int>(RoundOptions.size()); ++i) {
            if (RoundOptions[static_cast<std::size_t>(i)] == state_.rounds) idx = i;
        }
        idx = std::clamp(idx + direction, 0, static_cast<int>(RoundOptions.size()) - 1);
        state_.rounds = RoundOptions[static_cast<std::size_t>(idx)];
    }

    void StepRoute(int direction) {
        const int count = static_cast<int>(Routes.size());
        if (count <= 0) return;
        state_.routeIndex = (state_.routeIndex + direction % count + count) % count;
    }

    [[nodiscard]] MatchSettingsAction Activate() const {
        if (focus_ == 2) return MatchSettingsAction::BACK;
        if (focus_ == 3) return MatchSettingsAction::START;
        return MatchSettingsAction::NONE;
    }

    static void DrawDim(const Viewport& viewport) {
        DrawRectangle(viewport.X(0), viewport.Y(0), viewport.W(TableLayout::ImageWidth),
                      viewport.H(TableLayout::ImageHeight), Color{0, 0, 0, 90});
    }

    static void DrawPanel(const Viewport& viewport) {
        const Rectangle outer = viewport.Rec(Panel);
        DrawRectangleRounded(outer, 0.025f, 12, Color{16, 56, 140, 255});
        DrawRectangleRoundedLinesEx(outer, 0.025f, 12, std::max(2.0f, 5.0f * viewport.scale),
                                    Color{52, 116, 204, 255});

        const Rectangle body = viewport.Rec(Body);
        DrawRectangleRec(body, Color{74, 74, 74, 255});
    }

    static void DrawBanner(const Viewport& viewport, const MatchSettingsAssets& assets) {
        const Rectangle banner = viewport.Rec(Banner);
        DrawRectangleRounded(banner, 0.025f, 12, Color{20, 64, 160, 255});
        // Square off bottom corners.
        DrawRectangleRec({banner.x, banner.y + banner.height * 0.5f, banner.width, banner.height * 0.5f},
                         Color{14, 52, 136, 255});
        DrawRectangleGradientV(static_cast<int>(banner.x), static_cast<int>(banner.y),
                               static_cast<int>(banner.width), static_cast<int>(banner.height),
                               Color{30, 86, 190, 255}, Color{12, 44, 124, 255});
        // Restore rounded outer border.
        DrawRectangleRoundedLinesEx(viewport.Rec(Panel), 0.025f, 12, std::max(2.0f, 5.0f * viewport.scale),
                                    Color{52, 116, 204, 255});
        // Banner bottom separator.
        DrawRectangle(static_cast<int>(banner.x),
                      static_cast<int>(banner.y + banner.height - 3.0f * viewport.scale),
                      static_cast<int>(banner.width), std::max(2, static_cast<int>(3.0f * viewport.scale)),
                      Color{10, 36, 100, 255});

        const Font heading = assets.headingFont.texture.id != 0 ? assets.headingFont : GetFontDefault();
        const float size = 62.0f * viewport.scale;
        const float spacing = 1.0f * viewport.scale;
        float x = banner.x + 34.0f * viewport.scale;
        const float y = banner.y + (banner.height - size) / 2.0f;

        auto drawOutlined = [&](const char* text, Color color, Color outline) {
            const Vector2 m = MeasureTextEx(heading, text, size, spacing);
            const float o = std::max(1.0f, 2.0f * viewport.scale);
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx == 0 && dy == 0) continue;
                    DrawTextEx(heading, text, {x + dx * o, y + dy * o}, size, spacing, outline);
                }
            }
            DrawTextEx(heading, text, {x, y}, size, spacing, color);
            x += m.x;
        };

        drawOutlined("DENSHA", Color{230, 36, 36, 255}, Color{255, 255, 255, 220});
        drawOutlined(" ", WHITE, Color{0, 0, 0, 0});
        drawOutlined("KO!", Color{86, 184, 72, 255}, Color{255, 255, 255, 220});
        drawOutlined(" ", WHITE, Color{0, 0, 0, 0});
        drawOutlined("64", Color{246, 214, 36, 255}, Color{255, 255, 255, 220});
        x += 18.0f * viewport.scale;
        DrawTextEx(heading, "Match Settings", {x, y}, size, spacing, WHITE);
    }

    void DrawRoundsRow(const Viewport& viewport, const MatchSettingsAssets& assets) const {
        DrawLabel(viewport, assets, "Number of Rounds", RoundsLabel);
        for (int i = 0; i < static_cast<int>(RoundOptions.size()); ++i) {
            const int value = RoundOptions[static_cast<std::size_t>(i)];
            const bool selected = value == state_.rounds;
            const TableLayout::Rect r = RoundButton(i);
            const Color fill = selected ? Color{240, 204, 24, 255} : Color{36, 58, 128, 255};
            const Color border = selected ? Color{255, 230, 90, 255} : Color{92, 128, 200, 255};
            DrawButtonRect(viewport, r, fill, border);
            DrawCenteredText(viewport, assets.headingFont, std::to_string(value).c_str(), r, 54.0f,
                             selected ? Color{28, 28, 28, 255} : WHITE);
        }
    }

    void DrawRouteRow(const Viewport& viewport, const MatchSettingsAssets& assets) const {
        DrawLabel(viewport, assets, "Route", RouteLabel);

        const Color fill = Color{36, 58, 128, 255};
        const Color border = Color{92, 128, 200, 255};
        DrawButtonRect(viewport, RouteArrow(-1), fill, border);
        DrawButtonRect(viewport, RouteArrow(1), fill, border);
        DrawArrow(viewport, RouteArrow(-1), -1, Color{240, 204, 24, 255});
        DrawArrow(viewport, RouteArrow(1), 1, Color{240, 204, 24, 255});

        DrawButtonRect(viewport, RouteValueBox(), fill, border);
        const char* route = (state_.routeIndex >= 0 && state_.routeIndex < static_cast<int>(Routes.size()))
                                ? Routes[static_cast<std::size_t>(state_.routeIndex)]
                                : "";
        DrawCenteredText(viewport, assets.bodyFont, route, RouteValueBox(), 52.0f, WHITE);
    }

    void DrawBottomButtons(const Viewport& viewport, const MatchSettingsAssets& assets) const {
        DrawButtonRect(viewport, BackButton, Color{82, 82, 82, 255}, Color{150, 150, 150, 255});
        DrawCenteredText(viewport, assets.headingFont, "BACK", BackButton, 62.0f, WHITE);

        DrawButtonRect(viewport, StartButton, Color{240, 204, 24, 255}, Color{255, 228, 90, 255});
        DrawCenteredText(viewport, assets.headingFont, "Start", StartButton, 62.0f, Color{28, 28, 28, 255});
    }

    static void DrawLabel(const Viewport& viewport, const MatchSettingsAssets& assets, const char* text,
                          TableLayout::Rect r) {
        const Font font = assets.bodyFont.texture.id != 0 ? assets.bodyFont : GetFontDefault();
        const Rectangle rect = viewport.Rec(r);
        const float size = 54.0f * viewport.scale;
        const float spacing = 1.0f * viewport.scale;
        const Vector2 m = MeasureTextEx(font, text, size, spacing);
        DrawTextEx(font, text, {rect.x, rect.y + (rect.height - m.y) / 2.0f}, size, spacing, WHITE);
    }

    static void DrawButtonRect(const Viewport& viewport, TableLayout::Rect r, Color fill, Color border) {
        const Rectangle rect = viewport.Rec(r);
        DrawRectangleRounded(rect, 0.14f, 8, fill);
        DrawRectangleRoundedLinesEx(rect, 0.14f, 8, std::max(2.0f, 3.0f * viewport.scale), border);
    }

    static void DrawCenteredText(const Viewport& viewport, Font font, const char* text, TableLayout::Rect r,
                                 float fontSize, Color color) {
        const Font f = font.texture.id != 0 ? font : GetFontDefault();
        const Rectangle rect = viewport.Rec(r);
        const float size = fontSize * viewport.scale;
        const float spacing = 1.0f * viewport.scale;
        const Vector2 m = MeasureTextEx(f, text, size, spacing);
        DrawTextEx(f, text, {rect.x + (rect.width - m.x) / 2.0f, rect.y + (rect.height - m.y) / 2.0f},
                   size, spacing, color);
    }

    static void DrawArrow(const Viewport& viewport, TableLayout::Rect r, int direction, Color color) {
        const Rectangle rect = viewport.Rec(r);
        const float cx = rect.x + rect.width / 2.0f;
        const float cy = rect.y + rect.height / 2.0f;
        const float s = 15.0f * viewport.scale;
        const float dx = static_cast<float>(direction) * s;
        DrawTriangle({cx + dx, cy}, {cx - dx, cy - s * 1.2f}, {cx - dx, cy + s * 1.2f}, color);
    }
};

} // namespace Ui
