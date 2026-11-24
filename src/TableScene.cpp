#include "DeckRegister.hpp" // Parse the card enums before raylib's color macros.
#include "TableScene.hpp"

#include "TableLayout.hpp"
#include "TableGame.hpp"
#include "ui/PauseRadioMenu.hpp"
#include "ui/PauseMenu.hpp"
#include "ui/UiViewport.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using TableLayout::Rect;

constexpr const char* MagnifyFragmentShader = R"glsl(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 focus;
uniform float zoom;
out vec4 finalColor;
void main() {
    vec2 sampleUv = focus + (fragTexCoord - focus) / zoom;
    finalColor = texture(texture0, clamp(sampleUv, vec2(0.0), vec2(1.0))) * fragColor;
}
)glsl";

struct CardTransform {
    float cx, cy, cz;
    float hx, hy, hz;
    float cosZ, sinZ, cosX, sinX, cosY, sinY;
};

CardTransform MakeCardTransform(const TableCard& card) {
    const auto p00 = TableLayout::OnPlane(card.rect.x, card.rect.y, 0.0f);
    const auto p11 = TableLayout::OnPlane(card.rect.x + card.rect.width,
                                          card.rect.y + card.rect.height, 0.0f);
    const float hx = 0.5f * (p11.x - p00.x);
    const float hz = 0.5f * (p11.z - p00.z);
    const float hy = 0.5f * card.thickness;
    const float rz = card.flipAngle + card.roll;
    const float rx = card.pitch;
    const float ry = card.yaw;
    const float cosZ = std::cos(rz), sinZ = std::sin(rz);
    const float cosX = std::cos(rx), sinX = std::sin(rx);
    const float cosY = std::cos(ry), sinY = std::sin(ry);
    const float extentY = std::abs(hx * sinZ * cosX) +
                          std::abs(hy * cosZ * cosX) +
                          std::abs(hz * sinX);
    return {0.5f * (p00.x + p11.x), card.elevation + extentY, 0.5f * (p00.z + p11.z),
            hx, hy, hz, cosZ, sinZ, cosX, sinX, cosY, sinY};
}

void LocalVertex(const CardTransform& t, float lx, float ly, float lz, float u, float v) {
    const float x1 = lx * t.cosZ - ly * t.sinZ;
    const float y1 = lx * t.sinZ + ly * t.cosZ;
    const float y2 = y1 * t.cosX - lz * t.sinX;
    const float z2 = y1 * t.sinX + lz * t.cosX;
    const float x3 = x1 * t.cosY + z2 * t.sinY;
    const float z3 = -x1 * t.sinY + z2 * t.cosY;
    rlTexCoord2f(u, v);
    rlVertex3f(t.cx + x3, t.cy + y2, t.cz + z3);
}

void EmitQuad(const CardTransform& t,
              float x0, float y0, float z0, float u0, float v0,
              float x1, float y1, float z1, float u1, float v1,
              float x2, float y2, float z2, float u2, float v2,
              float x3, float y3, float z3, float u3, float v3,
              Color color) {
    rlColor4ub(color.r, color.g, color.b, color.a);
    LocalVertex(t, x0, y0, z0, u0, v0);
    LocalVertex(t, x1, y1, z1, u1, v1);
    LocalVertex(t, x2, y2, z2, u2, v2);
    LocalVertex(t, x3, y3, z3, u3, v3);
}

