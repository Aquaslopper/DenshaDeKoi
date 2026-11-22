#include "TableGame.hpp"
#include "TableScene.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

namespace {
TableLayout::Point2 CanvasPoint(Vector2 mouse) {
    const Rectangle viewport = TableCameraController::CanvasViewport(static_cast<float>(GetScreenWidth()),
                                                                      static_cast<float>(GetScreenHeight()));
    Vector2 point{};
    if (!TableCameraController::WindowToCanvas(mouse, viewport, point)) return {-1, -1};
    return {point.x, point.y};
}
bool In(TableLayout::Point2 p, float x, float y, float w, float h) {
    return p.x >= x && p.x <= x+w && p.y >= y && p.y <= y+h;
}
bool PlanPoint(TableLayout::Point2 canvas, const Camera3D& camera, float height, TableLayout::Point2& result) {
    Vector3 hit{};
    const auto ray = TableCameraController::ScreenRay(camera, {canvas.x, canvas.y},
                                                      TableLayout::ImageWidth, TableLayout::ImageHeight);
    if (!TableCameraController::IntersectPlane(ray, height, hit)) return false;
    result = TableLayout::FromPlane({hit.x, hit.y, hit.z});
    return true;
}
} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(TableLayout::ImageWidth, TableLayout::ImageHeight, "Densha de Koi");
    if (!IsWindowReady()) {
        std::fprintf(stderr, "Could not create the window.\n");
        return 1;
    }
    SetWindowMinSize(680, 384);
    SetTargetFPS(60);

    int result = 0;
    try {
        const std::filesystem::path appDir(GetApplicationDirectory());
        const std::string background = (appDir / "the-city.jpg").string();
        const std::string atlas = (appDir / "cards_atlas.png").string();
        const TableScene scene(background.c_str(), atlas.c_str());
        TableGame game;
        Camera3D camera = TableCameraController::DefaultOverview();
        TableCameraController cameraController(camera);
        CameraSettings cameraSettings;
        while (!WindowShouldClose()) {
            const float dt = std::max(0.0f, GetFrameTime());
            const auto canvas = CanvasPoint(GetMousePosition());
            if (IsKeyPressed(KEY_Z)) cameraSettings.enableDynamicZoom = !cameraSettings.enableDynamicZoom;
            if (IsKeyPressed(KEY_P)) cameraSettings.enableParallax = !cameraSettings.enableParallax;
            if (IsKeyPressed(KEY_LEFT_BRACKET)) cameraSettings.dollyIntensity = std::max(0.0f, cameraSettings.dollyIntensity - 0.1f);
            if (IsKeyPressed(KEY_RIGHT_BRACKET)) cameraSettings.dollyIntensity = std::min(1.0f, cameraSettings.dollyIntensity + 0.1f);
            const Phase phase = game.Rules().GetPhase();
            const TableCameraContext cameraContext{
                game.Rules().CurrentPlayer() == Player::PLAYER, game.Dragging(),
                phase == Phase::AWAIT_DECISION || phase == Phase::AWAIT_DRAW_CHOICE,
                phase == Phase::ROUND_END || game.MatchFinished(),
                {2.0f * canvas.x / TableLayout::ImageWidth - 1.0f,
                 2.0f * canvas.y / TableLayout::ImageHeight - 1.0f}};
            // Update once before picking; this exact camera is also used to draw.
            cameraController.Update(camera, dt, cameraContext, cameraSettings);
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) game.CancelDrag();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (game.Rules().GetPhase() == Phase::ROUND_END && In(canvas, 1000, 20, 310, 66)) {
                    if (game.MatchFinished()) game = TableGame();
                    else game.NextRound();
                } else if (game.Rules().GetPhase() == Phase::AWAIT_DECISION && !game.Busy() &&
                           game.Rules().CurrentPlayer() == Player::PLAYER && In(canvas, 1010, 20, 145, 40)) {
                    game.Choose(Decision::KOI_KOI);
                } else if (game.Rules().GetPhase() == Phase::AWAIT_DECISION && !game.Busy() &&
                           game.Rules().CurrentPlayer() == Player::PLAYER && In(canvas, 1165, 20, 145, 40)) {
                    game.Choose(Decision::SHOUBU);
                } else if (In(canvas, 0, 0, TableLayout::ImageWidth, TableLayout::ImageHeight)) {
                    TableLayout::Point2 plan{};
                    if (PlanPoint(canvas, camera, TableLayout::CardElevation, plan)) {
                        game.PointerDown(plan);
                        // The dragged card is raised slightly; retain its local grab point
                        // against the plane where it is actually rendered.
                        if (game.Dragging() && PlanPoint(canvas, camera, TableLayout::DragCardElevation, plan))
                            game.ReanchorDrag(plan);
                    }
                }
            }
            if (game.Dragging()) {
                TableLayout::Point2 plan{};
                const bool hit = PlanPoint(canvas, camera, TableLayout::DragCardElevation, plan);
                if (hit) game.PointerMove(plan);
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    if (hit) game.PointerUp(plan);
                    else game.CancelDrag();
                }
            }
            game.Update(std::min(dt, 0.1f));
            scene.Draw(game, camera, cameraController, cameraSettings);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        result = 1;
    }
    CloseWindow();
    return result;
}
