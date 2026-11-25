#pragma once

#include "GameOptions.hpp"
#include "ui/UiViewport.hpp"

#include <raylib.h>

#include <string>

namespace Ui {

struct MatchOverMenuAssets {
    Texture2D panel;
    Texture2D stripe;
    Font headingFont;
    Font bodyFont;
};

enum class MatchOverAction { NONE, RESTART, EXIT };

class MatchOverMenu {
public:
    explicit MatchOverMenu(const std::filesystem::path& appDirectory) {
        const auto dir = appDirectory / "options" / "match-over";
        panel_ = LoadTexture((dir / "panel.png").string().c_str());
        stripe_ = LoadTexture((dir / "stripe.png").string().c_str());
        restartButton_ = LoadTexture((dir / "restart.png").string().c_str());
        exitButton_ = LoadTexture((dir / "exit.png").string().c_str());
        font_ = LoadFontEx((appDirectory / "fonts" / "Rajdhani-Bold.ttf").string().c_str(), 48, nullptr, 0);
    }

    ~MatchOverMenu() {
        if (panel_.id != 0) UnloadTexture(panel_);
        if (stripe_.id != 0) UnloadTexture(stripe_);
        if (restartButton_.id != 0) UnloadTexture(restartButton_);
        if (exitButton_.id != 0) UnloadTexture(exitButton_);
        if (font_.texture.id != 0) UnloadFont(font_);
    }

    MatchOverMenu(const MatchOverMenu&) = delete;
    MatchOverMenu& operator=(const MatchOverMenu&) = delete;

    void SetResult(int playerScore, int cpuScore, int round) {
        playerScore_ = playerScore;
        cpuScore_ = cpuScore;
        round_ = round;
    }

    [[nodiscard]] MatchOverAction Update(TableLayout::Point2 pointer, bool moved, bool pressed, bool escape) {
        if (escape) return MatchOverAction::EXIT;

        const Rectangle panelRect = PanelRect();

        // Track hover
        hoverRestart_ = InRect(pointer, RestartButtonRect(panelRect));
        hoverExit_ = InRect(pointer, ExitButtonRect(panelRect));

        if (pressed) {
            if (hoverRestart_) return MatchOverAction::RESTART;
            if (hoverExit_) return MatchOverAction::EXIT;
        }

        return MatchOverAction::NONE;
    }

    void Draw(const Viewport& viewport, TableLayout::Point2 pointer) const {
        const Rectangle panelRect = PanelRect();

        // Subtle backdrop dim
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.35f));

        if (panel_.id != 0) {
            DrawTexturePro(panel_,
                           {0.0f, 0.0f, static_cast<float>(panel_.width), static_cast<float>(panel_.height)},
                           ToScreen(viewport, panelRect), {0.0f, 0.0f}, 0.0f, WHITE);
        }

        // Title
        DrawLabel(viewport, "MATCH OVER", panelRect.x + 56.0f, panelRect.y + 60.0f, 42.0f, WHITE);

        // Scores
        DrawLabel(viewport, ("YOU  " + std::to_string(playerScore_)).c_str(),
                  panelRect.x + 56.0f, panelRect.y + 140.0f, 36.0f, WHITE);
        DrawLabel(viewport, ("CPU  " + std::to_string(cpuScore_)).c_str(),
                  panelRect.x + 56.0f, panelRect.y + 190.0f, 36.0f, WHITE);

        // Round count
        DrawLabel(viewport, ("Rounds played: " + std::to_string(round_)).c_str(),
                  panelRect.x + 56.0f, panelRect.y + 260.0f, 24.0f, LIGHTGRAY);

        // Buttons
        const Rectangle restartRect = RestartButtonRect(panelRect);
        if (restartButton_.id != 0) {
            DrawTexturePro(restartButton_,
                           {0.0f, 0.0f, static_cast<float>(restartButton_.width), static_cast<float>(restartButton_.height)},
                           ToScreen(viewport, restartRect), {0.0f, 0.0f}, 0.0f,
                           hoverRestart_ ? WHITE : Color{225, 225, 235, 255});
        }

        const Rectangle exitRect = ExitButtonRect(panelRect);
        if (exitButton_.id != 0) {
            DrawTexturePro(exitButton_,
                           {0.0f, 0.0f, static_cast<float>(exitButton_.width), static_cast<float>(exitButton_.height)},
                           ToScreen(viewport, exitRect), {0.0f, 0.0f}, 0.0f,
                           hoverExit_ ? WHITE : Color{225, 225, 235, 255});
        }
    }

private:
    static Rectangle PanelRect() { return {365.0f, 165.0f, 630.0f, 350.0f}; }
    static Rectangle RestartButtonRect(Rectangle panel) { return {panel.x + 56.0f, panel.y + 280.0f, 240.0f, 64.0f}; }
    static Rectangle ExitButtonRect(Rectangle panel) { return {panel.x + 334.0f, panel.y + 280.0f, 240.0f, 64.0f}; }

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

    Texture2D panel_{};
    Texture2D stripe_{};
    Texture2D restartButton_{};
    Texture2D exitButton_{};
    Font font_{};
    int playerScore_ = 0;
    int cpuScore_ = 0;
    int round_ = 0;
    bool hoverRestart_ = false;
    bool hoverExit_ = false;
};

} // namespace Ui