void DrawShadows(const std::vector<TableCard>& cards) {
    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
    for (const auto& card : cards) {
        if (!card.shadow) continue;
        const auto p00 = TableLayout::OnPlane(card.rect.x, card.rect.y, 0.0f);
        const auto p11 = TableLayout::OnPlane(card.rect.x + card.rect.width,
                                              card.rect.y + card.rect.height, 0.0f);
        const float hx = 0.5f * (p11.x - p00.x);
        const float hz = 0.5f * (p11.z - p00.z);
        const float h = std::max(0.0f, card.elevation - TableLayout::CardElevation);
        const float cx = 0.5f * (p00.x + p11.x) + 0.004f + h * 0.45f;
        const float cz = 0.5f * (p00.z + p11.z) + 0.005f + h * 0.45f;
        const float flipScale = 0.25f + 0.75f * std::abs(std::cos(card.flipAngle));
        const float shx = hx * flipScale + 0.002f + h * 0.12f;
        const float shz = hz + 0.002f + h * 0.12f;
        const unsigned char alpha = static_cast<unsigned char>(std::clamp(58.0f - h * 260.0f, 22.0f, 64.0f));
        constexpr float shadowY = 0.0005f;
        const float cosY = std::cos(card.yaw), sinY = std::sin(card.yaw);
        const auto emitCorner = [&](float lx, float lz, float u, float v) {
            rlTexCoord2f(u, v);
            rlVertex3f(cx + lx * cosY + lz * sinY, shadowY, cz - lx * sinY + lz * cosY);
        };
        rlColor4ub(0, 0, 0, alpha);
        emitCorner(-shx, -shz, 0.0f, 0.0f);
        emitCorner(-shx,  shz, 0.0f, 1.0f);
        emitCorner( shx,  shz, 1.0f, 1.0f);
        emitCorner( shx, -shz, 1.0f, 0.0f);
    }
    rlEnd();
}

void DrawCards(Texture2D atlas, Texture2D cardBack, const std::vector<TableCard>& cards) {
    DrawShadows(cards);
    const unsigned int defaultTex = rlGetTextureIdDefault();
    for (const auto& card : cards) {
        const CardTransform t = MakeCardTransform(card);
        const bool backOnly = card.id < 0;
        const bool isStack = card.thickness > TableLayout::CardThickness * 1.5f;
        const Color frontSide = isStack ? Color{68, 65, 66, 255} : Color{50, 49, 54, 255};
        const Color leftSide  = isStack ? Color{54, 52, 54, 255} : Color{38, 37, 42, 255};
        const Color rightSide = isStack ? Color{40, 38, 40, 255} : Color{28, 27, 32, 255};
        const Color farSide   = isStack ? Color{30, 28, 30, 255} : Color{22, 22, 26, 255};

        // Top face (+Y): artwork or matte-black card back.
        rlSetTexture(backOnly ? cardBack.id : atlas.id);
        rlBegin(RL_QUADS);
        const float u0 = backOnly ? 0.0f : static_cast<float>(card.id / 4) / 12.0f;
        const float v0 = backOnly ? 0.0f : static_cast<float>(card.id % 4) / 4.0f;
        const float u1 = backOnly ? 1.0f : u0 + 1.0f / 12.0f;
        const float v1 = backOnly ? 1.0f : v0 + 1.0f / 4.0f;
        EmitQuad(t,
                 -t.hx, t.hy, -t.hz, u0, v0,
                 -t.hx, t.hy,  t.hz, u0, v1,
                  t.hx, t.hy,  t.hz, u1, v1,
                  t.hx, t.hy, -t.hz, u1, v0,
                 WHITE);
        rlEnd();

        // Bottom face (-Y): matte-black card back.
        rlSetTexture(cardBack.id);
        rlBegin(RL_QUADS);
        EmitQuad(t,
                  t.hx, -t.hy, -t.hz, 0.0f, 0.0f,
                  t.hx, -t.hy,  t.hz, 0.0f, 1.0f,
                 -t.hx, -t.hy,  t.hz, 1.0f, 1.0f,
                 -t.hx, -t.hy, -t.hz, 1.0f, 0.0f,
                 WHITE);
        rlEnd();

        // Four 3D side edges.
        rlSetTexture(defaultTex);
        rlBegin(RL_QUADS);
        EmitQuad(t,
                 -t.hx,  t.hy, t.hz, 0.0f, 0.0f,
                 -t.hx, -t.hy, t.hz, 0.0f, 1.0f,
                  t.hx, -t.hy, t.hz, 1.0f, 1.0f,
                  t.hx,  t.hy, t.hz, 1.0f, 0.0f,
                 frontSide);
        EmitQuad(t,
                  t.hx,  t.hy, -t.hz, 0.0f, 0.0f,
                  t.hx, -t.hy, -t.hz, 0.0f, 1.0f,
                 -t.hx, -t.hy, -t.hz, 1.0f, 1.0f,
                 -t.hx,  t.hy, -t.hz, 1.0f, 0.0f,
                 farSide);
        EmitQuad(t,
                 -t.hx,  t.hy, -t.hz, 0.0f, 0.0f,
                 -t.hx, -t.hy, -t.hz, 0.0f, 1.0f,
                 -t.hx, -t.hy,  t.hz, 1.0f, 1.0f,
                 -t.hx,  t.hy,  t.hz, 1.0f, 0.0f,
                 leftSide);
        EmitQuad(t,
                  t.hx,  t.hy,  t.hz, 0.0f, 0.0f,
                  t.hx, -t.hy,  t.hz, 0.0f, 1.0f,
                  t.hx, -t.hy, -t.hz, 1.0f, 1.0f,
                  t.hx,  t.hy, -t.hz, 1.0f, 0.0f,
                 rightSide);
        rlEnd();
    }
    rlSetTexture(0);
}

