#include "TableScene.hpp"

#include "TableLayout.hpp"
#include "TableGame.hpp"

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

// Maps the fixed 1360x768 HUD design space onto the letterboxed screen rect.
struct ScreenMap {
    Rectangle box{};
    float scale = 1.0f;
    [[nodiscard]] int X(float x) const { return static_cast<int>(std::lround(box.x + x * scale)); }
    [[nodiscard]] int Y(float y) const { return static_cast<int>(std::lround(box.y + y * scale)); }
    [[nodiscard]] int W(float w) const { return std::max(1, static_cast<int>(std::lround(w * scale))); }
    [[nodiscard]] int H(float h) const { return std::max(1, static_cast<int>(std::lround(h * scale))); }
    [[nodiscard]] int Font(float size) const { return W(size); }
    [[nodiscard]] Rectangle Rec(TableLayout::Rect r) const {
        return {box.x + r.x * scale, box.y + r.y * scale, r.width * scale, r.height * scale};
    }
};

void DrawBox(const ScreenMap& map, TableLayout::Rect r, Color fill, Color border, float borderPx = 1.5f) {
    DrawRectangle(map.X(r.x), map.Y(r.y), map.W(r.width), map.H(r.height), fill);
    DrawRectangleLinesEx(map.Rec(r), std::max(1.0f, borderPx * map.scale), border);
}

std::string FormatTenths(float seconds) {
    const int tenths = std::max(0, static_cast<int>(std::lround(seconds * 10.0f)));
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "s";
}

std::string BufferButtonText(const RadioSnapshot& radio) {
    if (radio.bufferMode == BufferMode::AUTO) {
        return "BUF: AUTO " + FormatTenths(radio.targetBufferSec);
    }
    return "BUF: " + FormatTenths(radio.targetBufferSec);
}

