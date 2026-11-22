#include "TableGame.hpp"
#include "TableScene.hpp"
#include "CardInspection.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

namespace {
Vector2 CanvasPoint(Vector2 mouse) {
    const Rectangle viewport = TableView::CanvasViewport(static_cast<float>(GetScreenWidth()),
                                                          static_cast<float>(GetScreenHeight()));
    Vector2 point{};
    if (!TableView::WindowToCanvas(mouse, viewport, point)) return {-1, -1};
    return point;
}
bool In(Vector2 p, float x, float y, float w, float h) {
    return p.x >= x && p.x <= x+w && p.y >= y && p.y <= y+h;
}
bool PlanPoint(Vector2 canvas, const Camera3D& camera, TableLayout::Point2& result) {
    Vector3 hit{};
    const auto ray = TableView::ScreenRay(camera, canvas, TableLayout::ImageWidth, TableLayout::ImageHeight);
    if (!TableView::IntersectPlane(ray, TableLayout::CardElevation, hit)) return false;
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
#ifdef DENSHA_TEST_SEED
        TableGame game(DENSHA_TEST_SEED);
#else
        TableGame game;
#endif
        const Camera3D camera = TableView::DefaultOverview();
        CardPresentation presentation;
        while (!WindowShouldClose()) {
            const float dt = std::max(0.0f, GetFrameTime());
            const Vector2 mouse = GetMousePosition();
            const Vector2 cursor = CanvasPoint(mouse);
            const auto viewport = TableView::CanvasViewport(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
            const float browserX = mouse.x - CardInspection::BrowserLeft(viewport.x, viewport.width) + CardInspection::Left;
            const float browserY = mouse.y - CardInspection::PanelTop(viewport.y, viewport.width) + CardInspection::Top;
            auto cards = game.CardVisuals();
            presentation.Update(cards, camera, cursor, dt);
            const auto& ownCapture = game.Rules().Captured(Player::PLAYER);
            const auto& cpuCapture = game.Rules().Captured(Player::OPPONENT);
            const bool browserOpen = !ownCapture.empty() || !cpuCapture.empty();
            if (browserOpen && CardInspection::InBrowser(browserX, browserY))
                presentation.InspectCaptured(CardInspection::CapturedAt(ownCapture, cpuCapture, browserX, browserY));
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && game.Dragging()) {
                game.CancelDrag(presentation.DragVisualRect(cards, camera, cursor));
                presentation.EndDrag();
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (game.Rules().GetPhase() == Phase::ROUND_END && In(cursor, 1000, 20, 310, 66)) {
                    if (game.MatchFinished()) game = TableGame();
                    else game.NextRound();
                    presentation = CardPresentation{};
                } else if (game.Rules().GetPhase() == Phase::AWAIT_DECISION && !game.Busy() &&
                           game.Rules().CurrentPlayer() == Player::PLAYER && In(cursor, 1010, 20, 145, 40)) {
                    game.Choose(Decision::KOI_KOI);
                } else if (game.Rules().GetPhase() == Phase::AWAIT_DECISION && !game.Busy() &&
                           game.Rules().CurrentPlayer() == Player::PLAYER && In(cursor, 1165, 20, 145, 40)) {
                    game.Choose(Decision::SHOUBU);
                } else if ((!browserOpen || !CardInspection::InBrowser(browserX, browserY)) &&
                           In(cursor, 0, 0, TableLayout::ImageWidth, TableLayout::ImageHeight)) {
                    const auto hit = presentation.Pick(cards, camera, cursor);
                    if (hit && hit->card.zone == TableCardZone::PLAYER_HAND) {
                        // Pick the actual tilted surface, but start gameplay in
                        // its equivalent flat, local card coordinates.
                        const auto r = hit->card.rect;
                        game.PointerDown({r.x + r.width*hit->u, r.y + r.height*hit->v});
                        if (game.Dragging()) presentation.BeginDrag(*hit);
                    } else {
                        TableLayout::Point2 plan{};
                        if (PlanPoint(cursor, camera, plan)) game.PointerDown(plan);
                    }
                }
            }
            if (game.Dragging()) {
                TableLayout::Point2 plan{};
                const bool hit = PlanPoint(cursor, camera, plan);
                if (hit) game.PointerMove(plan);
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    cards = game.CardVisuals();
                    const auto visual = presentation.DragVisualRect(cards, camera, cursor);
                    const bool onCanvas = In(cursor, 0, 0, TableLayout::ImageWidth, TableLayout::ImageHeight);
                    const bool onBrowser = browserOpen && CardInspection::InBrowser(browserX, browserY);
                    if (hit && onCanvas && !onBrowser) game.PointerUp(plan, visual);
                    else game.CancelDrag(visual);
                    presentation.EndDrag();
                }
            }
            game.Update(std::min(dt, 0.1f));
            scene.Draw(game, camera, presentation, cursor);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        result = 1;
    }
    CloseWindow();
    return result;
}
