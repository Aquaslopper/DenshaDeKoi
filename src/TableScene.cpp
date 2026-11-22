#include "TableScene.hpp"

#include "TableLayout.hpp"
#include "TableGame.hpp"

#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
using TableLayout::Rect;

void Vertex(float x, float y, float u, float v, float elevation) {
    const auto point = TableLayout::OnPlane(x, y, elevation);
    rlTexCoord2f(u, v);
    rlVertex3f(point.x, point.y, point.z);
}

// CCW from above: top-left, bottom-left, bottom-right, top-right. The quad is
// on the 3D plane, so OpenGL performs perspective-correct texture interpolation.
void Quad(Rect r, float u0, float v0, float u1, float v1, Color color, float elevation) {
    rlColor4ub(color.r, color.g, color.b, color.a);
    Vertex(r.x, r.y, u0, v0, elevation);
    Vertex(r.x, r.y + r.height, u0, v1, elevation);
    Vertex(r.x + r.width, r.y + r.height, u1, v1, elevation);
    Vertex(r.x + r.width, r.y, u1, v0, elevation);
}

void DrawCards(Texture2D atlas, const TableCardList& cards) {
    for (const auto& card : cards) {
        Rect r = card.rect;
        r.x += r.width * (1.0f - card.flip) * 0.5f;
        r.width *= card.flip;
        const bool back = card.id < 0;
        rlSetTexture(back ? rlGetTextureIdDefault() : atlas.id);
        rlBegin(RL_QUADS);
        const float u0 = back ? 0.0f : static_cast<float>(card.id / 4) / 12.0f;
        const float v0 = back ? 0.0f : static_cast<float>(card.id % 4) / 4.0f;
        Quad(r, u0, v0, back ? 1.0f : u0 + 1.0f / 12.0f,
             back ? 1.0f : v0 + 1.0f / 4.0f, back ? BLACK : WHITE, card.elevation);
        rlEnd();
    }
    rlSetTexture(0);
}

// Warp the flat photograph onto the moving table plane. Two fixed-size rows
// avoid per-frame allocations; the sky is blended back to the original image.
void DrawBackdrop(Texture2D texture, const Camera3D& camera, const TableCameraController& controller) {
    constexpr int columns = 64, rows = 48;
    constexpr float width = static_cast<float>(TableLayout::ImageWidth);
    constexpr float height = static_cast<float>(TableLayout::ImageHeight);
    std::array<Vector2, columns + 1> upper{}, lower{};
    const auto fill = [&](std::array<Vector2, columns + 1>& row, float y) {
        for (int x = 0; x <= columns; ++x)
            row[static_cast<std::size_t>(x)] = controller.BackgroundUv(camera, {width * x / columns, y});
    };
    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    fill(upper, 0);
    for (int y = 0; y < rows; ++y) {
        const float y0 = height * y / rows, y1 = height * (y + 1) / rows;
        fill(lower, y1);
        for (int x = 0; x < columns; ++x) {
            const float x0 = width * x / columns, x1 = width * (x + 1) / columns;
            const auto vertex = [](float vx, float vy, Vector2 uv) { rlTexCoord2f(uv.x, uv.y); rlVertex2f(vx, vy); };
            vertex(x0, y0, upper[static_cast<std::size_t>(x)]);
            vertex(x0, y1, lower[static_cast<std::size_t>(x)]);
            vertex(x1, y1, lower[static_cast<std::size_t>(x + 1)]);
            vertex(x1, y0, upper[static_cast<std::size_t>(x + 1)]);
        }
        upper = lower;
    }
    rlEnd();
    rlSetTexture(0);
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
        canvas_ = LoadRenderTexture(TableLayout::ImageWidth, TableLayout::ImageHeight);
        if (canvas_.id == 0 || canvas_.texture.id == 0)
            throw std::runtime_error("Could not create table render texture");
        SetTextureFilter(canvas_.texture, TEXTURE_FILTER_BILINEAR);
    } catch (...) {
        if (canvas_.id != 0) UnloadRenderTexture(canvas_);
        if (atlas_.id != 0) UnloadTexture(atlas_);
        if (background_.id != 0) UnloadTexture(background_);
        throw;
    }
}

TableScene::~TableScene() {
    UnloadRenderTexture(canvas_);
    UnloadTexture(atlas_);
    UnloadTexture(background_);
}

void TableScene::Draw(const TableGame& game, const Camera3D& camera,
                      const TableCameraController& controller, const CameraSettings& settings) const {
    BeginTextureMode(canvas_);
    ClearBackground(BLACK);
    DrawBackdrop(background_, camera, controller);
    BeginMode3D(camera);
    DrawCards(atlas_, game.CardVisuals());
    EndMode3D();
    DrawRectangle(8, 8, 540, 126, Color{15, 20, 27, 220});
    char label[128], status[128];
    std::snprintf(label, sizeof(label), "ROUND %d / %d    YOU %d  -  CPU %d", game.Round(), TableLayout::MatchRounds,
                  game.Scores()[PlayerIndex(Player::PLAYER)], game.Scores()[PlayerIndex(Player::OPPONENT)]);
    game.FormatStatus(status, sizeof(status));
    DrawText(label, 20, 18, 22, WHITE);
    DrawText(status, 20, 51, 19, WHITE);
    DrawText("Drag a card to a match; right-click to cancel", 20, 80, 15, LIGHTGRAY);
    const char* cameraInfo = !settings.enableDynamicZoom ? "Camera OFF (Z)  |  Parallax (P)  |  Dolly [ / ]" :
                             !settings.enableParallax ? "Camera ON (Z)  |  Parallax OFF (P)  |  Dolly [ / ]" :
                             "Camera ON (Z)  |  Parallax ON (P)  |  Dolly [ / ]";
    DrawText(cameraInfo, 20, 108, 15, LIGHTGRAY);
    if (game.Rules().GetPhase() == Phase::AWAIT_DECISION && game.Rules().CurrentPlayer() == Player::PLAYER && !game.Busy()) {
        DrawRectangle(1010, 20, 145, 40, Color{31, 75, 58, 245});
        DrawRectangle(1165, 20, 145, 40, Color{100, 50, 38, 245});
        DrawText("KOI-KOI", 1030, 30, 20, WHITE);
        DrawText("SHOBU", 1190, 30, 20, WHITE);
    }
    if (game.Rules().GetPhase() == Phase::ROUND_END) {
        DrawRectangle(1000, 20, 310, 66, Color{15, 20, 27, 235});
        DrawText(game.MatchFinished() ? "MATCH OVER" : "NEXT ROUND", 1020, 27, 20, WHITE);
        DrawText(game.MatchFinished() ? "Click to restart" : "Click to continue", 1020, 55, 16, LIGHTGRAY);
    }
    EndTextureMode();

    const Rectangle destination = TableCameraController::CanvasViewport(static_cast<float>(GetScreenWidth()),
                                                                          static_cast<float>(GetScreenHeight()));
    BeginDrawing();
    ClearBackground(BLACK);
    // A render texture is vertically inverted in Raylib.
    DrawTexturePro(canvas_.texture, {0, 0, static_cast<float>(canvas_.texture.width),
                                    -static_cast<float>(canvas_.texture.height)}, destination, {0, 0}, 0, WHITE);
    EndDrawing();
}
