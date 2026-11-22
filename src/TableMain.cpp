#include "TableGame.hpp"
#include "TableScene.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

namespace {
TableLayout::Point2 CanvasPoint(Vector2 mouse) {
    const float scale = std::min(static_cast<float>(GetScreenWidth()) / TableLayout::ImageWidth,
                                 static_cast<float>(GetScreenHeight()) / TableLayout::ImageHeight);
    return {(mouse.x - (GetScreenWidth() - TableLayout::ImageWidth * scale) / 2.0f) / scale,
            (mouse.y - (GetScreenHeight() - TableLayout::ImageHeight * scale) / 2.0f) / scale};
}
bool In(TableLayout::Point2 p, float x, float y, float w, float h) {
    return p.x >= x && p.x <= x+w && p.y >= y && p.y <= y+h;
}
// Maps the unzoomed canvas point through the visible (cropped) world source.
TableLayout::Point2 TablePoint(const TableScene& scene, TableLayout::Point2 canvas) {
    const Rectangle source = scene.SourceRect();
    return {source.x + canvas.x / TableLayout::ImageWidth * source.width,
            source.y + canvas.y / TableLayout::ImageHeight * source.height};
}
TableLayout::Point2 PlanPoint(TableLayout::Point2 canvas) {
    auto p = TableLayout::Unproject(canvas);
    p.x = std::clamp(p.x, -200.0f, TableLayout::PlanWidth + 200.0f);
    p.y = std::clamp(p.y, -200.0f, TableLayout::PlanHeight + 200.0f);
    return p;
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
        TableScene scene(background.c_str(), atlas.c_str());
        TableGame game;
        while (!WindowShouldClose()) {
            const Vector2 mouse = GetMousePosition();
            const auto canvas = CanvasPoint(mouse);
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
                } else game.PointerDown(PlanPoint(TablePoint(scene, canvas)));
            }
            if (game.Dragging()) game.PointerMove(PlanPoint(TablePoint(scene, canvas)));
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) game.PointerUp(PlanPoint(TablePoint(scene, canvas)));
            const float dt = std::min(GetFrameTime(), 0.1f);
            game.Update(dt);
            scene.Update(game, dt);
            scene.Draw(game);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        result = 1;
    }
    CloseWindow();
    return result;
}
