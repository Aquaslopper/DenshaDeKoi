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

constexpr Color HudNavy{8, 31, 122, 255};
constexpr Color HudNavyDeep{4, 13, 46, 255};
constexpr Color HudSilver{196, 207, 227, 255};
constexpr Color HudYellow{255, 213, 25, 255}; // Pause-menu focus yellow.
constexpr Color HudOrange{242, 103, 25, 255};
constexpr Color HudGreen{0, 111, 67, 255};
constexpr Color HudPaper{244, 248, 255, 255};
constexpr Color HudDisabled{125, 137, 157, 255};

enum class HudKana : std::size_t {
    KoiKoi, Shobu, NextRound, MatchOver, DealerPick, CardPick, Radio, Pause, Home
};

// Transparent 192x432 atlas in nine 48px rows. Each source width matches the
// baked Japanese phrase; no runtime CJK font or change to assets/fonts is needed.
constexpr std::array<Rectangle, 9> HudKanaSources{{
    {0, 0, 128, 48}, {0, 48, 128, 48}, {0, 96, 192, 48},
    {0, 144, 128, 48}, {0, 192, 128, 48}, {0, 240, 192, 48},
    {0, 288, 96, 48}, {0, 336, 128, 48}, {0, 384, 96, 48}
}};

// HUD textures live until CloseWindow releases the graphics context. Load them
// once on the render thread, including the new icon and kana art.
struct MatchHudTextures {
    Texture2D panel{};
    Texture2D atlas{};
    Texture2D radioIcon{};
    Texture2D pauseIcon{};
    Texture2D homeIcon{};
    Texture2D kana{};
    MatchHudTextures() {
        const std::filesystem::path dir(GetApplicationDirectory());
        const auto loadOption = [&](const char* name) {
            return LoadTexture((dir / "options" / "match-hud" / name).string().c_str());
        };
        panel = loadOption("MatchUI.png");
        atlas = LoadTexture((dir / "cards_atlas.png").string().c_str());
        radioIcon = loadOption("radio-icon.png");
        pauseIcon = loadOption("pause-icon.png");
        homeIcon = loadOption("home-icon.png");
        kana = loadOption("hud-kana.png");
        if (panel.id) SetTextureFilter(panel, TEXTURE_FILTER_BILINEAR);
        if (atlas.id) SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
        for (const Texture2D texture : {radioIcon, pauseIcon, homeIcon, kana})
            if (texture.id) SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    }
};

MatchHudTextures& GetMatchHudTextures() {
    static MatchHudTextures textures;
    return textures;
}

struct ButtonVisual {
    bool hover = false;
    bool pressed = false;
    bool active = false;
    bool disabled = false;
};

bool PointerOver(TableLayout::Point2 pointer, TableLayout::Rect rect) {
    return pointer.x >= rect.x && pointer.x <= rect.x + rect.width &&
           pointer.y >= rect.y && pointer.y <= rect.y + rect.height;
}

ButtonVisual ButtonAt(const TableHudState& hud, TableLayout::Rect rect, bool active, bool disabled) {
    const bool hover = PointerOver(hud.pointer, rect);
    return {hover, hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT), active, disabled};
}

Color ButtonTint(ButtonVisual state) {
    if (state.disabled) return HudDisabled;
    if (state.hover || state.pressed || state.active) return HudYellow;
    return HudPaper;
}

void DrawTriColourStripe(const Viewport& map, TableLayout::Rect rect, bool disabled) {
    const int x = map.X(rect.x + 3.0f);
    const int y = map.Y(rect.y + 3.0f);
    const int width = map.W(rect.width - 6.0f);
    const int height = map.H(std::min(2.0f, rect.height * 0.04f));
    const int first = width / 3;
    const int second = width / 3;
    const Color orange = disabled ? HudDisabled : HudOrange;
    const Color white = disabled ? HudDisabled : HudPaper;
    const Color green = disabled ? HudDisabled : HudGreen;
    DrawRectangle(x, y, first, height, orange);
    DrawRectangle(x + first, y, second, height, white);
    DrawRectangle(x + first + second, y, width - first - second, height, green);
}