float BackGrain(int x, int y) {
    std::uint32_t n = static_cast<std::uint32_t>(x) * 374761393u +
                      static_cast<std::uint32_t>(y) * 668265263u;
    n = (n ^ (n >> 13)) * 1274126177u;
    n ^= n >> 16;
    return static_cast<float>(n & 0xFFu) / 255.0f;
}

float EmblemMask(float px, float py) {
    constexpr float Pi = 3.14159265f;
    const float dx = px - 89.5f;
    const float dy = py - 132.0f;
    const float r = std::hypot(dx, dy);
    const float ang = std::atan2(dy, dx);
    const float petal = 27.0f + 9.5f * std::cos(5.0f * (ang + Pi * 0.5f));
    const float s = std::sin(2.5f * (ang + Pi * 0.5f));
    const float notch = 3.5f * std::exp(-(s * s) * 18.0f);
    const float sakuraR = petal - notch;
    const bool inRing = std::abs(r - 37.5f) < 1.5f || std::abs(r - 10.5f) < 1.3f;
    const bool fillPetal = (r >= 11.5f && r <= sakuraR);
    const bool hubDot = (r <= 4.3f);
    return (inRing || fillPetal || hubDot) ? 1.0f : 0.0f;
}

Texture2D CreateMatteCardBackTexture() {
    constexpr int w = 179;
    constexpr int h = 264;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int edge = std::min({x, w - 1 - x, y, h - 1 - y});
            const float weave = ((x ^ y) & 1) ? 0.45f : -0.45f;
            const float grain = (BackGrain(x, y) - 0.5f) * 3.4f + weave;
            const float nx = (static_cast<float>(x) - w * 0.5f) / (w * 0.5f);
            const float ny = (static_cast<float>(y) - h * 0.5f) / (h * 0.5f);
            const float rad = std::hypot(nx, ny);
            float base = 28.0f - 8.0f * std::min(1.3f, rad);

            if (edge < 5) {
                base = 11.0f + grain * 0.35f;
            } else if (edge == 5 || edge == 6) {
                base = 15.0f + grain * 0.45f;
            } else {
                base += grain;
                if (edge == 13) base += 22.0f;
                else if (edge == 17 && std::min(x, w - 1 - x) >= 17 && std::min(y, h - 1 - y) >= 17) base += 11.0f;
                if (edge == 21 && (x < 24 || x >= w - 24) && (y < 24 || y >= h - 24)) base += 12.0f;

                const float e = EmblemMask(static_cast<float>(x), static_cast<float>(y));
                const float eHi = EmblemMask(static_cast<float>(x + 1), static_cast<float>(y + 1));
                const float eLo = EmblemMask(static_cast<float>(x - 1), static_cast<float>(y - 1));
                if (e > 0.5f) base += 14.0f;
                else if (eHi > 0.5f) base += 22.0f;
                else if (eLo > 0.5f) base -= 8.0f;
            }

            const auto clampByte = [](float v) -> unsigned char {
                return static_cast<unsigned char>(std::clamp(std::lround(v), 0L, 255L));
            };
            const std::size_t idx = (static_cast<std::size_t>(y) * w + x) * 4;
            pixels[idx]     = clampByte(base);
            pixels[idx + 1] = clampByte(base);
            pixels[idx + 2] = clampByte(base + 2.5f);
            pixels[idx + 3] = 255;
        }
    }
    const Image image{pixels.data(), w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D tex = LoadTextureFromImage(image);
    if (tex.id == 0 || tex.width <= 0 || tex.height <= 0) {
        if (tex.id != 0) UnloadTexture(tex);
        throw std::runtime_error("Could not create matte card back texture");
    }
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    return tex;
}

