#pragma once

#include "TableHud.hpp"
#include "ui/UiViewport.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>

namespace Ui {

enum class MatchSettingsAction { NONE, BACK, START };

class MatchSettingsMenu {
public:
    explicit MatchSettingsMenu(const std::filesystem::path& appDirectory) {
        const auto dir = appDirectory / "options" / "match-settings";
        panel_ = LoadTexture((dir / "matchsettingsmenu.png").string().c_str());
        buttons_ = LoadTexture((dir / "buttons.png").string().c_str());
        backButton_ = LoadTexture((dir / "back.png").string().c_str());
        startButton_ = LoadTexture((dir / "start.png").string().c_str());
        font_ = LoadFontEx((appDirectory / "fonts" / "Rajdhani-Bold.ttf").string().c_str(), 48, nullptr, 0);
    }

    ~MatchSettingsMenu() {
        if (panel_.id != 0) UnloadTexture(panel_);
        if (buttons_.id != 0) UnloadTexture(buttons_);
        if (backButton_.id != 0) UnloadTexture(backButton_);
        if (startButton_.id != 0) UnloadTexture(startButton_);
        if (font_.texture.id != 0) UnloadFont(font_);
    }

    MatchSettingsMenu(const MatchSettingsMenu&) = delete;
    MatchSettingsMenu& operator=(const MatchSettingsMenu&) = delete;

    [[nodiscard]] int SelectedRounds() const { return kRoundOptions[roundIndex_]; }
    [[nodiscard]] const char* SelectedRoute() const { return kRouteOptions[routeIndex_]; }

    void ResetFocus() { focusRow_ = 0; focusAnim_ = 0.0f; }

    [[nodiscard]] MatchSettingsAction Update(TableLayout::Point2 pointer, bool moved, bool pressed,
                                             int vertical, int horizontal, bool activate, bool escape) {
        if (escape) return MatchSettingsAction::BACK;

        const Rectangle panelRect = PanelRect();

        // Track hover for visual feedback.
        hoverRound_ = -1;
        hoverRoute_ = false;
        hoverBack_ = false;
        hoverStart_ = false;

        if (moved) {
            for (int i = 0; i < 4; ++i) {
                if (InRect(pointer, RoundButtonRect(panelRect, i))) {
                    hoverRound_ = i;
                    focusRow_ = 0;
                    break;
                }
            }
            if (InRect(pointer, RouteBarRect(panelRect))) {
                hoverRoute_ = true;
                focusRow_ = 1;
            }
            if (InRect(pointer, BackButtonRect(panelRect))) {
                hoverBack_ = true;
                focusRow_ = 2;
                bottomFocus_ = 0;
            } else if (InRect(pointer, StartButtonRect(panelRect))) {
                hoverStart_ = true;
                focusRow_ = 2;
                bottomFocus_ = 1;
            }
        }

        if (vertical != 0) {
            focusRow_ = (focusRow_ + (vertical > 0 ? 1 : 2)) % 3;
        }

        if (horizontal != 0) {
            if (focusRow_ == 0) {
                roundIndex_ = (roundIndex_ + (horizontal > 0 ? 1 : 3)) % 4;
            } else if (focusRow_ == 1) {
                routeIndex_ = (routeIndex_ + (horizontal > 0 ? 1 : static_cast<int>(kRouteOptions.size()) - 1)) %
                              static_cast<int>(kRouteOptions.size());
            } else {
                bottomFocus_ = 1 - bottomFocus_;
            }
        }

        if (pressed) {
            for (int i = 0; i < 4; ++i) {
                if (InRect(pointer, RoundButtonRect(panelRect, i))) {
                    roundIndex_ = i;
                    focusRow_ = 0;
                    pressAnim_[i] = 1.0f; // Trigger press animation
                    return MatchSettingsAction::NONE;
                }
            }
            if (InRect(pointer, RouteLeftArrowRect(panelRect))) {
                routeIndex_ = (routeIndex_ + static_cast<int>(kRouteOptions.size()) - 1) %
                              static_cast<int>(kRouteOptions.size());
                focusRow_ = 1;
                pressAnim_[4] = 1.0f;
                return MatchSettingsAction::NONE;
            }
            if (InRect(pointer, RouteRightArrowRect(panelRect))) {
                routeIndex_ = (routeIndex_ + 1) % static_cast<int>(kRouteOptions.size());
                focusRow_ = 1;
                pressAnim_[5] = 1.0f;
                return MatchSettingsAction::NONE;
            }
            if (InRect(pointer, BackButtonRect(panelRect))) {
                pressAnim_[6] = 1.0f;
                return MatchSettingsAction::BACK;
            }
            if (InRect(pointer, StartButtonRect(panelRect))) {
                pressAnim_[7] = 1.0f;
                return MatchSettingsAction::START;
            }
        }

        if (activate) {
            if (focusRow_ == 2 && bottomFocus_ == 0) return MatchSettingsAction::BACK;
            return MatchSettingsAction::START;
        }

        return MatchSettingsAction::NONE;
    }

