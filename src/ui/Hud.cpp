#include "DeckRegister.hpp" // Parse card enums before Raylib defines its BLUE color macro.
#include "TableGame.hpp"
#include "ui/Hud.hpp"

#include "ui/LoadingScreen.hpp"
#include "ui/MatchResultMenu.hpp"
#include "ui/PauseMenu.hpp"
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
    DrawTextEx(font, text, {origin.x + shadow, origin.y + shadow}, size, spacing,
               Color{0, 0, 0, static_cast<unsigned char>(alpha * 3 / 4)});
    DrawTextEx(font, text, origin, size, spacing, Color{255, 255, 255, alpha});
}

// TableScene owns the real game fonts; DrawHud runs just before the dealer
// prompt, so cache its body font for that prompt without changing protected
// call sites or loading a second copy of the font.
Font activeMatchBodyFont{};

constexpr Color YouAccent{32, 158, 214, 255};
constexpr Color CpuAccent{226, 62, 58, 255};
constexpr Color YakuPanelFill{12, 19, 28, 214};
constexpr Color YakuSlotFill{28, 36, 48, 238};
constexpr Color YakuText{234, 242, 248, 255};
constexpr Color YakuTextDim{168, 187, 202, 255};

[[nodiscard]] Color WithAlpha(Color color, unsigned char alpha) {
    return {color.r, color.g, color.b, alpha};
}

[[nodiscard]] Vector2 MapPoint(const Viewport& map, float x, float y) {
    return {static_cast<float>(map.X(x)), static_cast<float>(map.Y(y))};
}

[[nodiscard]] Color YakuAccent(const std::string& name) {
    const auto startsWith = [&name](const char* prefix) { return name.rfind(prefix, 0) == 0; };
    if (startsWith("Goko") || startsWith("Shiko") || startsWith("Ame-shiko") || startsWith("Sanko"))
        return {255, 207, 92, 255};
    if (startsWith("Hanami") || startsWith("Tsukimi")) return {255, 148, 186, 255};
    if (startsWith("Ino-shika-cho")) return {126, 214, 150, 255};
    if (startsWith("Akatan")) return {255, 118, 110, 255};
    if (startsWith("Aotan")) return {112, 184, 255, 255};
    if (startsWith("Tane")) return {118, 210, 164, 255};
    if (startsWith("Tanzaku")) return {255, 168, 118, 255};
    if (startsWith("Kasu")) return {190, 199, 212, 255};
    return {210, 220, 230, 255};
}

// Ellipsize to the cell rather than shrinking the yaku name into illegibility.
[[nodiscard]] std::string FitText(Font font, const std::string& text, float size,
                                  float spacing, float maxWidth) {
    if (MeasureTextEx(font, text.c_str(), size, spacing).x <= maxWidth) return text;
    std::string clipped = text;
    while (!clipped.empty() && MeasureTextEx(font, (clipped + "...").c_str(), size, spacing).x > maxWidth)
        clipped.pop_back();
    return clipped + "...";
}

void DrawFittedText(const Viewport& map, Font font, const std::string& text,
                    TableLayout::Rect rect, float size, Color color, bool centered = false) {
    const Rectangle box = map.Rec(rect);
    const float fontSize = size * map.scale;
    const float spacing = 0.4f * map.scale;
    const std::string value = FitText(font, text, fontSize, spacing, box.width);
    const Vector2 measured = MeasureTextEx(font, value.c_str(), fontSize, spacing);
    const Vector2 position{centered ? box.x + (box.width - measured.x) * 0.5f : box.x,
                           box.y + (box.height - fontSize) * 0.5f};
    DrawTextEx(font, value.c_str(), position, fontSize, spacing, color);
}

// The centre of the strip sits over an unpredictable photo backdrop, so keep
// the text legible with a soft offset instead of another grey backing panel.
void DrawShadowedText(const Viewport& map, Font font, const std::string& text,
                      TableLayout::Rect rect, float size, Color color) {
    DrawFittedText(map, font, text,
                   {rect.x + 1.5f, rect.y + 1.5f, rect.width, rect.height},
                   size, Color{0, 0, 0, 185}, true);
    DrawFittedText(map, font, text, rect, size, color, true);
}

