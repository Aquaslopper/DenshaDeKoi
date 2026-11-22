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

// Camera presets and motion, expressed relative to the calibrated overview
// camera in TableLayout so the two can never drift apart.
constexpr float kFocusDolly = 0.18f;   // 18% of the 1-unit camera->target distance
constexpr float kFocusAimDrop = 0.02f; // leans the frame down; keeps the near hand in view
constexpr float kParallax = 0.02f;     // max target offset in world units (~14 px of drift)

const float PitchSin = std::sin(TableLayout::PitchRadians);
const float PitchCos = std::cos(TableLayout::PitchRadians);
const Vector3 View{0.0f, -PitchSin, -PitchCos}; // unit forward vector of the overview camera
const Vector3 CAM_OVERVIEW_POS{0.0f, TableLayout::CameraHeight, 0.0f};
const Vector3 CAM_OVERVIEW_TARGET{0.0f, TableLayout::CameraHeight - PitchSin, -PitchCos};

Vector3 Slide(Vector3 v, float distance) {
    return {v.x + View.x * distance, v.y + View.y * distance, v.z + View.z * distance};
}

// A dolly-only preset would push the near hand about 20 px past the bottom
// edge, so the target drops by a hair (1.1 degrees) as the camera leans in.
const Vector3 CAM_FOCUSED_POS = Slide(CAM_OVERVIEW_POS, kFocusDolly);
const Vector3 CAM_FOCUSED_TARGET{CAM_OVERVIEW_TARGET.x + View.x * kFocusDolly,
                                 CAM_OVERVIEW_TARGET.y + View.y * kFocusDolly - kFocusAimDrop,
                                 CAM_OVERVIEW_TARGET.z + View.z * kFocusDolly};

Vector3 Lerp(Vector3 a, Vector3 b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}
Vector3 Sub(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vector3 Cross(Vector3 a, Vector3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vector3 Normalized(Vector3 v) {
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return {v.x / length, v.y / length, v.z / length};
}

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
    camera_ = {CAM_OVERVIEW_POS, CAM_OVERVIEW_TARGET, {0.0f, 1.0f, 0.0f},
               TableLayout::FovDegrees, CAMERA_PERSPECTIVE};
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

void TableScene::Update(const TableGame& game, float dt, Vector2 mouse) {
    const bool playerTurn = game.Rules().GetPhase() != Phase::ROUND_END &&
                            game.Rules().CurrentPlayer() == Player::PLAYER;
    if (!game.Dragging()) pointer_ = mouse;
    Vector3 position = playerTurn ? CAM_FOCUSED_POS : CAM_OVERVIEW_POS;
    Vector3 target = playerTurn ? CAM_FOCUSED_TARGET : CAM_OVERVIEW_TARGET;
    if (playerTurn) {
        // Look a hair toward the pointer: a depth cue, not a pan. The offset
        // magnitude is capped at kParallax so corner pointers stay subtle.
        const float length = std::sqrt(pointer_.x * pointer_.x + pointer_.y * pointer_.y);
        const float scale = length > 1.0f ? kParallax / length : kParallax;
        target.x += pointer_.x * scale;
        target.y -= pointer_.y * scale;
    }
    const float blend = 1.0f - std::exp(-8.0f * dt);
    camera_.position = Lerp(camera_.position, position, blend);
    camera_.target = Lerp(camera_.target, target, blend);
}

TableLayout::Point2 TableScene::PlanPoint(TableLayout::Point2 canvas) const {
    // Inverse of the live perspective camera; identical to
    // TableLayout::Unproject() while the camera sits at CAM_OVERVIEW.
    const Vector3 forward = Normalized(Sub(camera_.target, camera_.position));
    const Vector3 right = Normalized(Cross(forward, camera_.up));
    const Vector3 up = Cross(right, forward);
    const float focal = TableLayout::ImageHeight * 0.5f /
                        std::tan(TableLayout::FovDegrees * 0.5f * 0.017453292519943295f);
    const float sx = (canvas.x - TableLayout::ImageWidth * 0.5f) / focal;
    const float sy = (canvas.y - TableLayout::ImageHeight * 0.5f) / focal;
    const Vector3 ray{forward.x + right.x * sx - up.x * sy,
                      forward.y + right.y * sx - up.y * sy,
                      forward.z + right.z * sx - up.z * sy};
    if (ray.y > -1e-5f) return {-10000.0f, -10000.0f};
    const float t = (TableLayout::CardElevation - camera_.position.y) / ray.y;
    const float x = camera_.position.x + ray.x * t;
    const float z = camera_.position.z + ray.z * t;
    return {(x + TableLayout::PlaneHalfWidth) * TableLayout::PlanWidth /
                (2.0f * TableLayout::PlaneHalfWidth),
            (z + TableLayout::FarDepth) * TableLayout::PlanHeight /
                (TableLayout::FarDepth - TableLayout::NearDepth)};
}

void TableScene::Draw(const TableGame& game) const {
    BeginTextureMode(canvas_);
    ClearBackground(BLACK);
    DrawTexturePro(background_, {0, 0, static_cast<float>(background_.width), static_cast<float>(background_.height)},
                   {0, 0, static_cast<float>(TableLayout::ImageWidth), static_cast<float>(TableLayout::ImageHeight)},
                   {0, 0}, 0, WHITE);
    BeginMode3D(camera_);
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