    void Draw(const Viewport& viewport, TableLayout::Point2 pointer) const {
        const Rectangle panelRect = PanelRect();

        // Subtle backdrop dim behind the modal dialog
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.35f));

        if (panel_.id != 0) {
            DrawTexturePro(panel_,
                           {0.0f, 0.0f, static_cast<float>(panel_.width), static_cast<float>(panel_.height)},
                           ToScreen(viewport, panelRect), {0.0f, 0.0f}, 0.0f, WHITE);
        }

        // Row labels
        DrawLabel(viewport, "Number", panelRect.x + 56.0f, panelRect.y + 114.0f, 26.0f, WHITE);
        DrawLabel(viewport, "of Rounds", panelRect.x + 56.0f, panelRect.y + 139.0f, 26.0f, WHITE);
        DrawLabel(viewport, "Route", panelRect.x + 56.0f, panelRect.y + 198.0f, 26.0f, WHITE);

        // Animated focus indicator bar - smoothly transitions between rows.
        const float dt = GetFrameTime();
        float focusTarget = static_cast<float>(focusRow_);
        // Note: focusAnim_ is updated in Draw (const) via mutable
        if (std::fabs(focusAnim_ - focusTarget) > 0.02f) {
            focusAnim_ += (focusTarget - focusAnim_) * 0.15f;
        } else {
            focusAnim_ = focusTarget;
        }

        const float focusY = (focusAnim_ == 0) ? panelRect.y + 116.0f : panelRect.y + 195.0f;
        const float focusH = (focusAnim_ == 0) ? 44.0f : 32.0f;
        DrawRectangleRec(ToScreen(viewport, {panelRect.x + 42.0f, focusY, 5.0f, focusH}), YELLOW);

        // Update press animations.
        for (int i = 0; i < 8; ++i) {
            if (pressAnim_[i] > 0.0f) pressAnim_[i] = std::max(0.0f, pressAnim_[i] - dt * 8.0f);
        }

        // Round buttons: 1, 3, 6, 12
        const Rectangle yellowSrc{150.0f, 0.0f, 160.0f, 97.0f};
        const Rectangle silverSrc{347.0f, 0.0f, 159.0f, 97.0f};
        for (int i = 0; i < 4; ++i) {
            const Rectangle dst = RoundButtonRect(panelRect, i);
            const bool selected = (i == roundIndex_);
            const bool hovered = hoverRound_ == i;
            const float pressScale = 1.0f - pressAnim_[i] * 0.08f; // Slight scale down on press

            if (buttons_.id != 0) {
                const Color tint = selected ? WHITE : (hovered ? Color{200, 210, 235, 255} : Color{140, 150, 180, 255});
                Rectangle drawDst = ToScreen(viewport, dst);
                if (pressScale < 1.0f) {
                    const float cx = drawDst.x + drawDst.width * 0.5f;
                    const float cy = drawDst.y + drawDst.height * 0.5f;
                    drawDst.width *= pressScale;
                    drawDst.height *= pressScale;
                    drawDst.x = cx - drawDst.width * 0.5f;
                    drawDst.y = cy - drawDst.height * 0.5f;
                }
                DrawTexturePro(buttons_, selected ? yellowSrc : silverSrc, drawDst, {0.0f, 0.0f}, 0.0f, tint);
            }
            const std::string label = std::to_string(kRoundOptions[i]);
            DrawCenteredLabel(viewport, label.c_str(), dst, 26.0f, selected ? BLACK : WHITE);
        }

