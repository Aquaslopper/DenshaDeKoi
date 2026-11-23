#include "TableScene.hpp"

#include "TableLayout.hpp"
#include "TableGame.hpp"
#include "ui/PauseRadioMenu.hpp"
#include "ui/UiViewport.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using TableLayout::Rect;

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

void DrawDealerCard(const ScreenMap& map, Texture2D atlas, Texture2D back,
                    int cardId, TableLayout::Rect rect, float progress) {
    progress = std::clamp(progress, 0.0f, 1.0f);
    const float width = rect.width * std::max(0.05f, std::abs(std::cos(progress * 3.14159265f)));
    const TableLayout::Rect dest{rect.x + (rect.width - width) * 0.5f, rect.y, width, rect.height};
    if (progress < 0.5f) {
        DrawTexturePro(back, {0, 0, static_cast<float>(back.width), static_cast<float>(back.height)},
                       map.Rec(dest), {0, 0}, 0, WHITE);
    } else {
        const float cellW = static_cast<float>(atlas.width) / 12.0f;
        const float cellH = static_cast<float>(atlas.height) / 4.0f;
        const Rectangle source{static_cast<float>(cardId / 4) * cellW,
                               static_cast<float>(cardId % 4) * cellH, cellW, cellH};
        DrawTexturePro(atlas, source, map.Rec(dest), {0, 0}, 0, WHITE);
    }
}

void DrawDealerSelection(const ScreenMap& map, const TableGame& game,
                         Texture2D atlas, Texture2D back) {
    DrawRectangle(map.X(0), map.Y(0), map.W(TableLayout::ImageWidth), map.H(TableLayout::ImageHeight),
                  Color{0, 0, 0, 238});
    DrawText("PICK A CARD TO DETERMINE DEALER", map.X(405), map.Y(100), map.Font(28), WHITE);
    const auto cards = game.DealerCards();
    const int choice = game.DealerChoice();
    if (choice < 0) {
        DrawDealerCard(map, atlas, back, cards[0], TableHud::DealerCardLeft, 0.0f);
        DrawDealerCard(map, atlas, back, cards[1], TableHud::DealerCardRight, 0.0f);
        DrawText("CLICK TO PICK", map.X(438), map.Y(515), map.Font(18), LIGHTGRAY);
        DrawText("CLICK TO PICK", map.X(782), map.Y(515), map.Font(18), LIGHTGRAY);
        return;
    }
    const float timer = game.OpeningTimer();
    const float leftProgress = choice == 0 ? timer / 0.45f : (timer - 0.45f) / 0.45f;
    const float rightProgress = choice == 1 ? timer / 0.45f : (timer - 0.45f) / 0.45f;
    DrawDealerCard(map, atlas, back, cards[0], TableHud::DealerCardLeft, leftProgress);
    DrawDealerCard(map, atlas, back, cards[1], TableHud::DealerCardRight, rightProgress);
    if (timer >= 0.9f) {
        DrawText(choice == 0 ? "YOUR CARD" : "CPU CARD", map.X(458), map.Y(515), map.Font(18), WHITE);
        DrawText(choice == 1 ? "YOUR CARD" : "CPU CARD", map.X(802), map.Y(515), map.Font(18), WHITE);
        const char* result = game.OpeningDealer() == Player::PLAYER ? "YOU ARE DEALER" : "CPU IS DEALER";
        DrawText(result, map.X(570), map.Y(575), map.Font(30), WHITE);
    }
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
        const char* intro = game.Round() == 1 ? "MATCH START" : "ROUND START";
        DrawRectangle(map.X(500), map.Y(320), map.W(360), map.H(86), Color{8, 12, 18, 220});
        DrawText(intro, map.X(game.Round() == 1 ? 585 : 570), map.Y(344), map.Font(30), WHITE);
    }

    // Right-side Pause / Radio Menu button
    const bool liveRadio = hud.radio.enabled && !hud.radio.muted;
    const Color pauseFill = hud.paused ? Color{34, 62, 56, 240} : Color{15, 20, 27, 230};
    const Color pauseBorder = liveRadio ? Color{102, 186, 152, 240} : Color{95, 118, 138, 220};
    DrawBox(map, TableHud::PauseButton, pauseFill, pauseBorder, 1.5f);
    // Twin pause bars icon
    DrawRectangle(map.X(TableHud::PauseButton.x + 14.0f), map.Y(TableHud::PauseButton.y + 14.0f),
                  map.W(5.0f), map.H(18.0f), WHITE);
    DrawRectangle(map.X(TableHud::PauseButton.x + 23.0f), map.Y(TableHud::PauseButton.y + 14.0f),
                  map.W(5.0f), map.H(18.0f), WHITE);
    DrawText(hud.paused ? "RESUME" : "MENU",
             map.X(TableHud::PauseButton.x + 36.0f), map.Y(TableHud::PauseButton.y + 14.0f),
             map.Font(19), WHITE);
    const char* subLabel = hud.radio.enabled
        ? (hud.radio.muted ? "RADIO MUTED" : (hud.radio.state == RadioState::PLAYING ? "J-POP LIVE" : "RADIO ..."))
        : "J-POP RADIO";
    const Color subColor = liveRadio ? Color{130, 228, 182, 255} : LIGHTGRAY;
    DrawText(subLabel, map.X(TableHud::PauseButton.x + 14.0f), map.Y(TableHud::PauseButton.y + 40.0f),
             map.Font(14), subColor);

    if (hud.paused) {
        Ui::DrawPauseRadioMenu(map, hud.radio,
                               {logo, headingFont, bodyFont});
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
                       const char* headingFontPath, const char* bodyFontPath) {
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
        canvas_ = LoadRenderTexture(TableLayout::ImageWidth, TableLayout::ImageHeight);
        if (canvas_.id == 0 || canvas_.texture.id == 0)
            throw std::runtime_error("Could not create table render texture");
        SetTextureFilter(canvas_.texture, TEXTURE_FILTER_BILINEAR);
    } catch (...) {
        if (ownsBodyFont_) UnloadFont(bodyFont_);
        if (ownsHeadingFont_) UnloadFont(headingFont_);
        if (canvas_.id != 0) UnloadRenderTexture(canvas_);
        if (cardBack_.id != 0) UnloadTexture(cardBack_);
        if (logo_.id != 0) UnloadTexture(logo_);
        if (atlas_.id != 0) UnloadTexture(atlas_);
        if (background_.id != 0) UnloadTexture(background_);
        throw;
    }
}

