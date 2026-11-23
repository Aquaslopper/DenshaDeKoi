#include "TableScene.hpp"

#include "TableLayout.hpp"
#include "TableGame.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
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


// ---------- Retro pause-menu chrome helpers (share the pixel-mock look) ----------
constexpr float SilkAscCap = 0.33f; // (1.03 - 0.70) em for Silkscreen
constexpr float VtAscCap = 0.24f;   // (0.80 - 0.56) em for VT323

Color LerpColor(Color a, Color b, float t) {
    const auto mix = [t](unsigned char ca, unsigned char cb) {
        return static_cast<unsigned char>(std::lround(ca + (cb - ca) * t));
    };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

void DrawVGradient(const ScreenMap& map, TableLayout::Rect r,
                   std::initializer_list<std::pair<float, Color>> stops) {
    const int bands = 48;
    const float bandH = r.height / static_cast<float>(bands);
    for (int i = 0; i < bands; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(bands);
        const auto* begin = stops.begin();
        const auto* end = stops.end();
        Color color = begin->second;
        for (const auto* it = begin; it != end; ++it) {
            const auto* next = std::next(it);
            if (next == end) { color = it->second; break; }
            if (t <= next->first) {
                const float span = next->first - it->first;
                const float f = span > 1e-6f ? (t - it->first) / span : 0.0f;
                color = LerpColor(it->second, next->second, std::clamp(f, 0.0f, 1.0f));
                break;
            }
        }
        const float y = r.y + static_cast<float>(i) * bandH;
        DrawRectangle(map.X(r.x), map.Y(y), map.W(r.width), map.H(bandH + 0.75f), color);
    }
}

void DrawSilk(const ScreenMap& map, Font font, const char* text, float x, float capTop,
              float size, Color color, bool shadow = false) {
    const Vector2 pos{static_cast<float>(map.X(x)),
                      static_cast<float>(map.Y(capTop - SilkAscCap * size))};
    const float fs = static_cast<float>(map.W(size));
    if (shadow)
        DrawTextEx(font, text, {pos.x + 2.0f * map.scale, pos.y + 2.0f * map.scale},
                   fs, 0.0f, Color{0, 10, 45, 255});
    DrawTextEx(font, text, pos, fs, 0.0f, color);
}

void DrawVt(const ScreenMap& map, Font font, const char* text, float x, float capTop,
            float size, Color color, bool bold = false, bool shadow = false) {
    const Vector2 pos{static_cast<float>(map.X(x)),
                      static_cast<float>(map.Y(capTop - VtAscCap * size))};
    const float fs = static_cast<float>(map.W(size));
    if (shadow)
        DrawTextEx(font, text, {pos.x + 2.0f * map.scale, pos.y + 2.0f * map.scale},
                   fs, 0.0f, Color{10, 10, 14, 200});
    if (bold)
        DrawTextEx(font, text, {pos.x + 1.0f * map.scale, pos.y}, fs, 0.0f, color);
    DrawTextEx(font, text, pos, fs, 0.0f, color);
}

float VtFit(Font font, const char* text, float size, float maxWidth) {
    float s = size;
    while (s > 12.0f) {
        if (MeasureTextEx(font, text, s, 0.0f).x <= maxWidth) break;
        s -= 1.0f;
    }
    return s;
}

enum class ButtonKind { Selected, Idle, Off, Mute };

void DrawChromeButton(const ScreenMap& map, TableLayout::Rect r, ButtonKind kind,
                      bool muted = false) {
    const Color outer{11, 13, 18, 255};
    DrawRectangle(map.X(r.x), map.Y(r.y), map.W(r.width), map.H(r.height), outer);
    if (kind == ButtonKind::Selected) {
        DrawRectangle(map.X(r.x + 2), map.Y(r.y + 2), map.W(r.width - 4), map.H(r.height - 4),
                      Color{245, 246, 250, 255});
        DrawVGradient(map, {r.x + 5, r.y + 5, r.width - 10, r.height - 10},
                      {{0.0f, {0, 19, 198, 255}}, {1.0f, {0, 10, 156, 255}}});
        return;
    }
    if (kind == ButtonKind::Off) {
        DrawRectangle(map.X(r.x + 2), map.Y(r.y + 2), map.W(r.width - 4), map.H(r.height - 4),
                      Color{199, 203, 230, 255});
        DrawVGradient(map, {r.x + 4, r.y + 4, r.width - 8, r.height - 8},
                      {{0.0f, {10, 31, 122, 255}}, {1.0f, {1, 18, 115, 255}}});
        return;
    }
    if (kind == ButtonKind::Mute) {
        DrawRectangle(map.X(r.x + 2), map.Y(r.y + 2), map.W(r.width - 4), map.H(2),
                      Color{205, 209, 216, 255});
        DrawRectangle(map.X(r.x + 2), map.Y(r.y + 2), map.W(2), map.H(r.height - 4),
                      Color{205, 209, 216, 255});
        DrawRectangle(map.X(r.x + 2), map.Y(r.y + r.height - 4), map.W(r.width - 4), map.H(2),
                      Color{56, 59, 66, 255});
        DrawRectangle(map.X(r.x + r.width - 4), map.Y(r.y + 2), map.W(2), map.H(r.height - 4),
                      Color{56, 59, 66, 255});
        if (muted)
            DrawVGradient(map, {r.x + 4, r.y + 4, r.width - 8, r.height - 8},
                          {{0.0f, {110, 74, 66, 255}}, {1.0f, {88, 60, 55, 255}}});
        else
            DrawVGradient(map, {r.x + 4, r.y + 4, r.width - 8, r.height - 8},
                          {{0.0f, {106, 110, 120, 255}}, {1.0f, {94, 98, 107, 255}}});
        return;
    }
    // Idle: light top-left bevel, dark bottom-right bevel, slate face.
    DrawRectangle(map.X(r.x + 2), map.Y(r.y + 2), map.W(r.width - 4), map.H(2),
                  Color{90, 98, 114, 255});
    DrawRectangle(map.X(r.x + 2), map.Y(r.y + 2), map.W(2), map.H(r.height - 4),
                  Color{90, 98, 114, 255});
    DrawRectangle(map.X(r.x + 2), map.Y(r.y + r.height - 4), map.W(r.width - 4), map.H(2),
                  Color{25, 29, 37, 255});
    DrawRectangle(map.X(r.x + r.width - 4), map.Y(r.y + 2), map.W(2), map.H(r.height - 4),
                  Color{25, 29, 37, 255});
    DrawVGradient(map, {r.x + 4, r.y + 4, r.width - 8, r.height - 8},
                  {{0.0f, {53, 59, 73, 255}}, {1.0f, {43, 48, 59, 255}}});
}

void DrawArrow(const ScreenMap& map, float x, float cy) {
    const auto pt = [&map](float px, float py) {
        return Vector2{static_cast<float>(map.X(px)), static_cast<float>(map.Y(py))};
    };
    DrawTriangle(pt(x - 2, cy - 22), pt(x + 40, cy), pt(x - 2, cy + 22), Color{138, 90, 0, 255});
    DrawTriangle(pt(x, cy - 20), pt(x + 38, cy), pt(x + 10, cy), Color{253, 199, 38, 255});
    DrawTriangle(pt(x, cy + 20), pt(x + 38, cy), pt(x + 10, cy), Color{253, 199, 38, 255});
}

std::string FormatTenths(float seconds) {
    const int tenths = std::max(0, static_cast<int>(std::lround(seconds * 10.0f)));
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "s";
}

void DrawPauseMenu(const ScreenMap& map, const TableHudState& hud, const TableSceneUiAssets& assets) {
    // (The dim layer itself is drawn by DrawHud before the score strip, so the
    // strip and edge chip stay lit like in the mock.)
    const float PX = TableHud::PausePanel.x, PYi = TableHud::PausePanel.y;
    const float PW = TableHud::PausePanel.width, PH = TableHud::PausePanel.height;
    const float RIGHT = PX + PW;
    const Color navy{21, 27, 95, 255};
    const Color white{255, 255, 255, 255};

    // Drop shadow, 1 px outer edge, white frame.
    DrawRectangle(map.X(PX + 5), map.Y(PYi + 7), map.W(PW), map.H(PH), Color{0, 0, 0, 90});
    DrawRectangle(map.X(PX - 1), map.Y(PYi - 1), map.W(PW + 2), map.H(PH + 2), Color{16, 16, 20, 255});
    DrawRectangle(map.X(PX), map.Y(PYi), map.W(PW), map.H(PH), Color{242, 243, 246, 255});

    // Navy title bar with an orange/green stripe underneath.
    DrawVGradient(map, {PX + 3, PYi + 3, PW - 6, 80.0f},
                  {{0.0f, {2, 23, 116, 255}}, {1.0f, {1, 15, 88, 255}}});
    DrawRectangle(map.X(PX + 3), map.Y(183), map.W(PW - 6), map.H(9), Color{248, 146, 46, 255});
    DrawRectangle(map.X(PX + 3), map.Y(192), map.W(PW - 6), map.H(1), Color{122, 62, 12, 255});
    DrawRectangle(map.X(PX + 3), map.Y(193), map.W(PW - 6), map.H(8), Color{13, 104, 48, 255});
    DrawRectangle(map.X(PX + 3), map.Y(201), map.W(PW - 6), map.H(1), Color{5, 46, 21, 255});

    // Light-grey body with the mock's faint horizontal scanlines.
    DrawVGradient(map, {PX + 3, 202, PW - 6, 374},
                  {{0.00f, {168, 169, 167, 255}}, {0.35f, {183, 184, 182, 255}},
                   {0.60f, {191, 192, 190, 255}}, {0.80f, {181, 182, 180, 255}},
                   {1.00f, {170, 171, 169, 255}}});
    for (float y = 202.0f; y < 576.0f; y += 4.0f)
        DrawRectangle(map.X(PX + 3), map.Y(y), map.W(PW - 6), std::max(1, map.H(1.0f)),
                      Color{0, 0, 0, 14});

    // Title bar: PAUSE + MENU (stacked, bottom-aligned), PAUSED, and the wordmark.
    DrawSilk(map, assets.title64, "PAUSE", 278.0f, 117.0f, 64.0f, white, true);
    const float pauseW = MeasureTextEx(assets.title64, "PAUSE", 64.0f, 0.0f).x;
    const float menuX = 278.0f + pauseW + 30.0f;
    DrawSilk(map, assets.title32, "MENU", menuX, 139.4f, 32.0f, white, true);
    const float menuW = MeasureTextEx(assets.title32, "MENU", 32.0f, 0.0f).x;
    DrawSilk(map, assets.title24, "PAUSED", menuX + menuW + 24.0f, 145.0f, 24.0f,
             Color{156, 168, 196, 255}, true);
    if (assets.logoValid) {
        const float lh = 64.0f;
        const float lw = lh * static_cast<float>(assets.logo.width) /
                         static_cast<float>(assets.logo.height);
        const Rectangle src{0.0f, 0.0f, static_cast<float>(assets.logo.width),
                            static_cast<float>(assets.logo.height)};
        const Rectangle dst{static_cast<float>(map.X(RIGHT - 22.0f - lw)),
                            static_cast<float>(map.Y(112.0f)),
                            lw * map.scale, lh * map.scale};
        DrawTexturePro(assets.logo, src, dst, {0.0f, 0.0f}, 0.0f, WHITE);
    }

    // BGM row: blue OFF/ON power toggle, chunky MUTE bevel button, segmented meter.
    const bool radioOn = hud.radio.enabled;
    DrawSilk(map, assets.title24, "BGM", 292.0f, 224.0f, 24.0f, navy);
    DrawChromeButton(map, TableHud::RadioPowerButton, ButtonKind::Off);
    {
        const auto& b = TableHud::RadioPowerButton;
        const char* label = radioOn ? "ON" : "OFF";
        const float tw = MeasureTextEx(assets.title24, label, 24.0f, 0.0f).x;
        DrawSilk(map, assets.title24, label, b.x + (b.width - tw) * 0.5f, b.y + 11.0f, 24.0f, white);
    }
    DrawChromeButton(map, TableHud::RadioMuteButton, ButtonKind::Mute, hud.radio.muted);
    {
        const auto& b = TableHud::RadioMuteButton;
        const char* label = hud.radio.muted ? "MUTED" : "MUTE";
        const float tw = MeasureTextEx(assets.title24, label, 24.0f, 0.0f).x;
        DrawSilk(map, assets.title24, label, b.x + (b.width - tw) * 0.5f, b.y + 11.0f, 24.0f,
                 Color{20, 20, 25, 255});
    }
    const float vol = std::clamp(hud.radio.volume, 0.0f, 1.0f);
    const int filled = static_cast<int>(std::lround(vol * 20.0f));
    const auto& meter = TableHud::VolumeMeter;
    for (int i = 0; i < 20; ++i) {
        const float sx = meter.x + static_cast<float>(i) * 14.0f;
        if (i < filled) {
            DrawRectangle(map.X(sx), map.Y(meter.y), map.W(8.0f), map.H(meter.height),
                          Color{15, 92, 51, 255});
            DrawRectangle(map.X(sx), map.Y(meter.y), map.W(8.0f), std::max(1, map.H(2.5f)),
                          Color{23, 120, 70, 255});
        } else {
            DrawRectangle(map.X(sx), map.Y(meter.y), map.W(8.0f), map.H(meter.height),
                          Color{106, 110, 117, 255});
            DrawRectangle(map.X(sx), map.Y(meter.y + meter.height - 2.5f), map.W(8.0f),
                          std::max(1, map.H(2.5f)), Color{84, 88, 95, 255});
        }
    }
    const std::string pct = std::to_string(static_cast<int>(std::lround(vol * 100.0f))) + "%";
    DrawVt(map, assets.vt28, pct.c_str(), 1032.0f, 229.0f, 28.0f, navy, true);

    // STATION readout: navy label, big black name, bracketed live status.
    DrawSilk(map, assets.title16, "STATION", 292.0f, 286.0f, 16.0f, navy);
    const std::string stationLabel =
        hud.radio.stationName.empty() ? "J-Pop Powerplay" : hud.radio.stationName;
    DrawVt(map, assets.vt42, stationLabel.c_str(), 400.0f, 270.0f, 42.0f,
           Color{16, 16, 19, 255}, true);
    std::string status = hud.radio.statusText.empty() ? "Radio off" : hud.radio.statusText;
    if (hud.radio.enabled && hud.radio.state == RadioState::PLAYING && hud.radio.bufferedSec > 0.05f)
        status += " - buffered " + FormatTenths(hud.radio.bufferedSec);
    DrawVt(map, assets.vt25, ("[" + status + "]").c_str(), 433.0f, 302.0f, 25.0f,
           Color{24, 24, 28, 255});

    // 2x3 preset .pls station grid with chunky beveled chrome.
    const auto& stations = DefaultRadioStations();
    for (std::size_t i = 0; i < stations.size() && i < TableHud::StationSlotCount; ++i) {
        const auto box = TableHud::StationButton(i);
        const bool selected = hud.radio.stationIndex == static_cast<int>(i);
        DrawChromeButton(map, box, selected ? ButtonKind::Selected : ButtonKind::Idle);
        const float textX = box.x + (selected ? 88.0f : 51.0f);
        const float maxW = box.x + box.width - textX - 12.0f;
        DrawVt(map, assets.vt28, stations[i].name.c_str(), textX, box.y + 10.0f,
               VtFit(assets.vt28, stations[i].name.c_str(), 28.0f, maxW),
               white, true, true);
        const std::string sub = stations[i].subtitle + "  [pls]";
        DrawVt(map, assets.vt21, sub.c_str(), textX, box.y + 38.0f,
               VtFit(assets.vt21, sub.c_str(), 21.0f, maxW),
               selected ? Color{233, 236, 250, 255} : Color{201, 205, 214, 255}, false, true);
        if (selected) DrawArrow(map, box.x + 12.0f, box.y + box.height * 0.5f);
    }

    // Footer divider: light line over a black groove.
    DrawRectangle(map.X(PX + 3), map.Y(576), map.W(PW - 6), map.H(2), Color{232, 232, 232, 255});
    DrawRectangle(map.X(PX + 3), map.Y(578), map.W(PW - 6), map.H(4), Color{16, 16, 20, 255});
    // Footer bar: the first column starts a new match, the last resumes; the
    // middle two are static hotkey hints.
    const struct { const char* line1; const char* line2; } footerCols[4] = {
        {"START NEW MATCH", "NEW MATCH"},
        {"OPEN MENU", "ESC / P : PAUSE"},
        {"TOGGLE VOLUME", "M : MUTE"},
        {"RETURN TO GAME", "RESUME GAME"},
    };
    const Vector2 mouse = GetMousePosition();
    for (int c = 0; c < 4; ++c) {
        if (c == 0 && CheckCollisionPointRec(mouse, map.Rec(TableHud::FooterNewMatch)))
            DrawRectangle(map.X(TableHud::FooterNewMatch.x), map.Y(TableHud::FooterNewMatch.y),
                          map.W(TableHud::FooterNewMatch.width), map.H(TableHud::FooterNewMatch.height),
                          Color{255, 255, 255, 38});
        if (c == 3 && CheckCollisionPointRec(mouse, map.Rec(TableHud::FooterResume)))
            DrawRectangle(map.X(TableHud::FooterResume.x), map.Y(TableHud::FooterResume.y),
                          map.W(TableHud::FooterResume.width), map.H(TableHud::FooterResume.height),
                          Color{255, 255, 255, 38});
        DrawVt(map, assets.vt24, footerCols[c].line1, TableHud::FooterColX[c], 600.0f, 24.0f,
               Color{40, 40, 44, 255});
        DrawVt(map, assets.vt24, footerCols[c].line2, TableHud::FooterColX[c], 622.0f, 24.0f,
               Color{18, 18, 20, 255}, true);
    }
    // Bottom white frame + 1 px outer edge.
    DrawRectangle(map.X(PX), map.Y(650), map.W(PW), map.H(3), Color{242, 243, 246, 255});
    DrawRectangle(map.X(PX - 1), map.Y(653), map.W(PW + 2), map.H(1.5f), Color{16, 16, 20, 255});
}

// The HUD belongs to the screen, not the world canvas: it is drawn after the
// world blit, in the same 1360x768 design space as the click rectangles.
void DrawHud(const TableGame& game, const TableHudState& hud, const TableSceneUiAssets& assets,
             Rectangle destination, float scale) {
    const ScreenMap map{destination, scale};
    // The mock dims the frozen table but keeps the score strip and the
    // right-edge chip fully lit on top of the dim layer.
    if (hud.paused) {
        DrawRectangle(map.X(0), map.Y(0), map.W(TableLayout::ImageWidth), map.H(TableLayout::ImageHeight),
                      Color{8, 12, 18, 175});
    }
    DrawRectangle(map.X(8), map.Y(8), map.W(500), map.H(102), Color{15, 20, 27, 220});
    const std::string label = "ROUND " + std::to_string(game.Round()) + " / " +
        std::to_string(TableLayout::MatchRounds) +
        "    YOU " + std::to_string(game.Scores()[PlayerIndex(Player::PLAYER)]) +
        "  -  CPU " + std::to_string(game.Scores()[PlayerIndex(Player::OPPONENT)]);
    DrawText(label.c_str(), map.X(20), map.Y(18), map.Font(22), WHITE);
    DrawText(game.Status().c_str(), map.X(20), map.Y(51), map.Font(19), WHITE);
    DrawText("Drag a card to a match; right-click to cancel", map.X(20), map.Y(80), map.Font(15), LIGHTGRAY);
    if (!hud.paused && game.Rules().GetPhase() == Phase::AWAIT_DECISION &&
        game.Rules().CurrentPlayer() == Player::PLAYER && !game.Busy()) {
        DrawRectangle(map.X(TableHud::KoiKoiButton.x), map.Y(TableHud::KoiKoiButton.y),
                      map.W(TableHud::KoiKoiButton.width), map.H(TableHud::KoiKoiButton.height), Color{31, 75, 58, 245});
        DrawRectangle(map.X(TableHud::ShobuButton.x), map.Y(TableHud::ShobuButton.y),
                      map.W(TableHud::ShobuButton.width), map.H(TableHud::ShobuButton.height), Color{100, 50, 38, 245});
        DrawText("KOI-KOI", map.X(1030), map.Y(30), map.Font(20), WHITE);
        DrawText("SHOBU", map.X(1190), map.Y(30), map.Font(20), WHITE);
    }
    if (!hud.paused && game.Rules().GetPhase() == Phase::ROUND_END) {
        DrawRectangle(map.X(TableHud::RoundEndButton.x), map.Y(TableHud::RoundEndButton.y),
                      map.W(TableHud::RoundEndButton.width), map.H(TableHud::RoundEndButton.height), Color{15, 20, 27, 235});
        DrawText(game.MatchFinished() ? "MATCH OVER" : "NEXT ROUND", map.X(1020), map.Y(27), map.Font(20), WHITE);
        DrawText(game.MatchFinished() ? "Click to restart" : "Click to continue", map.X(1020), map.Y(55), map.Font(16), LIGHTGRAY);
    }

    // Right-edge chip: "II RESUME / J-POP RADIO" while paused, else "MENU / J-POP RADIO".
    const bool liveRadio = hud.radio.enabled && !hud.radio.muted;
    const auto& chip = TableHud::PauseButton;
    DrawRectangle(map.X(chip.x), map.Y(chip.y), map.W(chip.width), map.H(chip.height),
                  Color{14, 20, 32, 235});
    DrawRectangleLinesEx(map.Rec(chip), std::max(1.0f, 1.0f * map.scale), Color{58, 74, 94, 255});
    // Twin pause-bars icon
    DrawRectangle(map.X(chip.x + 12), map.Y(chip.y + 15), map.W(5), map.H(16), Color{200, 205, 214, 255});
    DrawRectangle(map.X(chip.x + 21), map.Y(chip.y + 15), map.W(5), map.H(16), Color{200, 205, 214, 255});
    DrawSilk(map, assets.title16, hud.paused ? "RESUME" : "MENU", chip.x + 34.0f, chip.y + 12.0f, 16.0f,
             Color{200, 205, 214, 255});
    const char* subLabel = hud.radio.enabled
        ? (hud.radio.muted ? "RADIO MUTED" : (hud.radio.state == RadioState::PLAYING ? "J-POP LIVE" : "RADIO ..."))
        : "J-POP RADIO";
    DrawVt(map, assets.vt21, subLabel, chip.x + 12.0f, chip.y + 40.0f, 21.0f,
           liveRadio ? Color{53, 214, 176, 255} : LIGHTGRAY);

    if (hud.paused) {
        DrawPauseMenu(map, hud, assets);
    }
}

Font LoadFontOrDefault(const char* path, int size) {
    Font font = LoadFontEx(path, size, nullptr, 0);
    if (font.texture.id == 0 || font.texture.width <= 0 || font.texture.height <= 0) {
        std::fprintf(stderr, "[ui] could not load font '%s'; using the default font\n", path);
        return GetFontDefault();
    }
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}

void ReleaseFont(Font font) {
    // The shared Raylib default font is owned by Raylib itself; never unload it.
    if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id) UnloadFont(font);
}