        // Route selector bar with arrow hover feedback.
        const Rectangle routeRect = RouteBarRect(panelRect);
        if (buttons_.id != 0) {
            const Rectangle barSrc{0.0f, 106.0f, 665.0f, 98.0f};
            DrawTexturePro(buttons_, barSrc, ToScreen(viewport, routeRect), {0.0f, 0.0f}, 0.0f, WHITE);
        }
        DrawCenteredLabel(viewport, kRouteOptions[routeIndex_], routeRect, 25.0f, WHITE);

        // Route arrows with hover highlight.
        const Rectangle leftArrow = RouteLeftArrowRect(panelRect);
        const Rectangle rightArrow = RouteRightArrowRect(panelRect);
        if (buttons_.id != 0 && hoverRoute_) {
            const Rectangle arrowSrc{506.0f, 106.0f, 46.0f, 98.0f};
            DrawTexturePro(buttons_, arrowSrc, ToScreen(viewport, leftArrow), {0.0f, 0.0f}, 0.0f, WHITE);
            DrawTexturePro(buttons_, arrowSrc, ToScreen(viewport, rightArrow), {0.0f, 0.0f}, 0.0f, WHITE);
        }

        // BACK and Start buttons with press animation.
        const Rectangle backRect = BackButtonRect(panelRect);
        if (backButton_.id != 0) {
            const bool backHot = hoverBack_ || (focusRow_ == 2 && bottomFocus_ == 0);
            const float pressScale = 1.0f - pressAnim_[6] * 0.1f;
            Color tint = backHot ? Color{255, 255, 255, 255} : Color{225, 225, 235, 255};
            if (focusRow_ == 2 && bottomFocus_ == 0) tint = YELLOW;
            Rectangle drawBack = ToScreen(viewport, backRect);
            if (pressScale < 1.0f) {
                const float cx = drawBack.x + drawBack.width * 0.5f;
                const float cy = drawBack.y + drawBack.height * 0.5f;
                drawBack.width *= pressScale;
                drawBack.height *= pressScale;
                drawBack.x = cx - drawBack.width * 0.5f;
                drawBack.y = cy - drawBack.height * 0.5f;
            }
            DrawTexturePro(backButton_,
                           {0.0f, 0.0f, static_cast<float>(backButton_.width), static_cast<float>(backButton_.height)},
                           drawBack, {0.0f, 0.0f}, 0.0f, tint);
        }

        const Rectangle startRect = StartButtonRect(panelRect);
        if (startButton_.id != 0) {
            const bool startHot = hoverStart_ || (focusRow_ == 2 && bottomFocus_ == 1);
            const float pressScale = 1.0f - pressAnim_[7] * 0.1f;
            Color tint = startHot ? WHITE : Color{240, 240, 220, 255};
            if (focusRow_ == 2 && bottomFocus_ == 1) tint = YELLOW;
            Rectangle drawStart = ToScreen(viewport, startRect);
            if (pressScale < 1.0f) {
                const float cx = drawStart.x + drawStart.width * 0.5f;
                const float cy = drawStart.y + drawStart.height * 0.5f;
                drawStart.width *= pressScale;
                drawStart.height *= pressScale;
                drawStart.x = cx - drawStart.width * 0.5f;
                drawStart.y = cy - drawStart.height * 0.5f;
            }
            DrawTexturePro(startButton_,
                           {0.0f, 0.0f, static_cast<float>(startButton_.width), static_cast<float>(startButton_.height)},
                           drawStart, {0.0f, 0.0f}, 0.0f, tint);
        }
    }

