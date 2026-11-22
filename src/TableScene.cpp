#include "TableScene.hpp"

#include "TableLayout.hpp"
#include "TableGame.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cmath>
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

void DrawCards(Texture2D atlas, const std::vector<TableCard>& cards) {
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

void TableScene::Draw(const TableGame& game) const {
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
    DrawCards(atlas_, game.Cards());
    EndMode3D();
    DrawRectangle(8, 8, 500, 102, Color{15, 20, 27, 220});
    const std::string label = "ROUND " + std::to_string(game.Round()) + " / " + std::to_string(TableLayout::MatchRounds) +
        "    YOU " + std::to_string(game.Scores()[PlayerIndex(Player::PLAYER)]) +
        "  -  CPU " + std::to_string(game.Scores()[PlayerIndex(Player::OPPONENT)]);
    DrawText(label.c_str(), 20, 18, 22, WHITE);
    DrawText(game.Status().c_str(), 20, 51, 19, WHITE);
    DrawText("Drag a card to a match; right-click to cancel", 20, 80, 15, LIGHTGRAY);
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

    const float scale = std::min(static_cast<float>(GetScreenWidth()) / TableLayout::ImageWidth,
                                 static_cast<float>(GetScreenHeight()) / TableLayout::ImageHeight);
    const float width = TableLayout::ImageWidth * scale;
    const float height = TableLayout::ImageHeight * scale;
    const Rectangle destination{(GetScreenWidth() - width) / 2.0f, (GetScreenHeight() - height) / 2.0f,
                                width, height};
    BeginDrawing();
    ClearBackground(BLACK);
    // A render texture is vertically inverted in Raylib.
    DrawTexturePro(canvas_.texture, {0, 0, static_cast<float>(canvas_.texture.width),
                                    -static_cast<float>(canvas_.texture.height)}, destination, {0, 0}, 0, WHITE);
    EndDrawing();
}