using ScreenMap = Ui::Viewport;

void DrawBox(const ScreenMap& map, TableLayout::Rect rect, Color fill, Color border,
             float borderPx = 1.5f) {
    DrawRectangle(map.X(rect.x), map.Y(rect.y), map.W(rect.width), map.H(rect.height), fill);
    DrawRectangleLinesEx(map.Rec(rect), std::max(1.0f, borderPx * map.scale), border);
}

void DrawDealerSelection(const ScreenMap& map, const TableGame& game) {
    DrawText("PICK A CARD TO DETERMINE DEALER", map.X(405), map.Y(100), map.Font(28), WHITE);
    const int choice = game.DealerChoice();
    if (choice < 0) {
        DrawText("CLICK TO PICK", map.X(505), map.Y(520), map.Font(18), LIGHTGRAY);
        DrawText("CLICK TO PICK", map.X(760), map.Y(520), map.Font(18), LIGHTGRAY);
        return;
    }
    if (game.OpeningTimer() >= 0.9f) {
        DrawText(choice == 0 ? "YOUR CARD" : "CPU CARD", map.X(520), map.Y(520), map.Font(18), WHITE);
        DrawText(choice == 1 ? "YOUR CARD" : "CPU CARD", map.X(775), map.Y(520), map.Font(18), WHITE);
        const char* result = game.OpeningDealer() == Player::PLAYER ? "YOU ARE DEALER" : "CPU IS DEALER";
        DrawText(result, map.X(570), map.Y(575), map.Font(30), WHITE);
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
        const Rect r = game.FieldRect(id);
        const auto corner = [](float x, float y) {
            const auto p = TableLayout::Project(TableLayout::OnPlane(x, y, TableLayout::CardElevation + TableLayout::CardThickness));
            return Vector2{p.x, p.y};
        };
        const float l = r.x - 2, t = r.y - 2, b = r.y + r.height + 2, right = r.x + r.width + 2;
        const std::array points{corner(l, t), corner(right, t), corner(right, b), corner(l, b)};
        for (std::size_t i = 0; i < points.size(); ++i)
            DrawLineEx(points[i], points[(i + 1) % points.size()], 2.0f, Color{255, 218, 53, 235});
    }
}