void DrawYakuSlotCard(const Viewport& map, Font font, TableLayout::Rect rect,
                      const HudYakuSlot& slot, Color accent) {
    const Rectangle box = map.Rec(rect);
    DrawRectangleRounded(box, 0.14f, 6, YakuSlotFill);
    DrawRectangleLinesEx(box, std::max(1.0f, map.scale), WithAlpha(accent, 150));
    DrawRectangle(map.X(rect.x), map.Y(rect.y), map.W(3.0f), map.H(rect.height), accent);

    if (slot.name.empty()) {
        DrawFittedText(map, font, "--", rect, 14.0f, YakuTextDim, true);
        return;
    }

    // Tiny hanafuda chip: month number on the yaku's family colour.
    const TableLayout::Rect chip{rect.x + 7.0f, rect.y + 6.0f, 20.0f, rect.height - 12.0f};
    const Color chipColor = YakuAccent(slot.name);
    DrawRectangleRounded(map.Rec(chip), 0.22f, 5, chipColor);
    DrawFittedText(map, font, slot.month > 0 ? std::to_string(slot.month) : "-", chip,
                   13.0f, Color{12, 20, 30, 255}, true);

    const TableLayout::Rect nameRect{rect.x + 33.0f, rect.y + 3.0f,
                                     rect.width - 67.0f, 20.0f};
    const TableLayout::Rect pointsRect{rect.x + rect.width - 31.0f, rect.y + 3.0f,
                                       27.0f, 20.0f};
    DrawFittedText(map, font, slot.name, nameRect, 12.0f, YakuText);
    DrawFittedText(map, font, std::to_string(slot.points), pointsRect, 13.0f, accent, true);

    const std::string detail = slot.overflow > 0
        ? "+" + std::to_string(slot.overflow) + " MORE"
        : "PTS";
    const TableLayout::Rect detailRect{rect.x + 33.0f, rect.y + 23.0f,
                                       rect.width - 38.0f, rect.height - 25.0f};
    DrawFittedText(map, font, detail, detailRect, 10.0f, YakuTextDim);
}

void DrawYakuPanel(const Viewport& map, Font font, bool cpu,
                   const std::array<HudYakuSlot, TableHud::YakuSlotCount>& slots,
                   int base, bool active) {
    const TableLayout::Rect panel = cpu ? TableHud::CpuYakuPanel : TableHud::YouYakuPanel;
    const TableLayout::Rect badge = cpu ? TableHud::CpuYakuBadge : TableHud::YouYakuBadge;
    const Color accent = cpu ? CpuAccent : YouAccent;
    const Rectangle panelBox = map.Rec(panel);
    DrawRectangleRounded(panelBox, 0.09f, 8, YakuPanelFill);
    DrawRectangleLinesEx(panelBox, std::max(1.0f, 1.5f * map.scale), WithAlpha(accent, 210));

    // The two dividers make the four yaku corners read as one 2x2 grid.
    const float midX = panel.x + panel.width * 0.5f;
    const float midY = panel.y + panel.height * 0.5f;
    DrawLineEx(MapPoint(map, midX, panel.y + 3.0f), MapPoint(map, midX, panel.y + panel.height - 3.0f),
               1.5f * map.scale, WithAlpha(accent, 170));
    DrawLineEx(MapPoint(map, panel.x + 3.0f, midY), MapPoint(map, panel.x + panel.width - 3.0f, midY),
               1.5f * map.scale, WithAlpha(accent, 170));

    for (std::size_t i = 0; i < TableHud::YakuSlotCount; ++i)
        DrawYakuSlotCard(map, font, TableHud::YakuSlotRect(cpu, i), slots[i], accent);

    const Rectangle badgeBox = map.Rec(badge);
    DrawRectangleRounded(badgeBox, 0.13f, 8, WithAlpha(accent, active ? 255 : 216));
    DrawRectangleLinesEx(badgeBox, std::max(1.0f, map.scale), WithAlpha(YakuPanelFill, 170));
    DrawFittedText(map, font, cpu ? "CPU" : "YOU",
                   {badge.x, badge.y + 11.0f, badge.width, 27.0f}, 21.0f,
                   Color{9, 17, 26, 255}, true);
    // "DRIVE" stands in for 運転: the game fonts are Latin-only by design.
    DrawFittedText(map, font, "DRIVE", {badge.x, badge.y + 41.0f, badge.width, 17.0f}, 12.0f,
                   Color{14, 23, 33, 235}, true);
    DrawFittedText(map, font, std::to_string(base) + " PT",
                   {badge.x, badge.y + 68.0f, badge.width, 15.0f}, 11.0f,
                   Color{14, 23, 33, 235}, true);
}

