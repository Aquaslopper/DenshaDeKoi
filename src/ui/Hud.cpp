#include "ui/Hud.hpp"

#include "ui/PauseRadioMenu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>

namespace Ui {
namespace {

void DrawBox(const Viewport& map, TableLayout::Rect rect, Color fill, Color border,
             float borderPx = 1.5f) {
    DrawRectangle(map.X(rect.x), map.Y(rect.y), map.W(rect.width), map.H(rect.height), fill);
    DrawRectangleLinesEx(map.Rec(rect), std::max(1.0f, borderPx * map.scale), border);
}

// Round-start bullet-time beat: the logo spins three turns above the middle of the table and
// slows to a front view, where it stays for the match cue. Width follows cos(angle) for the
// Y-axis roll; height breathes with sin(angle) to suggest depth. `opacity` fades it out with the cue.
constexpr float RoundIntroLogoCenterY = 330.0f;
constexpr float RoundIntroCueCenterY = 452.0f;

void DrawRoundIntroLogo(const Viewport& map, Texture2D logo, float progress, float opacity) {
    constexpr float Pi = 3.14159265f;
    constexpr float LogoWidth = 380.0f;
    constexpr float LogoHeight = 132.0f;
    const float u = std::clamp(progress, 0.0f, 1.0f);
    const float ease = 1.0f - (1.0f - u) * (1.0f - u); // Fast start, slow stop.
    const float angle = 6.0f * Pi * ease;
    const float c = std::cos(angle);
    const float width = LogoWidth * std::fabs(c);
    const float height = LogoHeight * (1.0f + 0.1f * std::sin(angle));
    const float centerX = static_cast<float>(TableLayout::ImageWidth) * 0.5f;
    const TableLayout::Rect box{centerX - width * 0.5f, RoundIntroLogoCenterY - height * 0.5f, width, height};
    // A negative source width mirrors the texture, so the back of the roll shows the logo reversed.
    const float sourceWidth = c < 0.0f ? -static_cast<float>(logo.width) : static_cast<float>(logo.width);
    const auto tint = Color{255, 255, 255, static_cast<unsigned char>(std::lround(opacity * 255.0f))};
    DrawTexturePro(logo, {0, 0, sourceWidth, static_cast<float>(logo.height)}, map.Rec(box), {0, 0}, 0.0f, tint);
}

// Plain match/round cue in the options-menu body font, centred under the logo with no backing box.
// A soft dark offset keeps it readable over the table.
void DrawRoundIntroCue(const Viewport& map, Font font, const char* text, float opacity) {
    constexpr float FontSize = 44.0f;
    const float size = FontSize * map.scale;
    const float spacing = 0.5f * map.scale;
    const Rectangle line = map.Rec({0.0f, RoundIntroCueCenterY - FontSize * 0.5f,
                                    static_cast<float>(TableLayout::ImageWidth), FontSize});
    const Vector2 measured = MeasureTextEx(font, text, size, spacing);
    const Vector2 origin{line.x + (line.width - measured.x) * 0.5f, line.y + (line.height - measured.y) * 0.5f};
    const auto alpha = static_cast<unsigned char>(std::lround(opacity * 255.0f));
    const float shadow = std::max(1.0f, 2.0f * map.scale);
    DrawTextEx(font, text, {origin.x + shadow, origin.y + shadow}, size, spacing, Color{0, 0, 0, static_cast<unsigned char>(alpha * 3 / 4)});
    DrawTextEx(font, text, origin, size, spacing, Color{255, 255, 255, alpha});
}

} // namespace

void DrawHud(const Viewport& viewport, const TableGame& game, const TableHudState& hud,
             Texture2D logo, Font headingFont, Font bodyFont) {
    const Viewport& map = viewport;
    DrawRectangle(map.X(8), map.Y(8), map.W(500), map.H(102), Color{15, 20, 27, 220});
    const std::string label = "ROUND " + std::to_string(game.Round()) + " / " +
        std::to_string(game.TotalRounds()) +
        "    YOU " + std::to_string(game.Scores()[PlayerIndex(Player::PLAYER)]) +
        "  -  CPU " + std::to_string(game.Scores()[PlayerIndex(Player::OPPONENT)]);
    DrawText(label.c_str(), map.X(20), map.Y(18), map.Font(22), WHITE);
    DrawText(game.Status().c_str(), map.X(20), map.Y(51), map.Font(19), WHITE);
    if (hud.options.cardHints)
        DrawText("Drag a card to a match; right-click to cancel", map.X(20), map.Y(80), map.Font(15), LIGHTGRAY);
    if (game.Rules().GetPhase() == Phase::AWAIT_DECISION && game.Rules().CurrentPlayer() == Player::PLAYER && !game.Busy()) {
        DrawRectangle(map.X(TableHud::KoiKoiButton.x), map.Y(TableHud::KoiKoiButton.y),
                      map.W(TableHud::KoiKoiButton.width), map.H(TableHud::KoiKoiButton.height), Color{31, 75, 58, 245});
        DrawRectangle(map.X(TableHud::ShobuButton.x), map.Y(TableHud::ShobuButton.y),
                      map.W(TableHud::ShobuButton.width), map.H(TableHud::ShobuButton.height), Color{100, 50, 38, 245});
        DrawText("KOI-KOI", map.X(543), map.Y(349), map.Font(20), WHITE);
        DrawText("SHOBU", map.X(739), map.Y(349), map.Font(20), WHITE);
    }
    if (game.Rules().GetPhase() == Phase::ROUND_END) {
        DrawRectangle(map.X(TableHud::RoundEndButton.x), map.Y(TableHud::RoundEndButton.y),
                      map.W(TableHud::RoundEndButton.width), map.H(TableHud::RoundEndButton.height), Color{15, 20, 27, 235});
        DrawText(game.MatchFinished() ? "MATCH OVER" : "NEXT ROUND", map.X(615), map.Y(340), map.Font(20), WHITE);
        DrawText(game.MatchFinished() ? "Click to restart" : "Click to continue", map.X(604), map.Y(370), map.Font(16), LIGHTGRAY);
    }
    if (game.RoundIntroActive()) {
        // The logo and match cue share one opacity, so they leave together when play resumes.
        const float opacity = game.RoundIntroCueOpacity();
        const float beat = game.RoundIntroBulletProgress();
        if (opacity > 0.0f) {
            DrawRoundIntroLogo(map, logo, beat, opacity);
            if (beat >= 1.0f) {
                DrawRoundIntroCue(map, bodyFont, game.Round() == 1 ? "MATCH START" : "ROUND START", opacity);
            }
        }
    }

    // Separate radio and pause entry points; labels are temporary icon-asset placeholders.
    const bool liveRadio = hud.radio.enabled && !hud.radio.muted;
    DrawBox(map, TableHud::RadioMenuButton,
            hud.overlay == TableOverlay::RADIO ? Color{34, 62, 56, 240} : Color{15, 20, 27, 230},
            liveRadio ? Color{102, 186, 152, 240} : Color{95, 118, 138, 220}, 1.5f);
    DrawText("RADIO", map.X(TableHud::RadioMenuButton.x + 10.0f),
             map.Y(TableHud::RadioMenuButton.y + 19.0f), map.Font(14), WHITE);
    DrawBox(map, TableHud::PauseButton,
            hud.overlay == TableOverlay::PAUSE ? Color{34, 62, 56, 240} : Color{15, 20, 27, 230},
            Color{95, 118, 138, 220}, 1.5f);
    DrawText("PAUSE", map.X(TableHud::PauseButton.x + 9.0f),
             map.Y(TableHud::PauseButton.y + 19.0f), map.Font(14), WHITE);

    if (hud.overlay == TableOverlay::RADIO) {
        DrawPauseRadioMenu(map, hud.radio, {logo, headingFont, bodyFont});
    }
}

} // namespace Ui