void DrawPauseMenu(const ScreenMap& map, const TableHudState& hud) {
    DrawRectangle(map.X(0), map.Y(0), map.W(TableLayout::ImageWidth), map.H(TableLayout::ImageHeight),
                  Color{8, 12, 18, 175});
    DrawBox(map, TableHud::PausePanel, Color{17, 23, 32, 246}, Color{98, 124, 148, 255}, 2.0f);

    DrawText("PAUSED", map.X(336), map.Y(116), map.Font(28), WHITE);
    DrawText("J-Pop Internet Radio (.pls)", map.X(462), map.Y(124), map.Font(18), Color{172, 206, 224, 255});
    DrawBox(map, TableHud::ResumeTopButton, Color{38, 78, 64, 245}, Color{102, 174, 144, 255});
    DrawText("RESUME", map.X(912), map.Y(129), map.Font(19), WHITE);

    const bool radioOn = hud.radio.enabled;
    const Color powerFill = radioOn ? Color{31, 86, 66, 245} : Color{46, 54, 66, 245};
    const Color powerBorder = radioOn ? Color{96, 184, 146, 255} : Color{96, 110, 126, 255};
    DrawBox(map, TableHud::RadioPowerButton, powerFill, powerBorder);
    DrawText(radioOn ? "RADIO: ON" : "RADIO: OFF", map.X(349), map.Y(197), map.Font(18), WHITE);

    const Color muteFill = hud.radio.muted ? Color{112, 54, 46, 245} : Color{40, 49, 60, 245};
    const Color muteBorder = hud.radio.muted ? Color{206, 114, 102, 255} : Color{96, 110, 126, 255};
    DrawBox(map, TableHud::RadioMuteButton, muteFill, muteBorder);
    DrawText(hud.radio.muted ? "MUTED" : "MUTE", map.X(496), map.Y(197), map.Font(17), WHITE);

    const bool autoBuf = (hud.radio.bufferMode == BufferMode::AUTO);
    const Color bufFill = autoBuf ? Color{32, 68, 84, 245} : Color{44, 54, 68, 245};
    const Color bufBorder = autoBuf ? Color{112, 184, 214, 255} : Color{96, 114, 134, 255};
    DrawBox(map, TableHud::RadioBufferButton, bufFill, bufBorder);
    DrawText(BufferButtonText(hud.radio).c_str(), map.X(588), map.Y(197), map.Font(17), WHITE);

    // Volume slider
    DrawBox(map, TableHud::VolumeBar, Color{26, 33, 44, 255}, Color{84, 104, 124, 255}, 1.0f);
    const float vol = std::clamp(hud.radio.volume, 0.0f, 1.0f);
    if (vol > 0.0f) {
        const Color fillCol = hud.radio.muted ? Color{108, 114, 122, 220} : Color{98, 178, 152, 255};
        DrawRectangle(map.X(TableHud::VolumeBar.x + 2.0f), map.Y(TableHud::VolumeBar.y + 2.0f),
                      map.W(std::max(2.0f, (TableHud::VolumeBar.width - 4.0f) * vol)),
                      map.H(TableHud::VolumeBar.height - 4.0f), fillCol);
    }
    const float knobX = TableHud::VolumeBar.x + vol * (TableHud::VolumeBar.width - 10.0f);
    DrawRectangle(map.X(knobX), map.Y(TableHud::VolumeBar.y - 3.0f), map.W(10.0f), map.H(26.0f), WHITE);
    const int volPct = static_cast<int>(std::lround(vol * 100.0f));
    DrawText((std::to_string(volPct) + "%").c_str(), map.X(974), map.Y(197), map.Font(18), WHITE);

    // Live station & stream metadata readout
    const std::string stationLabel = hud.radio.stationName.empty() ? "J-Pop Powerplay" : hud.radio.stationName;
    const std::string statusBadge = hud.radio.statusText.empty() ? "Radio off" : hud.radio.statusText;
    Color badgeColor = LIGHTGRAY;
    if (hud.radio.state == RadioState::PLAYING) badgeColor = Color{130, 228, 182, 255};
    else if (hud.radio.state == RadioState::CONNECTING || hud.radio.state == RadioState::BUFFERING)
        badgeColor = Color{240, 206, 126, 255};
    else if (hud.radio.state == RadioState::FAILED) badgeColor = Color{236, 122, 112, 255};

    DrawText(("Station: " + stationLabel).c_str(), map.X(338), map.Y(242), map.Font(17), WHITE);
    const std::string bufReadout = radioOn
        ? ("  Buf " + FormatTenths(hud.radio.bufferedSec) + "/" + FormatTenths(hud.radio.targetBufferSec) +
           "  Drops:" + std::to_string(hud.radio.underruns))
        : "";
    DrawText(("[" + statusBadge + bufReadout + "]").c_str(), map.X(656), map.Y(242), map.Font(16), badgeColor);
    const std::string trackLine = hud.radio.nowPlaying.empty()
        ? (radioOn ? "Now Playing: Streaming live J-Pop..." : "Select a J-Pop .pls station below to listen")
        : ("Now Playing: " + hud.radio.nowPlaying);
    DrawText(trackLine.c_str(), map.X(338), map.Y(263), map.Font(15), Color{202, 214, 226, 255});

    // 2x3 preset .pls station grid
    const auto& stations = DefaultRadioStations();
    for (std::size_t i = 0; i < stations.size() && i < TableHud::StationSlotCount; ++i) {
        const auto box = TableHud::StationButton(i);
        const bool selected = (hud.radio.stationIndex == static_cast<int>(i));
        const Color fill = selected
            ? (radioOn ? Color{31, 70, 62, 245} : Color{42, 54, 70, 245})
            : Color{28, 35, 46, 240};
        const Color border = selected ? Color{122, 198, 170, 255} : Color{72, 88, 108, 220};
        DrawBox(map, box, fill, border);
        DrawText(stations[i].name.c_str(), map.X(box.x + 14.0f), map.Y(box.y + 12.0f), map.Font(18), WHITE);
        DrawText((stations[i].subtitle + "  [.pls]").c_str(),
                 map.X(box.x + 14.0f), map.Y(box.y + 37.0f), map.Font(14),
                 selected ? Color{196, 232, 216, 255} : LIGHTGRAY);
    }

    // Custom .pls / stream URL paste row
    DrawBox(map, TableHud::PasteUrlButton, Color{42, 54, 72, 245}, Color{102, 128, 158, 255});
    DrawText("PASTE .PLS / URL", map.X(355), map.Y(536), map.Font(17), WHITE);

    const TableLayout::Rect customBox{558.0f, 524.0f, 466.0f, 42.0f};
    DrawBox(map, customBox, Color{24, 30, 40, 240}, Color{68, 84, 102, 220}, 1.0f);
    std::string customText = "Copy any .pls or MP3 stream URL, then click Paste";
    Color customColor = LIGHTGRAY;
    if (!hud.feedback.empty()) {
        customText = hud.feedback;
        customColor = Color{240, 206, 126, 255};
    } else if (hud.radio.stationIndex < 0 && !hud.radio.customUrl.empty()) {
        customText = SanitizeAsciiDisplay(hud.radio.customUrl, 48);
        customColor = Color{130, 228, 182, 255};
    }
    DrawText(customText.c_str(), map.X(570), map.Y(537), map.Font(15), customColor);

    DrawBox(map, TableHud::NewMatchButton, Color{94, 48, 42, 245}, Color{168, 96, 86, 255});
    DrawText("NEW MATCH", map.X(364), map.Y(607), map.Font(18), WHITE);
    DrawText("ESC / P: Pause   M: Mute", map.X(546), map.Y(609), map.Font(15), LIGHTGRAY);
    DrawBox(map, TableHud::ResumeBottomButton, Color{31, 75, 58, 245}, Color{96, 174, 144, 255});
    DrawText("RESUME GAME", map.X(872), map.Y(607), map.Font(18), WHITE);
}