TableSceneUiAssets LoadPauseAssets() {
    const std::filesystem::path appDir(GetApplicationDirectory());
    const std::string fontsDir = (appDir / "fonts").string();
    TableSceneUiAssets assets;
    assets.title64 = LoadFontOrDefault((fontsDir + "/Silkscreen-Bold.ttf").c_str(), 64);
    assets.title32 = LoadFontOrDefault((fontsDir + "/Silkscreen-Bold.ttf").c_str(), 32);
    assets.title24 = LoadFontOrDefault((fontsDir + "/Silkscreen-Bold.ttf").c_str(), 24);
    assets.title16 = LoadFontOrDefault((fontsDir + "/Silkscreen-Bold.ttf").c_str(), 16);
    assets.vt42 = LoadFontOrDefault((fontsDir + "/VT323.ttf").c_str(), 42);
    assets.vt28 = LoadFontOrDefault((fontsDir + "/VT323.ttf").c_str(), 28);
    assets.vt25 = LoadFontOrDefault((fontsDir + "/VT323.ttf").c_str(), 25);
    assets.vt24 = LoadFontOrDefault((fontsDir + "/VT323.ttf").c_str(), 24);
    assets.vt21 = LoadFontOrDefault((fontsDir + "/VT323.ttf").c_str(), 21);
    const std::string logoPath = (appDir / "densha-logo.png").string();
    assets.logo = LoadTexture(logoPath.c_str());
    if (assets.logo.id == 0 || assets.logo.width <= 0 || assets.logo.height <= 0) {
        std::fprintf(stderr, "[ui] could not load logo '%s'; title bar will render text only\n",
                     logoPath.c_str());
        assets.logoValid = false;
    } else {
        SetTextureFilter(assets.logo, TEXTURE_FILTER_BILINEAR);
        assets.logoValid = true;
    }
    return assets;
}

void ReleasePauseAssets(TableSceneUiAssets& assets) {
    if (assets.logoValid) UnloadTexture(assets.logo);
    ReleaseFont(assets.vt21);
    ReleaseFont(assets.vt24);
    ReleaseFont(assets.vt25);
    ReleaseFont(assets.vt28);
    ReleaseFont(assets.vt42);
    ReleaseFont(assets.title16);
    ReleaseFont(assets.title24);
    ReleaseFont(assets.title32);
    ReleaseFont(assets.title64);
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
        assets_ = LoadPauseAssets();
    } catch (...) {
        ReleasePauseAssets(assets_);
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
    ReleasePauseAssets(assets_);
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
    DrawHud(game, hud, assets_, destination, scale);
    EndDrawing();
}