void DrawMatchTopStrip(const Viewport& map, Font font, const TableGame& game, bool cardHints) {
    const auto& rules = game.Rules();
    const auto youCaptured = rules.Captured(Player::PLAYER);
    const auto cpuCaptured = rules.Captured(Player::OPPONENT);
    DrawYakuPanel(map, font, false, BuildHudYakuSlots(youCaptured),
                  ScoreYaku(youCaptured).base, rules.CurrentPlayer() == Player::PLAYER);
    DrawYakuPanel(map, font, true, BuildHudYakuSlots(cpuCaptured),
                  ScoreYaku(cpuCaptured).base, rules.CurrentPlayer() == Player::OPPONENT);

    // No grey backing: the score and status float in the open centre of the strip.
    const std::string score = "ROUND " + std::to_string(game.Round()) + " / " +
        std::to_string(game.TotalRounds()) + "    YOU " +
        std::to_string(game.Scores()[PlayerIndex(Player::PLAYER)]) + "  -  CPU " +
        std::to_string(game.Scores()[PlayerIndex(Player::OPPONENT)]);
    DrawShadowedText(map, font, score, {410.0f, 14.0f, 540.0f, 32.0f}, 25.0f, YakuText);
    DrawShadowedText(map, font, game.Status(), {410.0f, 47.0f, 540.0f, 26.0f}, 17.0f, YakuText);
    if (cardHints)
        DrawShadowedText(map, font, "Drag a card to a match; right-click to cancel",
                         {410.0f, 73.0f, 540.0f, 20.0f}, 13.0f, YakuTextDim);
}

} // namespace

std::array<HudYakuSlot, TableHud::YakuSlotCount> BuildHudYakuSlots(std::span<const CardId> captured) {
    std::array<HudYakuSlot, TableHud::YakuSlotCount> slots{};
    if (captured.empty()) return slots;
    const auto combos = MatchResultMenu::BuildCombosFromCaptures(captured);
    for (std::size_t i = 0; i < slots.size() && i < combos.size(); ++i) {
        slots[i].name = combos[i].name;
        slots[i].points = combos[i].points;
        if (!combos[i].cards.empty())
            slots[i].month = DeckRegister::At(combos[i].cards.front()).month;
    }
    if (combos.size() > slots.size())
        slots.back().overflow = static_cast<int>(combos.size() - slots.size());
    return slots;
}

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
    const Font font = activeMatchBodyFont.texture.id != 0 ? activeMatchBodyFont : GetFontDefault();
    DrawFittedText(map, font, "PICK A CARD TO DETERMINE DEALER",
                   {405.0f, 96.0f, 550.0f, 36.0f}, 28.0f, YakuText, true);
    const int choice = game.DealerChoice();
    if (choice < 0) {
        DrawFittedText(map, font, "CLICK TO PICK", {470.0f, 516.0f, 160.0f, 26.0f}, 18.0f, YakuTextDim, true);
        DrawFittedText(map, font, "CLICK TO PICK", {725.0f, 516.0f, 160.0f, 26.0f}, 18.0f, YakuTextDim, true);
        return;
    }
    if (game.OpeningTimer() >= 0.9f) {
        DrawFittedText(map, font, choice == 0 ? "YOUR CARD" : "CPU CARD",
                       {480.0f, 516.0f, 170.0f, 26.0f}, 18.0f, YakuText, true);
        DrawFittedText(map, font, choice == 1 ? "YOUR CARD" : "CPU CARD",
                       {735.0f, 516.0f, 170.0f, 26.0f}, 18.0f, YakuText, true);
        const char* result = game.OpeningDealer() == Player::PLAYER ? "YOU ARE DEALER" : "CPU IS DEALER";
        DrawFittedText(map, font, result, {515.0f, 570.0f, 330.0f, 36.0f}, 30.0f, YakuText, true);
    }
}

