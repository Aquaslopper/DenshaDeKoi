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

        if (vertical != 0) {
            focus_ = std::clamp(focus_ + vertical, 0, 3);
        }
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

    static constexpr TableLayout::Rect Panel{40, 34, 1280, 700};
    static constexpr TableLayout::Rect Banner{40, 34, 1280, 116};
    static constexpr TableLayout::Rect Body{40, 150, 1280, 584};

    static constexpr TableLayout::Rect RoundsLabel{92, 232, 360, 56};
    static constexpr TableLayout::Rect RouteLabel{92, 392, 360, 56};

    static constexpr TableLayout::Rect BackButton{92, 596, 332, 88};
    static constexpr TableLayout::Rect StartButton{936, 596, 332, 88};

    static bool Contains(TableLayout::Rect r, TableLayout::Point2 p) {
        return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
    }

    static TableLayout::Rect RoundButton(int index) {
        return {548.0f + index * 132.0f, 214.0f, 108.0f, 92.0f};
    }

    static TableLayout::Rect RouteArrow(int direction) {
        return direction < 0 ? TableLayout::Rect{548, 374, 92, 92}
                             : TableLayout::Rect{1012, 374, 92, 92};
    }

    static TableLayout::Rect RouteValueBox() { return {648, 374, 356, 92}; }

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
                      viewport.H(TableLayout::ImageHeight), Color{0, 0, 0, 110});
    }

    static void DrawPanel(const Viewport& viewport) {
        const Rectangle outer = viewport.Rec(Panel);
        const float roundness = 0.03f;
        DrawRectangleRounded(outer, roundness, 12, Color{20, 52, 112, 255});
        DrawRectangleRoundedLinesEx(outer, roundness, 12, std::max(2.0f, 5.0f * viewport.scale),
                                    Color{46, 108, 196, 255});

        const Rectangle body = viewport.Rec({48, 150, 1264, 576});
        DrawRectangleRec(body, Color{58, 58, 58, 255});
        DrawRectangleLinesEx(Rectangle{body.x, body.y, body.width, body.height},
                             std::max(1.0f, 2.0f * viewport.scale), Color{92, 92, 92, 255});
    }

    static void DrawBanner(const Viewport& viewport, const MatchSettingsAssets& assets) {
        const Rectangle banner = viewport.Rec(Banner);
        DrawRectangleRounded(banner, 0.03f, 12, Color{20, 54, 132, 255});
        // Square off bottom corners of the banner.
        const Rectangle bannerBottom{banner.x, banner.y + banner.height / 2.0f, banner.width, banner.height / 2.0f};
        DrawRectangleRec(bannerBottom, Color{18, 48, 124, 255});
        DrawRectangleGradientV(static_cast<int>(banner.x), static_cast<int>(banner.y),
                               static_cast<int>(banner.width), static_cast<int>(banner.height),
                               Color{30, 72, 168, 255}, Color{16, 44, 120, 255});
        // Re-apply rounded top corners by redrawing panel border over banner corners.
        DrawRectangleRoundedLinesEx(viewport.Rec(Panel), 0.03f, 12, std::max(2.0f, 5.0f * viewport.scale),
                                    Color{46, 108, 196, 255});
        DrawRectangle(static_cast<int>(banner.x),
                      static_cast<int>(banner.y + banner.height - 4 * viewport.scale),
                      static_cast<int>(banner.width), std::max(2, static_cast<int>(4 * viewport.scale)),
                      Color{8, 24, 70, 255});

        const Font heading = assets.headingFont.texture.id != 0 ? assets.headingFont : GetFontDefault();
        const float titleSize = 58.0f * viewport.scale;
        const float spacing = 1.0f * viewport.scale;
        float x = banner.x + 36.0f * viewport.scale;
        const float y = banner.y + (banner.height - titleSize) / 2.0f;

        auto drawWord = [&](const char* text, Color color) {
            const Vector2 size = MeasureTextEx(heading, text, titleSize, spacing);
            DrawTextEx(heading, text, {x, y}, titleSize, spacing, color);
            x += size.x;
        };

        drawWord("DENSHA", Color{228, 46, 36, 255});
        drawWord(" ", WHITE);
        drawWord("KO!", Color{90, 186, 74, 255});
        drawWord(" ", WHITE);
        drawWord("64", Color{242, 214, 44, 255});
        drawWord("  ", WHITE);
        drawWord("Match Settings", WHITE);

        // Decorative train silhouette on the right of the banner.
        const Rectangle trainArea{banner.x + banner.width - 230.0f * viewport.scale,
                                  banner.y + 10.0f * viewport.scale,
                                  210.0f * viewport.scale, 92.0f * viewport.scale};
        DrawRectangleRounded({trainArea.x + 20 * viewport.scale, trainArea.y + 18 * viewport.scale,
                              150 * viewport.scale, 52 * viewport.scale},
                             0.2f, 6, Color{228, 232, 236, 230});
        DrawRectangleRounded({trainArea.x + 128 * viewport.scale, trainArea.y + 6 * viewport.scale,
                              54 * viewport.scale, 64 * viewport.scale},
                             0.15f, 6, Color{208, 216, 224, 230});
        DrawRectangleRounded({trainArea.x + 28 * viewport.scale, trainArea.y + 26 * viewport.scale,
                              34 * viewport.scale, 20 * viewport.scale},
                             0.15f, 4, Color{60, 120, 190, 220});
        DrawRectangleRounded({trainArea.x + 70 * viewport.scale, trainArea.y + 26 * viewport.scale,
                              34 * viewport.scale, 20 * viewport.scale},
                             0.15f, 4, Color{60, 120, 190, 220});
        DrawCircleV({trainArea.x + 58 * viewport.scale, trainArea.y + 72 * viewport.scale},
                    10 * viewport.scale, Color{40, 44, 52, 230});
        DrawCircleV({trainArea.x + 142 * viewport.scale, trainArea.y + 72 * viewport.scale},
                    10 * viewport.scale, Color{40, 44, 52, 230});
    }

    void DrawRoundsRow(const Viewport& viewport, const MatchSettingsAssets& assets) const {
        DrawLabel(viewport, assets, "Number of Rounds", RoundsLabel, focus_ == 0);
        for (int i = 0; i < static_cast<int>(RoundOptions.size()); ++i) {
            const int value = RoundOptions[static_cast<std::size_t>(i)];
            const bool selected = value == state_.rounds;
            const TableLayout::Rect r = RoundButton(i);
            const Color fill = selected ? Color{242, 214, 44, 255} : Color{34, 52, 116, 255};
            const Color border = selected ? Color{255, 236, 120, 255} : Color{86, 116, 190, 255};
            DrawButtonRect(viewport, r, fill, border, focus_ == 0 && selected);
            DrawCenteredText(viewport, assets.headingFont, std::to_string(value).c_str(), r, 52.0f,
                             selected ? Color{20, 20, 20, 255} : WHITE);
        }
    }

    void DrawRouteRow(const Viewport& viewport, const MatchSettingsAssets& assets) const {
        DrawLabel(viewport, assets, "Route", RouteLabel, focus_ == 1);

        const bool focus = focus_ == 1;
        const Color arrowFill = Color{34, 52, 116, 255};
        const Color arrowBorder = Color{86, 116, 190, 255};
        DrawButtonRect(viewport, RouteArrow(-1), arrowFill, arrowBorder, focus);
        DrawButtonRect(viewport, RouteArrow(1), arrowFill, arrowBorder, focus);
        DrawArrow(viewport, RouteArrow(-1), -1, WHITE);
        DrawArrow(viewport, RouteArrow(1), 1, WHITE);

        DrawButtonRect(viewport, RouteValueBox(), Color{34, 52, 116, 255}, Color{86, 116, 190, 255}, focus);
        const char* route = (state_.routeIndex >= 0 && state_.routeIndex < static_cast<int>(Routes.size()))
                                ? Routes[static_cast<std::size_t>(state_.routeIndex)]
                                : "";
        DrawCenteredText(viewport, assets.bodyFont, route, RouteValueBox(), 52.0f, WHITE);
    }

    void DrawBottomButtons(const Viewport& viewport, const MatchSettingsAssets& assets) const {
        const bool backFocus = focus_ == 2;
        const bool startFocus = focus_ == 3;

        DrawButtonRect(viewport, BackButton, Color{74, 74, 74, 255},
                       backFocus ? Color{255, 236, 120, 255} : Color{150, 150, 150, 255}, backFocus);
        DrawCenteredText(viewport, assets.headingFont, "BACK", BackButton, 58.0f, WHITE);

        DrawButtonRect(viewport, StartButton, Color{242, 214, 44, 255},
                       startFocus ? Color{255, 246, 170, 255} : Color{214, 180, 20, 255}, startFocus);
        DrawCenteredText(viewport, assets.headingFont, "Start", StartButton, 58.0f, Color{20, 20, 20, 255});
    }

    static void DrawLabel(const Viewport& viewport, const MatchSettingsAssets& assets, const char* text,
                          TableLayout::Rect r, bool focused) {
        const Font font = assets.bodyFont.texture.id != 0 ? assets.bodyFont : GetFontDefault();
        const Rectangle rect = viewport.Rec(r);
        const float size = 52.0f * viewport.scale;
        const float spacing = 1.0f * viewport.scale;
        const Vector2 m = MeasureTextEx(font, text, size, spacing);
        DrawTextEx(font, text, {rect.x, rect.y + (rect.height - m.y) / 2.0f}, size, spacing,
                   focused ? Color{255, 236, 120, 255} : WHITE);
    }

    static void DrawButtonRect(const Viewport& viewport, TableLayout::Rect r, Color fill, Color border, bool focused) {
        const Rectangle rect = viewport.Rec(r);
        const float roundness = 0.12f;
        DrawRectangleRounded(rect, roundness, 8, fill);
        const float thickness = std::max(2.0f, (focused ? 4.0f : 2.5f) * viewport.scale);
        DrawRectangleRoundedLinesEx(rect, roundness, 8, thickness, border);
        if (focused) {
            const Rectangle glow{rect.x - 3 * viewport.scale, rect.y - 3 * viewport.scale,
                                 rect.width + 6 * viewport.scale, rect.height + 6 * viewport.scale};
            DrawRectangleRoundedLinesEx(glow, roundness, 8, std::max(1.0f, 2.0f * viewport.scale),
                                        Color{255, 240, 150, 70});
        }
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
        const float s = 16.0f * viewport.scale;
        const float dx = static_cast<float>(direction) * s;
        DrawTriangle({cx + dx, cy}, {cx - dx, cy - s}, {cx - dx, cy + s}, color);
    }
};

} // namespace Ui