void DrawRaisedPlate(const Viewport& map, TableLayout::Rect rect, ButtonVisual state,
                     bool triColour = true) {
    const Color fill = state.disabled ? Color{31, 39, 55, 255} :
                       state.pressed ? HudNavyDeep :
                       state.hover ? Color{17, 45, 142, 255} :
                       state.active ? Color{7, 26, 100, 255} : HudNavy;
    const Color border = state.disabled ? HudDisabled :
                         (state.hover || state.pressed) ? HudYellow : HudSilver;
    const Color bevel = state.disabled ? Color{77, 89, 109, 255} : HudPaper;
    const float inset = state.pressed ? 1.0f : 0.0f;

    DrawRectangle(map.X(rect.x + 3.0f), map.Y(rect.y + 4.0f),
                  map.W(rect.width), map.H(rect.height), Color{3, 7, 16, 155});
    DrawRectangle(map.X(rect.x + inset), map.Y(rect.y + inset),
                  map.W(rect.width - inset * 2.0f), map.H(rect.height - inset * 2.0f), fill);
    DrawRectangleLinesEx(map.Rec(rect), std::max(1.0f, 2.0f * map.scale), border);
    DrawRectangle(map.X(rect.x + 2.0f), map.Y(rect.y + 2.0f),
                  map.W(rect.width - 4.0f), map.H(1.5f), bevel);
    DrawRectangle(map.X(rect.x + 2.0f), map.Y(rect.y + rect.height - 4.0f),
                  map.W(rect.width - 4.0f), map.H(2.0f), HudNavyDeep);
    DrawRectangle(map.X(rect.x + 2.0f), map.Y(rect.y + 2.0f),
                  map.W(1.5f), map.H(rect.height - 4.0f), bevel);
    DrawRectangle(map.X(rect.x + rect.width - 4.0f), map.Y(rect.y + 2.0f),
                  map.W(2.0f), map.H(rect.height - 4.0f), HudNavyDeep);
    if (triColour) DrawTriColourStripe(map, rect, state.disabled);
    if (state.active || state.hover || state.pressed) {
        DrawRectangle(map.X(rect.x + 4.0f), map.Y(rect.y + 8.0f),
                      map.W(3.0f), map.H(std::max(1.0f, rect.height - 16.0f)), HudYellow);
    }
}

void DrawKana(const Viewport& map, Texture2D atlas, HudKana label,
              TableLayout::Rect bounds, float maxHeight, Color tint) {
    if (!atlas.id) return;
    const Rectangle source = HudKanaSources[static_cast<std::size_t>(label)];
    float height = std::min(maxHeight, bounds.height);
    float width = height * source.width / source.height;
    if (width > bounds.width) {
        width = bounds.width;
        height = width * source.height / source.width;
    }
    const TableLayout::Rect destination{bounds.x + (bounds.width - width) * 0.5f,
                                        bounds.y + (bounds.height - height) * 0.5f,
                                        width, height};
    DrawTexturePro(atlas, source, map.Rec(destination), {0, 0}, 0.0f, tint);
}

void DrawCenteredText(const Viewport& map, Font font, const char* text,
                      float centerX, float top, float size, Color color) {
    if (!font.texture.id) font = GetFontDefault();
    const float scaledSize = size * map.scale;
    const float spacing = 0.5f * map.scale;
    const Vector2 measured = MeasureTextEx(font, text, scaledSize, spacing);
    const Vector2 position{map.box.x + centerX * map.scale - measured.x * 0.5f,
                           map.box.y + top * map.scale};
    DrawTextEx(font, text, position, scaledSize, spacing, color);
}