// The HUD belongs to the screen, not the world canvas: it is drawn after the
// world blit, in the same 1360x768 design space as the click rectangles.
void DrawHud(const TableGame& game, const TableHudState& hud, Rectangle destination, float scale) {
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
        DrawText("KOI-KOI", map.X(1030), map.Y(30), map.Font(20), WHITE);
        DrawText("SHOBU", map.X(1190), map.Y(30), map.Font(20), WHITE);
    }
    if (game.Rules().GetPhase() == Phase::ROUND_END) {
        DrawRectangle(map.X(TableHud::RoundEndButton.x), map.Y(TableHud::RoundEndButton.y),
                      map.W(TableHud::RoundEndButton.width), map.H(TableHud::RoundEndButton.height), Color{15, 20, 27, 235});
        DrawText(game.MatchFinished() ? "MATCH OVER" : "NEXT ROUND", map.X(1020), map.Y(27), map.Font(20), WHITE);
        DrawText(game.MatchFinished() ? "Click to restart" : "Click to continue", map.X(1020), map.Y(55), map.Font(16), LIGHTGRAY);
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
        DrawPauseMenu(map, hud);
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
} // namespace

TableScene::TableScene(const char* backgroundPath, const char* atlasPath) {
    try {
        background_ = LoadRequiredTexture(backgroundPath);
        atlas_ = LoadRequiredTexture(atlasPath);
        if (atlas_.width % 12 != 0 || atlas_.height % 4 != 0)
            throw std::runtime_error("Card atlas must be a tightly packed 12x4 grid");
        SetTextureFilter(atlas_, TEXTURE_FILTER_POINT);
        cardBack_ = CreateMatteCardBackTexture();
        canvas_ = LoadRenderTexture(TableLayout::ImageWidth, TableLayout::ImageHeight);
        if (canvas_.id == 0 || canvas_.texture.id == 0)
            throw std::runtime_error("Could not create table render texture");
        SetTextureFilter(canvas_.texture, TEXTURE_FILTER_BILINEAR);
    } catch (...) {
        if (canvas_.id != 0) UnloadRenderTexture(canvas_);
        if (cardBack_.id != 0) UnloadTexture(cardBack_);
        if (atlas_.id != 0) UnloadTexture(atlas_);
        if (background_.id != 0) UnloadTexture(background_);
        throw;
    }
}

TableScene::~TableScene() {
    UnloadRenderTexture(canvas_);
    UnloadTexture(cardBack_);
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
    DrawHud(game, hud, destination, scale);
    EndDrawing();
}
