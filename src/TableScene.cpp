#include "TableScene.hpp"

#include "TableLayout.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {
using TableLayout::Rect;

void Vertex(float x, float y, float u, float v) {
    const auto point = TableLayout::OnPlane(x, y, TableLayout::CardElevation);
    rlTexCoord2f(u, v);
    rlVertex3f(point.x, point.y, point.z);
}

// CCW from above: top-left, bottom-left, bottom-right, top-right. The quad is
// on the 3D plane, so OpenGL performs perspective-correct texture interpolation.
void Quad(Rect r, float u0, float v0, float u1, float v1, Color color) {
    rlColor4ub(color.r, color.g, color.b, color.a);
    Vertex(r.x, r.y, u0, v0);
    Vertex(r.x, r.y + r.height, u0, v1);
    Vertex(r.x + r.width, r.y + r.height, u1, v1);
    Vertex(r.x + r.width, r.y, u1, v0);
}

void DrawBacks() {
    rlSetTexture(rlGetTextureIdDefault()); // Opaque white texture, tinted solid black.
    rlBegin(RL_QUADS);
    for (const Rect card : TableLayout::OpponentHand) Quad(card, 0, 0, 1, 1, BLACK);
    Quad(TableLayout::Stock, 0, 0, 1, 1, BLACK);
    rlEnd();
}

void DrawFaces(Texture2D atlas) {
    rlSetTexture(atlas.id);
    rlBegin(RL_QUADS);
    const auto draw = [](Rect r, int id) {
        const float u0 = static_cast<float>(id / 4) / 12.0f;
        const float v0 = static_cast<float>(id % 4) / 4.0f;
        Quad(r, u0, v0, u0 + 1.0f / 12.0f, v0 + 1.0f / 4.0f, WHITE);
    };
    for (std::size_t i = 0; i < TableLayout::PreviewFieldSlots.size(); ++i)
        draw(TableLayout::Field[TableLayout::PreviewFieldSlots[i]], TableLayout::PreviewFieldCards[i]);
    for (std::size_t i = 0; i < TableLayout::PlayerHand.size(); ++i)
        draw(TableLayout::PlayerHand[i], TableLayout::PreviewHandCards[i]);
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

void TableScene::Draw() const {
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
    DrawBacks();
    DrawFaces(atlas_);
    EndMode3D();
    const std::string label = "TABLE PREVIEW  |  " + std::to_string(TableLayout::MatchRounds) + " ROUNDS";
    DrawText(label.c_str(), 1005, 22, 20, WHITE);
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