void DrawHudEntryButton(const Viewport& map, Font font, const TableHudState& hud,
                        Texture2D icon, HudKana kana, const char* english,
                        TableLayout::Rect rect, bool active) {
    const ButtonVisual state = ButtonAt(hud, rect, active, false);
    DrawRaisedPlate(map, rect, state);
    const Color tint = ButtonTint(state);
    if (icon.id) {
        const Rectangle source{0, 0, static_cast<float>(icon.width), static_cast<float>(icon.height)};
        const TableLayout::Rect destination{rect.x + (rect.width - 18.0f) * 0.5f,
                                            rect.y + 8.0f, 18.0f, 18.0f};
        DrawTexturePro(icon, source, map.Rec(destination), {0, 0}, 0.0f, tint);
    }
    DrawKana(map, GetMatchHudTextures().kana, kana,
             {rect.x + 5.0f, rect.y + 26.0f, rect.width - 10.0f, 15.0f}, 15.0f, tint);
    DrawCenteredText(map, font, english, rect.x + rect.width * 0.5f,
                     rect.y + 43.0f, 10.0f, tint);
}

void DrawRoundCounter(const Viewport& map, Font font, const TableGame& game) {
    constexpr TableLayout::Rect plate{574.0f, 9.0f, 212.0f, 48.0f};
    DrawRaisedPlate(map, plate, {});
    if (!font.texture.id) font = GetFontDefault();

    const std::string round = std::to_string(game.Round());
    const std::string total = std::to_string(game.TotalRounds());
    const std::string whole = "ROUND " + round + " / " + total;
    const float size = 22.0f * map.scale;
    const float spacing = 0.5f * map.scale;
    const float width = MeasureTextEx(font, whole.c_str(), size, spacing).x;
    float x = map.box.x + 680.0f * map.scale - width * 0.5f;
    const float y = map.box.y + 22.0f * map.scale;
    const auto draw = [&](const std::string& text, Color color) {
        DrawTextEx(font, text.c_str(), {x, y}, size, spacing, color);
        x += MeasureTextEx(font, text.c_str(), size, spacing).x;
    };
    draw("ROUND ", HudPaper);
    draw(round, HudYellow);
    draw(" / ", HudPaper);
    draw(total, HudYellow);
}

void DrawActionButton(const Viewport& map, Font font, Texture2D kanaAtlas,
                      const TableHudState& hud, TableLayout::Rect rect,
                      HudKana kana, const char* english, bool disabled) {
    const ButtonVisual state = ButtonAt(hud, rect, false, disabled);
    const Color tint = ButtonTint(state);
    DrawRaisedPlate(map, rect, state);
    DrawKana(map, kanaAtlas, kana,
             {rect.x + 12.0f, rect.y + 7.0f, rect.width - 24.0f, 18.0f}, 16.0f, tint);
    DrawCenteredText(map, font, english, rect.x + rect.width * 0.5f,
                     rect.y + 26.0f, 16.0f, tint);
}