private:
    static constexpr std::array<int, 4> kRoundOptions{1, 3, 6, 12};
    static constexpr std::array<const char*, 1> kRouteOptions{"The City"};

    static Rectangle PanelRect() {
        return {365.0f, 165.0f, 630.0f, 350.0f};
    }

    static Rectangle RoundButtonRect(Rectangle panel, int index) {
        return {panel.x + 220.0f + static_cast<float>(index) * 90.0f, panel.y + 118.0f, 78.0f, 48.0f};
    }

    static Rectangle RouteBarRect(Rectangle panel) {
        return {panel.x + 220.0f, panel.y + 186.0f, 368.0f, 52.0f};
    }

    static Rectangle RouteLeftArrowRect(Rectangle panel) {
        const Rectangle bar = RouteBarRect(panel);
        return {bar.x, bar.y, 46.0f, bar.height};
    }

    static Rectangle RouteRightArrowRect(Rectangle panel) {
        const Rectangle bar = RouteBarRect(panel);
        return {bar.x + bar.width - 46.0f, bar.y, 46.0f, bar.height};
    }

    static Rectangle BackButtonRect(Rectangle panel) {
        return {panel.x + 56.0f, panel.y + 260.0f, 184.0f, 64.0f};
    }

    static Rectangle StartButtonRect(Rectangle panel) {
        return {panel.x + 278.0f, panel.y + 260.0f, 310.0f, 64.0f};
    }

    static Rectangle ToScreen(const Viewport& viewport, Rectangle rect) {
        return viewport.Rec({rect.x, rect.y, rect.width, rect.height});
    }

    static bool InRect(TableLayout::Point2 p, Rectangle r) {
        return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
    }

    void DrawLabel(const Viewport& viewport, const char* text, float x, float y, float size, Color color) const {
        const float scaledSize = size * viewport.scale;
        const Vector2 pos{viewport.box.x + x * viewport.scale, viewport.box.y + y * viewport.scale};
        if (font_.texture.id != 0) {
            DrawTextEx(font_, text, pos, scaledSize, 1.0f * viewport.scale, color);
        } else {
            DrawText(text, static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(scaledSize), color);
        }
    }

    void DrawCenteredLabel(const Viewport& viewport, const char* text, Rectangle rect, float size, Color color) const {
        const float scaledSize = size * viewport.scale;
        const float spacing = 1.0f * viewport.scale;
        Vector2 textSize{};
        if (font_.texture.id != 0) {
            textSize = MeasureTextEx(font_, text, scaledSize, spacing);
        } else {
            textSize = {static_cast<float>(MeasureText(text, static_cast<int>(scaledSize))), scaledSize};
        }
        const Rectangle screenRect = ToScreen(viewport, rect);
        const Vector2 pos{
            screenRect.x + (screenRect.width - textSize.x) * 0.5f,
            screenRect.y + (screenRect.height - textSize.y) * 0.5f,
        };
        if (font_.texture.id != 0) {
            DrawTextEx(font_, text, pos, scaledSize, spacing, color);
        } else {
            DrawText(text, static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(scaledSize), color);
        }
    }

    Texture2D panel_{};
    Texture2D buttons_{};
    Texture2D backButton_{};
    Texture2D startButton_{};
    Font font_{};
    int roundIndex_ = 1; // Default: 3 rounds
    int routeIndex_ = 0; // Default: The City
    int focusRow_ = 0;   // 0 = rounds, 1 = route, 2 = bottom buttons
    int bottomFocus_ = 1; // 0 = BACK, 1 = Start
    mutable float focusAnim_ = 0.0f;
    mutable float pressAnim_[8] = {}; // 4 round buttons + 2 route arrows + back + start
    int hoverRound_ = -1;
    bool hoverRoute_ = false;
    bool hoverBack_ = false;
    bool hoverStart_ = false;
};

} // namespace Ui
