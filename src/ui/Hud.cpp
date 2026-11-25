#include "DeckRegister.hpp" // Parse card enums before Raylib defines its BLUE color macro.
#include "TableGame.hpp"
#include "AtlasLayout.hpp"
#include "ui/Hud.hpp"

#include "ui/MatchCaptureLanes.hpp"
#include "ui/LoadingScreen.hpp"
#include "ui/MatchResultMenu.hpp"
#include "ui/PauseMenu.hpp"
#include "ui/PauseRadioMenu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <string>

namespace Ui {
namespace {

// The scene calls DrawHud immediately before the dealer prompt, using the
// same loaded in-game body font for both without changing the scene contract.
Font matchBodyFont{};

void DrawMatchText(const Viewport& map, Font font, const char* text,
                   float x, float y, float size, Color color) {
    if (!font.texture.id) font = GetFontDefault();
    DrawTextEx(font, text, {map.box.x + x * map.scale, map.box.y + y * map.scale},
               size * map.scale, 0.5f * map.scale, color);
}

void DrawBox(const Viewport& map, TableLayout::Rect rect, Color fill, Color border,
             float borderPx = 1.5f) {
    DrawRectangle(map.X(rect.x), map.Y(rect.y), map.W(rect.width), map.H(rect.height), fill);
    DrawRectangleLinesEx(map.Rec(rect), std::max(1.0f, borderPx * map.scale), border);
}

// The HUD owns these textures for the lifetime of the Raylib window. They are
// loaded once on the render thread; CloseWindow releases the graphics context.
struct MatchHudTextures {
    Texture2D panel{};
    Texture2D atlas{};
    MatchHudTextures() {
        const std::filesystem::path dir(GetApplicationDirectory());
        panel = LoadTexture((dir / "options" / "match-hud" / "MatchUI.png").string().c_str());
        atlas = LoadTexture((dir / "cards_atlas.png").string().c_str());
        if (panel.id) SetTextureFilter(panel, TEXTURE_FILTER_BILINEAR);
        if (atlas.id) SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
    }
};

void DrawCapturePanel(const Viewport& map, const TableGame& game, Player player,
                      const MatchHudTextures& textures) {
    const bool you = player == Player::PLAYER;
    // MatchUI is 1334x81: two separate panels with transparent space between.
    const float panelX = you ? 12.0f : 950.0f;
    if (textures.panel.id) {
        const Rectangle source = you ? Rectangle{0, 0, 397, 81} : Rectangle{936, 0, 398, 81};
        DrawTexturePro(textures.panel, source, map.Rec({panelX, 10, 398, 81}), {0, 0}, 0, WHITE);
    }
    if (!textures.atlas.id) return;
    const auto lanes = MatchCaptureLanes(game.Rules().Captured(player));
    // Xeno Koi's B/A/R/P lane order, folded row-major into the 2x2 grid.
    // YOU's label is at the left and CPU's at the right.
    constexpr float cardHeight = 28.0f;
    constexpr float cardWidth = cardHeight * 179.0f / 264.0f;
    for (std::size_t lane = 0; lane < lanes.size(); ++lane) {
        const auto& cards = lanes[lane];
        const auto layout = MatchCaptureLayout(you, lane % 2, cards.size(), cardWidth);
        const float y = MatchCaptureY(lane / 2, cardHeight);
        for (std::size_t i = 0; i < cards.size(); ++i) {
            const auto& card = DeckRegister::At(cards[i]);
            const auto src = AtlasLayout::GetCardSourceRect(static_cast<float>(textures.atlas.width),
                static_cast<float>(textures.atlas.height), card.month, card.cardIndex);
            DrawTexturePro(textures.atlas, {src.x, src.y, src.width, src.height},
                           map.Rec({layout.x + static_cast<float>(i) * layout.step, y, cardWidth, cardHeight}),
                           {0, 0}, 0, WHITE);
        }
    }
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
    DrawTextEx(font, text, {origin.x + shadow, origin.y + shadow}, size, spacing,
               Color{0, 0, 0, static_cast<unsigned char>(alpha * 3 / 4)});
    DrawTextEx(font, text, origin, size, spacing, Color{255, 255, 255, alpha});
}

} // namespace

void DrawBulletTimeDim(const TableGame& game, int canvasWidth, int canvasHeight) {
    if (const float beat = game.RoundIntroBulletProgress(); beat < 1.0f) {
        DrawRectangle(0, 0, canvasWidth, canvasHeight,
                      Color{0, 0, 0, static_cast<unsigned char>(std::lround(110.0f * (1.0f - beat)))});
    }
}

// Read-only assistance: use visible hand cards and the rule engine's pending
// choices. It never selects a card or participates in hit testing or scoring.
void DrawCardHints(const TableGame& game, const std::vector<TableCard>& cards, TableLayout::Point2 pointer) {
    const auto& rules = game.Rules();
    if (game.Busy() || rules.CurrentPlayer() != Player::PLAYER) return;
    const auto phase = rules.GetPhase();
    int month = 0;
    if (phase == Phase::AWAIT_HAND_PLAY) {
        const auto plan = TableLayout::Unproject(pointer);
        const auto& hand = rules.Hand(Player::PLAYER);
        for (auto it = cards.rbegin(); it != cards.rend(); ++it) {
            const auto r = it->rect;
            if (plan.x >= r.x && plan.x <= r.x + r.width && plan.y >= r.y && plan.y <= r.y + r.height &&
                std::find(hand.begin(), hand.end(), it->id) != hand.end()) {
                month = DeckRegister::At(it->id).month;
                break;
            }
        }
        if (!month) return;
    } else if (phase != Phase::AWAIT_DRAW_CHOICE) return;
    for (int id : (month ? rules.Field() : rules.Choices())) {
        if (month && DeckRegister::At(id).month != month) continue;
        const TableLayout::Rect r = game.FieldRect(id);
        const auto corner = [](float x, float y) {
            const auto p = TableLayout::Project(
                TableLayout::OnPlane(x, y, TableLayout::CardElevation + TableLayout::CardThickness));
            return Vector2{p.x, p.y};
        };
        const float l = r.x - 2, t = r.y - 2, b = r.y + r.height + 2, right = r.x + r.width + 2;
        const std::array points{corner(l, t), corner(right, t), corner(right, b), corner(l, b)};
        for (std::size_t i = 0; i < points.size(); ++i)
            DrawLineEx(points[i], points[(i + 1) % points.size()], 2.0f, Color{255, 218, 53, 235});
    }
}

void DrawDealerSelection(const Viewport& map, const TableGame& game) {
    DrawMatchText(map, matchBodyFont, "PICK A CARD TO DETERMINE DEALER", 405, 100, 28, WHITE);
    const int choice = game.DealerChoice();
    if (choice < 0) {
        DrawMatchText(map, matchBodyFont, "CLICK TO PICK", 505, 520, 18, LIGHTGRAY);
        DrawMatchText(map, matchBodyFont, "CLICK TO PICK", 760, 520, 18, LIGHTGRAY);
        return;
    }
    if (game.OpeningTimer() >= 0.9f) {
        DrawMatchText(map, matchBodyFont, choice == 0 ? "YOUR CARD" : "CPU CARD", 520, 520, 18, WHITE);
        DrawMatchText(map, matchBodyFont, choice == 1 ? "YOUR CARD" : "CPU CARD", 775, 520, 18, WHITE);
        const char* result = game.OpeningDealer() == Player::PLAYER ? "YOU ARE DEALER" : "CPU IS DEALER";
        DrawMatchText(map, matchBodyFont, result, 570, 575, 30, WHITE);
    }
}

void DrawHud(const Viewport& map, const TableGame& game, const TableHudState& hud,
             Texture2D logo, Font bodyFont) {
    static const MatchHudTextures matchHud;
    DrawCapturePanel(map, game, Player::PLAYER, matchHud);
    DrawCapturePanel(map, game, Player::OPPONENT, matchHud);
    matchBodyFont = bodyFont;
    const std::string roundLabel = "ROUND " + std::to_string(game.Round()) + " / " +
                                   std::to_string(game.TotalRounds());
    const Font roundFont = bodyFont.texture.id ? bodyFont : GetFontDefault();
    const float roundSize = 24.0f * map.scale;
    const float spacing = 0.5f * map.scale;
    const Vector2 roundWidth = MeasureTextEx(roundFont, roundLabel.c_str(), roundSize, spacing);
    const Vector2 roundPos{map.box.x + 680.0f * map.scale - roundWidth.x * 0.5f,
                           map.box.y + 20.0f * map.scale};
    DrawTextEx(roundFont, roundLabel.c_str(), {roundPos.x + map.scale, roundPos.y + map.scale},
               roundSize, spacing, Color{0, 0, 0, 200});
    DrawTextEx(roundFont, roundLabel.c_str(), roundPos, roundSize, spacing, WHITE);
    if (game.Rules().GetPhase() == Phase::AWAIT_DECISION && game.Rules().CurrentPlayer() == Player::PLAYER && !game.Busy()) {
        DrawRectangle(map.X(TableHud::KoiKoiButton.x), map.Y(TableHud::KoiKoiButton.y),
                      map.W(TableHud::KoiKoiButton.width), map.H(TableHud::KoiKoiButton.height), Color{31, 75, 58, 245});
        DrawRectangle(map.X(TableHud::ShobuButton.x), map.Y(TableHud::ShobuButton.y),
                      map.W(TableHud::ShobuButton.width), map.H(TableHud::ShobuButton.height), Color{100, 50, 38, 245});
        DrawMatchText(map, bodyFont, "KOI-KOI", 543, 349, 20, WHITE);
        DrawMatchText(map, bodyFont, "SHOBU", 739, 349, 20, WHITE);
    }
    if (game.Rules().GetPhase() == Phase::ROUND_END && !game.Busy() && hud.matchResult == nullptr) {
        DrawRectangle(map.X(TableHud::RoundEndButton.x), map.Y(TableHud::RoundEndButton.y),
                      map.W(TableHud::RoundEndButton.width), map.H(TableHud::RoundEndButton.height), Color{15, 20, 27, 235});
        DrawMatchText(map, bodyFont, game.MatchFinished() ? "MATCH OVER" : "NEXT ROUND", 615, 340, 20, WHITE);
        DrawMatchText(map, bodyFont, game.MatchFinished() ? "Click to restart" : "Click to continue", 604, 370, 16, LIGHTGRAY);
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
    DrawMatchText(map, bodyFont, "RADIO", TableHud::RadioMenuButton.x + 10.0f, TableHud::RadioMenuButton.y + 19.0f, 14, WHITE);
    DrawBox(map, TableHud::PauseButton,
            hud.overlay == TableOverlay::PAUSE ? Color{34, 62, 56, 240} : Color{15, 20, 27, 230},
            Color{95, 118, 138, 220}, 1.5f);
    DrawMatchText(map, bodyFont, "PAUSE", TableHud::PauseButton.x + 9.0f, TableHud::PauseButton.y + 19.0f, 14, WHITE);
    DrawBox(map, TableHud::HomeButton, Color{15, 20, 27, 230},
            Color{95, 118, 138, 220}, 1.5f);
    DrawMatchText(map, bodyFont, "HOME", TableHud::HomeButton.x + 16.0f, TableHud::HomeButton.y + 19.0f, 14, WHITE);
}

void DrawScreenOverlay(const Viewport& viewport, const TableHudState& hud, Resolution display,
                       Texture2D logo, Texture2D optionsPanel, Texture2D optionsStripe,
                       Font headingFont, Font bodyFont) {
    if (hud.matchResult && hud.matchResult->IsOpen()) {
        hud.matchResult->Draw(viewport, hud.pointer, headingFont, bodyFont);
    }
    if (hud.overlay == TableOverlay::RADIO) {
        DrawPauseRadioMenu(viewport, hud.radio, {logo, headingFont, bodyFont});
    } else if (hud.overlay == TableOverlay::PAUSE) {
        DrawPauseMenu(viewport, hud.options, display, hud.focusedOption,
                      {optionsPanel, optionsStripe, headingFont, bodyFont});
    }
    if (activeLoadingScreen) activeLoadingScreen->Draw(viewport);
}

} // namespace Ui