// Round-start bullet-time beat: the logo spins three turns in the middle of the table and
// slows to a front view. Width follows cos(angle) for the Y-axis roll; height breathes with
// sin(angle) to suggest depth. It fades out over the last fifth of the beat.
void DrawRoundIntroLogo(const ScreenMap& map, Texture2D logo, float progress) {
    constexpr float Pi = 3.14159265f;
    constexpr float LogoWidth = 380.0f;
    constexpr float LogoHeight = 132.0f;
    const float u = std::clamp(progress, 0.0f, 1.0f);
    const float ease = 1.0f - (1.0f - u) * (1.0f - u); // Fast start, slow stop.
    const float angle = 6.0f * Pi * ease;
    const float c = std::cos(angle);
    const float width = LogoWidth * std::fabs(c);
    const float height = LogoHeight * (1.0f + 0.1f * std::sin(angle));
    const float alpha = u < 0.8f ? 1.0f : std::clamp((1.0f - u) / 0.2f, 0.0f, 1.0f);
    const float centerX = static_cast<float>(TableLayout::ImageWidth) * 0.5f;
    const float centerY = static_cast<float>(TableLayout::ImageHeight) * 0.5f;
    const TableLayout::Rect box{centerX - width * 0.5f, centerY - height * 0.5f, width, height};
    // A negative source width mirrors the texture, so the back of the roll shows the logo reversed.
    const float sourceWidth = c < 0.0f ? -static_cast<float>(logo.width) : static_cast<float>(logo.width);
    const auto tint = Color{255, 255, 255, static_cast<unsigned char>(std::lround(alpha * 255.0f))};
    DrawTexturePro(logo, {0, 0, sourceWidth, static_cast<float>(logo.height)}, map.Rec(box), {0, 0}, 0.0f, tint);
}

// The HUD belongs to the screen, not the world canvas: it is drawn after the
// world blit, in the same 1360x768 design space as the click rectangles.
void DrawHud(const TableGame& game, const TableHudState& hud, Rectangle destination, float scale,
             Texture2D logo, Font headingFont, Font bodyFont) {
    const ScreenMap map{destination, scale};
    DrawRectangle(map.X(8), map.Y(8), map.W(500), map.H(102), Color{15, 20, 27, 220});
    const std::string label = "ROUND " + std::to_string(game.Round()) + " / " +
        std::to_string(TableLayout::MatchRounds) +
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
        const float beat = game.RoundIntroBulletProgress();
        if (beat < 1.0f) {
            DrawRoundIntroLogo(map, logo, beat);
        } else {
            const char* intro = game.Round() == 1 ? "MATCH START" : "ROUND START";
            DrawRectangle(map.X(500), map.Y(320), map.W(360), map.H(86), Color{8, 12, 18, 220});
            DrawText(intro, map.X(game.Round() == 1 ? 585 : 570), map.Y(344), map.Font(30), WHITE);
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
        Ui::DrawPauseRadioMenu(map, hud.radio, {logo, headingFont, bodyFont});
    }
}

Texture2D LoadRequiredTexture(const char* path) {
    Texture2D texture = LoadTexture(path);
    if (texture.id == 0 || texture.width <= 0 || texture.height <= 0) {
        if (texture.id != 0) UnloadTexture(texture);
        throw std::runtime_error(std::string("Could not load texture: ") + path);
    }
    return texture;
}

Font LoadMenuFont(const char* path, bool& owned) {
    const Font fallback = GetFontDefault();
    if (!FileExists(path)) {
        owned = false;
        return fallback;
    }
    Font font = LoadFontEx(path, 64, nullptr, 0);
    owned = font.texture.id != 0 && font.texture.id != fallback.texture.id;
    return owned ? font : fallback;
}
} // namespace

