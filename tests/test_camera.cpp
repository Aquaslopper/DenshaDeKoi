#include "TableGame.hpp" // Card enums before Raylib's color macros.
#include "TableCameraController.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

namespace {
std::atomic<int> trackedAllocations{0};
bool trackAllocations = false;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool Near(float a, float b, float epsilon = 0.001f) { return std::abs(a-b) <= epsilon; }
float Distance(Vector3 a, Vector3 b) { return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)); }
Vector3 World(float x, float y, float elevation) { auto p = TableLayout::OnPlane(x, y, elevation); return {p.x,p.y,p.z}; }
Vector2 Project(const Camera3D& c, Vector3 p) {
    Vector2 result{};
    Check(TableCameraController::ProjectPoint(c, p, 1360, 768, result), "Projection failed");
    return result;
}
TableLayout::Point2 Pick(const Camera3D& c, Vector2 screen, float height) {
    Vector3 point{};
    Check(TableCameraController::IntersectPlane(TableCameraController::ScreenRay(c, screen, 1360, 768), height, point), "Ray failed");
    return TableLayout::FromPlane({point.x, point.y, point.z});
}
void TestKinematics() {
    const auto base = TableCameraController::DefaultOverview();
    CameraSettings settings; settings.enableParallax = false;
    TableCameraContext player; player.playerTurn = true;
    Camera3D thirty = base, oneFortyFour = base;
    TableCameraController a(base), b(base);
    for (int i = 0; i < 30; ++i) a.Update(thirty, 1.0f/30, player, settings);
    for (int i = 0; i < 144; ++i) b.Update(oneFortyFour, 1.0f/144, player, settings);
    Check(Distance(thirty.position, oneFortyFour.position) < 0.00001f &&
          Distance(thirty.target, oneFortyFour.target) < 0.00001f, "Camera is frame-rate dependent");
    Check(a.State() == TableCameraState::PLAYER_TURN && Near(thirty.fovy, base.fovy) &&
          thirty.position.y < base.position.y && thirty.position.z < base.position.z,
          "Player camera does not dolly with fixed FOV");
    Check(Near(Distance(thirty.position, base.position), 0.18f, 0.001f), "Player dolly not 18% of overview target distance");
    const float oldPitch = (base.position.y-base.target.y)/(base.position.z-base.target.z);
    const float newPitch = (thirty.position.y-thirty.target.y)/(thirty.position.z-thirty.target.z);
    Check(newPitch > oldPitch, "Player view must tilt downward");
    const auto oldA = Project(base, World(318, 184, TableLayout::CardElevation));
    const auto oldB = Project(base, World(393, 309, TableLayout::CardElevation));
    const auto newA = Project(thirty, World(318, 184, TableLayout::CardElevation));
    const auto newB = Project(thirty, World(393, 309, TableLayout::CardElevation));
    Check(newB.x-newA.x > (oldB.x-oldA.x) * 1.03f, "Field cards did not grow meaningfully on screen");
    const auto oldTopRight = Project(base, World(393, 184, TableLayout::CardElevation));
    const auto newTopRight = Project(thirty, World(393, 184, TableLayout::CardElevation));
    Check(newTopRight.x-newA.x > (oldTopRight.x-oldA.x) * 1.03f,
          "Field card top edge did not become wider");
    for (const auto& r : TableLayout::PlayerHand) for (float x : {r.x, r.x+r.width}) for (float y : {r.y, r.y+r.height}) {
        const auto p = Project(thirty, World(x, y, TableLayout::CardElevation));
        Check(p.x >= 0 && p.x <= 1360 && p.y >= 0 && p.y <= 768, "Player camera clipped a hand card");
    }
    player.dragging = true;
    for (int i = 0; i < 100; ++i) a.Update(thirty, 1.0f/60, player, settings);
    Check(a.State() == TableCameraState::DRAGGING && Near(Distance(thirty.position, base.position), 0.23f, 0.001f), "Dragging dolly wrong");
    player.decision = true; a.Update(thirty, 0.1f, player, settings);
    Check(a.State() == TableCameraState::DECISION, "Decision must take priority");
    player.roundTransition = true; a.Update(thirty, 0.1f, player, settings);
    Check(a.State() == TableCameraState::OVERVIEW, "Round transition must use overview");
    player.roundTransition = false; player.playerTurn = false;
    for (int i = 0; i < 100; ++i) a.Update(thirty, 1.0f/60, player, settings);
    Check(a.State() == TableCameraState::OVERVIEW && Distance(thirty.position, base.position) < 0.00001f, "CPU did not return to overview");

    player.playerTurn = true; player.decision = false; player.dragging = false;
    settings.enableParallax = true; player.mouseNormalized = {10, -10};
    for (int i = 0; i < 100; ++i) a.Update(thirty, 1.0f/60, player, settings);
    Check(std::abs(thirty.position.x) <= 0.013f && std::abs(thirty.position.x) > 0.01f, "Parallax not small or clamped");
    player.dragging = true;
    for (int i = 0; i < 100; ++i) a.Update(thirty, 1.0f/60, player, settings);
    const Camera3D dragAtEdge = thirty;
    player.mouseNormalized = {-10, 10};
    for (int i = 0; i < 100; ++i) a.Update(thirty, 1.0f/60, player, settings);
    Check(Distance(thirty.position, dragAtEdge.position) < 0.00001f &&
          Distance(thirty.target, dragAtEdge.target) < 0.00001f, "Dragging should not apply mouse parallax");
    player.dragging = false;
    settings.enableDynamicZoom = false;
    for (int i = 0; i < 100; ++i) a.Update(thirty, 1.0f/60, player, settings);
    Check(a.State() == TableCameraState::OVERVIEW && Distance(thirty.position, base.position) < 0.00001f &&
          Distance(thirty.target, base.target) < 0.00001f, "Disabled camera did not return to fixed overview");
    settings.enableDynamicZoom = true; settings.dollyIntensity = 0;
    for (int i = 0; i < 60; ++i) a.Update(thirty, 1.0f/60, player, settings);
    Check(a.State() == TableCameraState::OVERVIEW && Distance(thirty.position, base.position) < 0.00001f, "Zero intensity must be fixed");
    Check(TableCameraController::ExponentialBlend(11.5f, -1) == 0 &&
          TableCameraController::ExponentialBlend(11.5f, std::numeric_limits<float>::infinity()) == 0, "Invalid dt handling");

    settings.dollyIntensity = 1;
    TableGame game(42);
    char status[128];
    trackedAllocations = 0; trackAllocations = true;
    for (int i = 0; i < 500; ++i) {
        a.Update(thirty, 1.0f/60, player, settings);
        (void)a.BackgroundUv(thirty, {650, 500});
        (void)game.CardVisuals();
        game.FormatStatus(status, sizeof(status));
    }
    trackAllocations = false;
    Check(trackedAllocations == 0, "Camera or frame presentation allocates per frame");
}
void TestViewportAndSettling() {
    for (auto window : {Vector2{1600,900}, Vector2{800,1000}, Vector2{1360,768}}) {
        const auto viewport = TableCameraController::CanvasViewport(window.x, window.y);
        Vector2 p{};
        Check(TableCameraController::WindowToCanvas({viewport.x, viewport.y}, viewport, p) && Near(p.x, 0) && Near(p.y, 0),
              "Letterboxed top left mapping failed");
        Check(TableCameraController::WindowToCanvas({viewport.x+viewport.width, viewport.y+viewport.height}, viewport, p) &&
              Near(p.x, 1360) && Near(p.y, 768), "Letterboxed bottom right mapping failed");
        Check(TableCameraController::WindowToCanvas({viewport.x-10, viewport.y}, viewport, p) && p.x < 0,
              "Outside drag should remain outside canvas");
    }
    Vector2 ignored{};
    Check(!TableCameraController::WindowToCanvas({0,0}, TableCameraController::CanvasViewport(0,0), ignored),
          "Zero-sized window should not pick");
    const auto base = TableCameraController::DefaultOverview();
    Camera3D camera = base; TableCameraController controller(base);
    CameraSettings settings; settings.enableParallax = false;
    TableCameraContext context; context.playerTurn = true;
    controller.Update(camera, 0.26f, context, settings);
    Check(Distance(camera.position, base.position) / 0.18f > 0.94f, "Player transition settles too slowly");
    for (int i = 0; i < 120; ++i) controller.Update(camera, 1.0f/60, context, settings);
    const auto beforeDrag = camera;
    context.dragging = true;
    controller.Update(camera, 0.15f, context, settings);
    Check(Distance(camera.position, beforeDrag.position) / 0.05f > 0.94f, "Drag transition settles too slowly");
}
void TestRaysAndBackdrop() {
    auto base = TableCameraController::DefaultOverview();
    Camera3D moving = base; TableCameraController controller(base);
    TableCameraContext ctx; ctx.playerTurn = true; ctx.dragging = true; ctx.mouseNormalized = {0.8f, -0.4f};
    for (int i = 0; i < 100; ++i) controller.Update(moving, 1.0f/60, ctx, CameraSettings{});
    for (const auto& c : {base, moving}) for (float y : {190.f, 340.f, 590.f, 700.f}) for (float x : {100.f, 420.f, 750.f}) {
        auto world = World(x, y, TableLayout::CardElevation);
        auto screen = Project(c, world);
        auto picked = Pick(c, screen, TableLayout::CardElevation);
        Check(Near(picked.x, x, 0.01f) && Near(picked.y, y, 0.01f), "Ray-plane round trip failed");
    }
    Vector3 hit{};
    Check(!TableCameraController::IntersectPlane({{0,1,0}, {1,0,0}}, 0, hit), "Parallel ray accepted");
    Check(!TableCameraController::IntersectPlane({{0,1,0}, {0,1,0}}, 0, hit), "Backwards ray accepted");
    Check(!TableCameraController::IntersectPlane({{0,1,0}, {0,std::numeric_limits<float>::quiet_NaN(),0}}, 0, hit), "NaN ray accepted");
    for (float y : {200.f, 360.f, 500.f, 650.f}) for (float x : {300.f, 420.f, 550.f}) {
        const auto world = World(x, y, 0);
        const auto screen = Project(moving, world), overview = Project(base, world);
        auto uv = controller.BackgroundUv(moving, screen);
        Check(screen.y >= 170 && overview.x >= 0 && overview.x <= 1360 && overview.y >= 0 && overview.y <= 768,
              "Invalid backdrop test fixture");
        Check(Near(uv.x * 1360, overview.x, 0.02f) && Near(uv.y * 768, overview.y, 0.02f),
              "Moving table backdrop not aligned with world plane");
    }
}
void TestBackdropMesh() {
    auto base = TableCameraController::DefaultOverview();
    auto camera = base; TableCameraController controller(base);
    TableCameraContext ctx; ctx.playerTurn = true; ctx.dragging = true;
    for (int i = 0; i < 120; ++i) controller.Update(camera, 1.0f / 60.0f, ctx, CameraSettings{});
    // Sample both triangle halves of each 64x48 screen-space cell. Compare the
    // GPU's interpolated UV against the analytic plane mapping in the table area.
    float maxError = 0;
    for (int y = 11; y < 48; ++y) for (int x = 0; x < 64; ++x) {
        const float x0 = 1360.0f*x/64, x1 = 1360.0f*(x+1)/64;
        const float y0 = 768.0f*y/48, y1 = 768.0f*(y+1)/48;
        const auto a = controller.BackgroundUv(camera, {x0,y0});
        const auto b = controller.BackgroundUv(camera, {x0,y1});
        const auto c = controller.BackgroundUv(camera, {x1,y1});
        const auto d = controller.BackgroundUv(camera, {x1,y0});
        for (auto fraction : {Vector2{0.25f,0.75f}, Vector2{0.75f,0.25f}}) {
            const float u = fraction.x, v = fraction.y;
            const auto actual = controller.BackgroundUv(camera, {x0+(x1-x0)*u, y0+(y1-y0)*v});
            const float iu = v >= u ? a.x*(1-v)+b.x*(v-u)+c.x*u : a.x*(1-u)+c.x*v+d.x*(u-v);
            const float iv = v >= u ? a.y*(1-v)+b.y*(v-u)+c.y*u : a.y*(1-u)+c.y*v+d.y*(u-v);
            maxError = std::max(maxError, std::hypot((actual.x-iu)*1360, (actual.y-iv)*768));
        }
    }
    Check(maxError < 1.0f, "Background mesh undersamples the tabletop warp");
}
void TestMovingGrab() {
    const auto base = TableCameraController::DefaultOverview();
    for (unsigned seed = 1; seed < 1000; ++seed) {
        TableGame game(seed);
        if (game.Rules().GetPhase() != Phase::AWAIT_HAND_PLAY || game.Rules().CurrentPlayer() != Player::PLAYER) continue;
        const auto r = TableLayout::PlayerHand[0];
        Camera3D camera = base; TableCameraController controller(base); TableCameraContext ctx; ctx.playerTurn = true;
        const Vector2 cursor = Project(camera, World(r.x + r.width*0.35f, r.y + r.height*0.30f, TableLayout::CardElevation));
        auto firstPick = Pick(camera, cursor, TableLayout::CardElevation);
        game.PointerDown(firstPick); Check(game.Dragging(), "Failed to grab card");
        auto raisedPick = Pick(camera, cursor, TableLayout::DragCardElevation);
        game.ReanchorDrag(raisedPick);
        const float offsetX = raisedPick.x - r.x, offsetY = raisedPick.y - r.y;
        ctx.dragging = true; ctx.mouseNormalized = {0.8f, -0.5f};
        for (int i = 0; i < 40; ++i) {
            const Vector2 pointer{cursor.x + static_cast<float>(i)*2.0f, cursor.y - static_cast<float>(i)};
            ctx.mouseNormalized = {pointer.x * 2.0f / 1360 - 1.0f, pointer.y * 2.0f / 768 - 1.0f};
            controller.Update(camera, 1.0f/60, ctx, CameraSettings{});
            auto picked = Pick(camera, pointer, TableLayout::DragCardElevation);
            game.PointerMove(picked);
            const auto card = game.Cards().back();
            const auto grab = Project(camera, World(card.rect.x + offsetX, card.rect.y + offsetY, TableLayout::DragCardElevation));
            Check(Near(grab.x, pointer.x, 0.01f) && Near(grab.y, pointer.y, 0.01f), "Card slipped under moving camera");
        }
        return;
    }
    throw std::runtime_error("No valid human turn fixture");
}
} // namespace

void* operator new(std::size_t n) { if (trackAllocations) ++trackedAllocations; if (void* p = std::malloc(n == 0 ? 1 : n)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { if (trackAllocations) ++trackedAllocations; if (void* p = std::malloc(n == 0 ? 1 : n)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main() {
    try { TestKinematics(); TestViewportAndSettling(); TestRaysAndBackdrop(); TestBackdropMesh(); TestMovingGrab(); std::cout << "Camera controller and ray picking passed.\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