TableScene::~TableScene() {
    if (ownsBodyFont_) UnloadFont(bodyFont_);
    if (ownsHeadingFont_) UnloadFont(headingFont_);
    UnloadRenderTexture(canvas_);
    UnloadTexture(cardBack_);
    UnloadTexture(logo_);
    UnloadTexture(atlas_);
    UnloadTexture(background_);
}

void TableScene::Draw(const TableGame& game, const TableHudState& hud) const {
    BeginTextureMode(canvas_);
    ClearBackground(BLACK);
    DrawTexturePro(background_, {0, 0, static_cast<float>(background_.width), static_cast<float>(background_.height)},
                   {0, 0, static_cast<float>(TableLayout::ImageWidth), static_cast<float>(TableLayout::ImageHeight)},
                   {0, 0}, 0, WHITE);
    const Camera3D camera{{0, TableLayout::CameraHeight, 0},
                          {0, TableLayout::CameraHeight - std::sin(TableLayout::PitchRadians),
                           -std::cos(TableLayout::PitchRadians)},
                          {0, 1, 0}, TableLayout::FovDegrees, CAMERA_PERSPECTIVE};
    BeginMode3D(camera);
    DrawCards(atlas_, cardBack_, game.Cards());
    EndMode3D();
    EndTextureMode();

    const float scale = std::min(static_cast<float>(GetScreenWidth()) / TableLayout::ImageWidth,
                                 static_cast<float>(GetScreenHeight()) / TableLayout::ImageHeight);
    const float width = TableLayout::ImageWidth * scale;
    const float height = TableLayout::ImageHeight * scale;
    const Rectangle destination{(GetScreenWidth() - width) / 2.0f, (GetScreenHeight() - height) / 2.0f,
                                width, height};
    BeginDrawing();
    ClearBackground(BLACK);
    // A render texture is vertically inverted in Raylib; a negative source
    // height flips it back. The whole canvas is blitted: no crop, no zoom.
    DrawTexturePro(canvas_.texture, {0, 0, static_cast<float>(canvas_.texture.width),
                                    -static_cast<float>(canvas_.texture.height)},
                   destination, {0, 0}, 0, WHITE);
    DrawHud(game, hud, destination, scale, logo_, headingFont_, bodyFont_);
    if (game.DealerSelectionActive()) {
        DrawDealerSelection(ScreenMap{destination, scale}, game, atlas_, cardBack_);
    }
    EndDrawing();
}