void DrawHud(const Viewport& map, const TableGame& game, const TableHudState& hud,
             Texture2D logo, Font bodyFont) {
    activeMatchBodyFont = bodyFont;
    DrawMatchTopStrip(map, bodyFont, game, hud.options.cardHints);
    if (game.Rules().GetPhase() == Phase::AWAIT_DECISION && game.Rules().CurrentPlayer() == Player::PLAYER && !game.Busy()) {
        const auto koi = map.Rec(TableHud::KoiKoiButton);
        const auto shobu = map.Rec(TableHud::ShobuButton);
        DrawRectangleRounded(koi, 0.18f, 7, Color{31, 98, 76, 244});
        DrawRectangleRounded(shobu, 0.18f, 7, Color{126, 58, 43, 244});
        DrawRectangleLinesEx(koi, std::max(1.0f, 1.5f * map.scale), Color{110, 224, 178, 235});
        DrawRectangleLinesEx(shobu, std::max(1.0f, 1.5f * map.scale), Color{255, 166, 126, 235});
        DrawFittedText(map, bodyFont, "KOI-KOI",
                       {TableHud::KoiKoiButton.x, TableHud::KoiKoiButton.y,
                        TableHud::KoiKoiButton.width, TableHud::KoiKoiButton.height},
                       21.0f, YakuText, true);
        DrawFittedText(map, bodyFont, "SHOBU",
                       {TableHud::ShobuButton.x, TableHud::ShobuButton.y,
                        TableHud::ShobuButton.width, TableHud::ShobuButton.height},
                       21.0f, YakuText, true);
    }
    if (game.Rules().GetPhase() == Phase::ROUND_END && !game.Busy() && hud.matchResult == nullptr) {
        // Keep the click target but drop its old grey slab: just a bright cue line.
        const auto& button = TableHud::RoundEndButton;
        const std::string heading = game.MatchFinished() ? "MATCH OVER" : "NEXT ROUND";
        const std::string sub = game.MatchFinished() ? "Click to restart" : "Click to continue";
        DrawFittedText(map, bodyFont, heading, {button.x, button.y, button.width, 36.0f}, 24.0f, YakuText, true);
        DrawFittedText(map, bodyFont, sub, {button.x, button.y + 36.0f, button.width, 24.0f}, 16.0f, YakuTextDim, true);
        DrawLineEx(MapPoint(map, button.x + 20.0f, button.y + button.height - 2.0f),
                   MapPoint(map, button.x + button.width - 20.0f, button.y + button.height - 2.0f),
                   2.0f * map.scale, YouAccent);
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

    // Radio, pause and home keep their established entry points and casings.
    const bool liveRadio = hud.radio.enabled && !hud.radio.muted;
    DrawBox(map, TableHud::RadioMenuButton,
            hud.overlay == TableOverlay::RADIO ? Color{34, 62, 56, 240} : Color{15, 20, 27, 230},
            liveRadio ? Color{102, 186, 152, 240} : Color{95, 118, 138, 220}, 1.5f);
    DrawFittedText(map, bodyFont, "RADIO", TableHud::RadioMenuButton, 14.0f, YakuText, true);
    DrawBox(map, TableHud::PauseButton,
            hud.overlay == TableOverlay::PAUSE ? Color{34, 62, 56, 240} : Color{15, 20, 27, 230},
            Color{95, 118, 138, 220}, 1.5f);
    DrawFittedText(map, bodyFont, "PAUSE", TableHud::PauseButton, 14.0f, YakuText, true);
    DrawBox(map, TableHud::HomeButton, Color{15, 20, 27, 230},
            Color{95, 118, 138, 220}, 1.5f);
    DrawFittedText(map, bodyFont, "HOME", TableHud::HomeButton, 14.0f, YakuText, true);
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