TableScene::TableScene(const char* backgroundPath, const char* atlasPath, const char* logoPath,
                       const char* headingFontPath, const char* bodyFontPath,
                       const std::filesystem::path& optionsDirectory) {
    try {
        background_ = LoadRequiredTexture(backgroundPath);
        atlas_ = LoadRequiredTexture(atlasPath);
        logo_ = LoadRequiredTexture(logoPath);
        headingFont_ = LoadMenuFont(headingFontPath, ownsHeadingFont_);
        bodyFont_ = LoadMenuFont(bodyFontPath, ownsBodyFont_);
        if (atlas_.width % 12 != 0 || atlas_.height % 4 != 0)
            throw std::runtime_error("Card atlas must be a tightly packed 12x4 grid");
        SetTextureFilter(atlas_, TEXTURE_FILTER_POINT);
        cardBack_ = CreateMatteCardBackTexture();
        optionsPanel_ = LoadRequiredTexture((optionsDirectory / "menu.png").string().c_str());
        optionsStripe_ = LoadRequiredTexture((optionsDirectory / "setting-stripe.png").string().c_str());
        SetTextureFilter(optionsPanel_, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(optionsStripe_, TEXTURE_FILTER_BILINEAR);
        canvas_ = LoadRenderTexture(TableLayout::ImageWidth, TableLayout::ImageHeight);
        if (canvas_.id == 0 || canvas_.texture.id == 0)
            throw std::runtime_error("Could not create table render texture");
        SetTextureFilter(canvas_.texture, TEXTURE_FILTER_BILINEAR);
        magnifyShader_ = LoadShaderFromMemory(nullptr, MagnifyFragmentShader);
        if (magnifyShader_.id == 0) throw std::runtime_error("Could not create magnification shader");
        magnifyFocusLocation_ = GetShaderLocation(magnifyShader_, "focus");
        magnifyZoomLocation_ = GetShaderLocation(magnifyShader_, "zoom");
    } catch (...) {
        if (ownsBodyFont_) UnloadFont(bodyFont_);
        if (ownsHeadingFont_) UnloadFont(headingFont_);
        if (magnifyShader_.id != 0) UnloadShader(magnifyShader_);
        if (canvas_.id != 0) UnloadRenderTexture(canvas_);
        if (optionsStripe_.id != 0) UnloadTexture(optionsStripe_);
        if (optionsPanel_.id != 0) UnloadTexture(optionsPanel_);
        if (cardBack_.id != 0) UnloadTexture(cardBack_);
        if (logo_.id != 0) UnloadTexture(logo_);
        if (atlas_.id != 0) UnloadTexture(atlas_);
        if (background_.id != 0) UnloadTexture(background_);
        throw;
    }
}

TableScene::~TableScene() {
    if (ownsBodyFont_) UnloadFont(bodyFont_);
    UnloadShader(magnifyShader_);
    if (ownsHeadingFont_) UnloadFont(headingFont_);
    if (sampledWorld_.id != 0) UnloadRenderTexture(sampledWorld_);
    UnloadRenderTexture(canvas_);
    UnloadTexture(optionsStripe_);
    UnloadTexture(optionsPanel_);
    UnloadTexture(cardBack_);
    UnloadTexture(logo_);
    UnloadTexture(atlas_);
    UnloadTexture(background_);
}

int TableScene::SetAntiAliasing(int samples) {
    if (samples == aaSamples_ || samples < 2 || samples > 4) return aaSamples_;
    // N scene samples per logical pixel (not N times each dimension). The
    // bilinear resolve below downsamples by at most 2:1 on either axis.
    // Allocate whole 85:48 blocks so the 3D projection aspect stays exact.
    constexpr int units = std::gcd(TableLayout::ImageWidth, TableLayout::ImageHeight);
    const int blocks = static_cast<int>(std::lround(units * std::sqrt(static_cast<float>(samples))));
    auto target = LoadRenderTexture(blocks * (TableLayout::ImageWidth / units),
                                    blocks * (TableLayout::ImageHeight / units));
    if (!target.id || !target.texture.id || !target.depth.id || !rlFramebufferComplete(target.id)) {
        if (target.id) UnloadRenderTexture(target);
        std::fprintf(stderr, "[options] Could not allocate %dx anti-aliasing; keeping %dx.\n", samples, aaSamples_);
        return aaSamples_;
    }
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    if (sampledWorld_.id) UnloadRenderTexture(sampledWorld_);
    sampledWorld_ = target;
    return aaSamples_ = samples;
}

void TableScene::Draw(const TableGame& game, const TableHudState& hud,
                      const MagnifyEffect& magnify) const {
    const auto world = sampledWorld_.id ? sampledWorld_ : canvas_;
    BeginTextureMode(world);
    ClearBackground(BLACK);
    DrawTexturePro(background_, {0, 0, static_cast<float>(background_.width), static_cast<float>(background_.height)},
                   {0, 0, static_cast<float>(world.texture.width), static_cast<float>(world.texture.height)},
                   {0, 0}, 0, WHITE);
    const Camera3D camera{{0, TableLayout::CameraHeight, 0},
                          {0, TableLayout::CameraHeight - std::sin(TableLayout::PitchRadians),
                           -std::cos(TableLayout::PitchRadians)},
                          {0, 1, 0}, TableLayout::FovDegrees, CAMERA_PERSPECTIVE};
    const auto cards = game.Cards();
    BeginMode3D(camera);
    DrawCards(atlas_, cardBack_, cards);
    EndMode3D();
    const Rectangle canvasDestination{0, 0, static_cast<float>(TableLayout::ImageWidth),
                                      static_cast<float>(TableLayout::ImageHeight)};
    if (sampledWorld_.id) {
        EndTextureMode();
        BeginTextureMode(canvas_);
        ClearBackground(BLACK);
        DrawTexturePro(world.texture, {0, 0, float(world.texture.width), -float(world.texture.height)},
                       canvasDestination, {0, 0}, 0, WHITE);
    }
    // Bullet time darkens the table behind the logo beat, fading out as the logo settles.
    if (const float beat = game.RoundIntroBulletProgress(); beat < 1.0f) {
        DrawRectangle(0, 0, world.texture.width, world.texture.height,
                      Color{0, 0, 0, static_cast<unsigned char>(std::lround(110.0f * (1.0f - beat)))});
    }
    if (hud.options.cardHints && hud.overlay == TableOverlay::NONE) DrawCardHints(game, cards, hud.pointer);
    DrawHud(game, hud, canvasDestination, 1.0f, logo_, headingFont_, bodyFont_);
    if (game.DealerSelectionActive())
        DrawDealerSelection(ScreenMap{canvasDestination, 1.0f}, game);
    EndTextureMode();

    const float scale = std::min(static_cast<float>(GetScreenWidth()) / TableLayout::ImageWidth,
                                 static_cast<float>(GetScreenHeight()) / TableLayout::ImageHeight);
    const float width = TableLayout::ImageWidth * scale;
    const float height = TableLayout::ImageHeight * scale;
    const Rectangle destination{(GetScreenWidth() - width) / 2.0f, (GetScreenHeight() - height) / 2.0f,
                                width, height};
    BeginDrawing();
    ClearBackground(BLACK);
    const float zoom = 1.0f + std::max(0.0f, magnify.amount);
    const float shaderFocus[2]{
        std::clamp(magnify.focus.x / TableLayout::ImageWidth, 0.0f, 1.0f),
        1.0f - std::clamp(magnify.focus.y / TableLayout::ImageHeight, 0.0f, 1.0f)};
    SetShaderValue(magnifyShader_, magnifyFocusLocation_, shaderFocus, SHADER_UNIFORM_VEC2);
    SetShaderValue(magnifyShader_, magnifyZoomLocation_, &zoom, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(magnifyShader_);
    // The shader magnifies final canvas pixels only; the 3D camera is untouched.
    DrawTexturePro(canvas_.texture, {0, 0, static_cast<float>(canvas_.texture.width),
                                    -static_cast<float>(canvas_.texture.height)},
                   destination, {0, 0}, 0, WHITE);
    EndShaderMode();
    // Keep the pause art above every game prompt and outside the inspection zoom.
    if (hud.overlay == TableOverlay::PAUSE)
        Ui::DrawPauseMenu({destination, scale}, hud.options, {GetScreenWidth(), GetScreenHeight()}, hud.focusedOption,
                          {optionsPanel_, optionsStripe_, headingFont_, bodyFont_});
    EndDrawing();
}