void DrawRoundEndButton(const Viewport& map, Font font, Texture2D kanaAtlas,
                        const TableHudState& hud, const TableGame& game, bool disabled) {
    const TableLayout::Rect rect = TableHud::RoundEndButton;
    const ButtonVisual state = ButtonAt(hud, rect, false, disabled);
    const Color tint = ButtonTint(state);
    DrawRaisedPlate(map, rect, state);
    DrawKana(map, kanaAtlas, game.MatchFinished() ? HudKana::MatchOver : HudKana::NextRound,
             {rect.x + 16.0f, rect.y + 7.0f, rect.width - 32.0f, 18.0f}, 16.0f, tint);
    DrawCenteredText(map, font, game.MatchFinished() ? "MATCH OVER" : "NEXT ROUND",
                     rect.x + rect.width * 0.5f, rect.y + 27.0f, 19.0f, tint);
    DrawCenteredText(map, font, game.MatchFinished() ? "CLICK TO RESTART" : "CLICK TO CONTINUE",
                     rect.x + rect.width * 0.5f, rect.y + 51.0f, 11.0f, tint);
}

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
    const auto& textures = GetMatchHudTextures();
    constexpr TableLayout::Rect prompt{400.0f, 96.0f, 560.0f, 48.0f};
    DrawRaisedPlate(map, prompt, {false, false, true, false});
    DrawKana(map, textures.kana, HudKana::DealerPick,
             {prompt.x + 12.0f, prompt.y + 5.0f, prompt.width - 24.0f, 17.0f},
             16.0f, HudYellow);
    DrawCenteredText(map, matchBodyFont, "PICK A CARD TO DETERMINE DEALER",
                     680.0f, prompt.y + 26.0f, 16.0f, HudPaper);

    const int choice = game.DealerChoice();
    if (choice < 0) {
        const auto drawPickTag = [&](TableLayout::Rect rect) {
            DrawRaisedPlate(map, rect, {false, false, true, false});
            DrawKana(map, textures.kana, HudKana::CardPick,
                     {rect.x + 5.0f, rect.y + 2.0f, rect.width - 10.0f, 14.0f},
                     13.0f, HudYellow);
            DrawCenteredText(map, matchBodyFont, "PICK CARD", rect.x + rect.width * 0.5f,
                             rect.y + 19.0f, 10.0f, HudPaper);
        };
        drawPickTag({490.0f, 510.0f, 155.0f, 32.0f});
        drawPickTag({715.0f, 510.0f, 175.0f, 32.0f});
        return;
    }
    if (game.OpeningTimer() >= 0.9f) {
        DrawMatchText(map, matchBodyFont, choice == 0 ? "YOUR CARD" : "CPU CARD", 520, 520, 18, HudPaper);
        DrawMatchText(map, matchBodyFont, choice == 1 ? "YOUR CARD" : "CPU CARD", 775, 520, 18, HudPaper);
        const char* result = game.OpeningDealer() == Player::PLAYER ? "YOU ARE DEALER" : "CPU IS DEALER";
        DrawMatchText(map, matchBodyFont, result, 570, 575, 30, HudYellow);
    }
}

void DrawHud(const Viewport& map, const TableGame& game, const TableHudState& hud,
             Texture2D logo, Font bodyFont) {
    MatchHudTextures& matchHud = GetMatchHudTextures();
    DrawCapturePanel(map, game, Player::PLAYER, matchHud);
    DrawCapturePanel(map, game, Player::OPPONENT, matchHud);
    matchBodyFont = bodyFont;
    DrawRoundCounter(map, bodyFont, game);

    if (game.Rules().GetPhase() == Phase::AWAIT_DECISION &&
        game.Rules().CurrentPlayer() == Player::PLAYER) {
        const bool disabled = game.Busy() || game.MatchFinished();
        DrawActionButton(map, bodyFont, matchHud.kana, hud, TableHud::KoiKoiButton,
                         HudKana::KoiKoi, "KOI-KOI", disabled);
        DrawActionButton(map, bodyFont, matchHud.kana, hud, TableHud::ShobuButton,
                         HudKana::Shobu, "SHOBU", disabled);
    }
    if (game.Rules().GetPhase() == Phase::ROUND_END && hud.matchResult == nullptr) {
        DrawRoundEndButton(map, bodyFont, matchHud.kana, hud, game, game.Busy());
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

    const bool liveRadio = hud.radio.enabled && !hud.radio.muted;
    DrawHudEntryButton(map, bodyFont, hud, matchHud.radioIcon, HudKana::Radio, "RADIO",
                       TableHud::RadioMenuButton,
                       liveRadio || hud.overlay == TableOverlay::RADIO);
    DrawHudEntryButton(map, bodyFont, hud, matchHud.pauseIcon, HudKana::Pause, "PAUSE",
                       TableHud::PauseButton, hud.overlay == TableOverlay::PAUSE);
    // A completed match makes the return-to-title route the primary exit.
    DrawHudEntryButton(map, bodyFont, hud, matchHud.homeIcon, HudKana::Home, "HOME",
                       TableHud::HomeButton, game.MatchFinished());
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
