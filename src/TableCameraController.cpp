#include "TableCameraController.hpp"

#include "TableLayout.hpp"

#include <algorithm>
#include <cmath>

namespace {
Vector3 Add(Vector3 a, Vector3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
Vector3 Sub(Vector3 a, Vector3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
Vector3 Scale(Vector3 v, float s) { return {v.x*s, v.y*s, v.z*s}; }
float Dot(Vector3 a, Vector3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vector3 Cross(Vector3 a, Vector3 b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
Vector3 Normalize(Vector3 v) {
    const float length = std::sqrt(Dot(v, v));
    return length > 1e-8f && std::isfinite(length) ? Scale(v, 1.0f / length) : Vector3{};
}
Vector3 Lerp(Vector3 a, Vector3 b, float t) { return Add(a, Scale(Sub(b, a), t)); }
float Unit(float v) { return std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f; }
bool Finite(Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
struct Basis { Vector3 forward, right, up; float focal; };
Basis MakeBasis(const Camera3D& c, float height) {
    const Vector3 forward = Normalize(Sub(c.target, c.position));
    const Vector3 right = Normalize(Cross(forward, c.up));
    const Vector3 up = Normalize(Cross(right, forward));
    const float focal = height * 0.5f / std::tan(c.fovy * 0.008726646259971648f);
    return {forward, right, up, focal};
}
} // namespace

Camera3D TableCameraController::DefaultOverview() {
    return {{0, TableLayout::CameraHeight, 0},
            {0, TableLayout::CameraHeight - std::sin(TableLayout::PitchRadians),
             -std::cos(TableLayout::PitchRadians)},
            {0, 1, 0}, TableLayout::FovDegrees, CAMERA_PERSPECTIVE};
}
TableCameraController::TableCameraController(const Camera3D& overview) : overview_(overview) {}

float TableCameraController::ExponentialBlend(float lambda, float dt) noexcept {
    if (!std::isfinite(lambda) || !std::isfinite(dt) || lambda <= 0 || dt <= 0) return 0;
    return std::clamp(-std::expm1(-lambda * dt), 0.0f, 1.0f);
}

Rectangle TableCameraController::CanvasViewport(float windowWidth, float windowHeight) noexcept {
    if (!std::isfinite(windowWidth) || !std::isfinite(windowHeight) || windowWidth <= 0 || windowHeight <= 0) return {};
    const float scale = std::min(windowWidth / TableLayout::ImageWidth, windowHeight / TableLayout::ImageHeight);
    const float width = TableLayout::ImageWidth * scale, height = TableLayout::ImageHeight * scale;
    return {(windowWidth - width) * 0.5f, (windowHeight - height) * 0.5f, width, height};
}

bool TableCameraController::WindowToCanvas(Vector2 point, Rectangle viewport, Vector2& result) noexcept {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(viewport.x) ||
        !std::isfinite(viewport.y) || !std::isfinite(viewport.width) || !std::isfinite(viewport.height) ||
        viewport.width <= 0 || viewport.height <= 0) return false;
    result = {(point.x - viewport.x) * TableLayout::ImageWidth / viewport.width,
              (point.y - viewport.y) * TableLayout::ImageHeight / viewport.height};
    return std::isfinite(result.x) && std::isfinite(result.y);
}

void TableCameraController::Update(Camera3D& camera, float dt, const TableCameraContext& context,
                                   const CameraSettings& settings) {
    state_ = TableCameraState::OVERVIEW;
    const float intensity = std::isfinite(settings.dollyIntensity) ? std::clamp(settings.dollyIntensity, 0.0f, 1.0f) : 0.0f;
    if (settings.enableDynamicZoom && intensity > 0 && !context.roundTransition && context.playerTurn) {
        state_ = context.decision ? TableCameraState::DECISION :
                 context.dragging ? TableCameraState::DRAGGING : TableCameraState::PLAYER_TURN;
    }

    // The calibrated overview's target is one world unit ahead. Dolly is an
    // actual translation along that view vector, never a change in FOV.
    const Vector3 view = Sub(overview_.target, overview_.position);
    float dolly = 0, tilt = 0, lambda = 11.5f;
    if (state_ == TableCameraState::PLAYER_TURN) { dolly = 0.18f; tilt = 0.075f; }
    else if (state_ == TableCameraState::DRAGGING) { dolly = 0.23f; tilt = 0.09f; lambda = 20.0f; }
    else if (state_ == TableCameraState::DECISION) { dolly = 0.12f; tilt = 0.06f; }
    Vector3 position = Add(overview_.position, Scale(view, dolly * intensity));
    Vector3 target = Add(position, view);
    target.y -= tilt * intensity;
    if (state_ == TableCameraState::DRAGGING) target.z -= 0.06f * intensity;

    // Millimetre-scale offsets relative to the ~1.8-unit table. They depend
    // only on raw screen coordinates, never on a picked card's world position.
    if (settings.enableParallax && intensity > 0 && state_ == TableCameraState::PLAYER_TURN) {
        const Vector3 offset{Unit(context.mouseNormalized.x) * 0.012f * intensity, 0,
                             Unit(context.mouseNormalized.y) * 0.006f * intensity};
        position = Add(position, offset); target = Add(target, offset);
    }
    const float blend = ExponentialBlend(lambda, dt);
    camera.position = Lerp(camera.position, position, blend);
    camera.target = Lerp(camera.target, target, blend);
    camera.up = overview_.up;
    camera.fovy = overview_.fovy;
    camera.projection = overview_.projection;
}

TableRay TableCameraController::ScreenRay(const Camera3D& camera, Vector2 point,
                                           float width, float height) noexcept {
    if (width <= 0 || height <= 0 || !std::isfinite(width) || !std::isfinite(height) ||
        !std::isfinite(point.x) || !std::isfinite(point.y)) return {camera.position, {}};
    const Basis b = MakeBasis(camera, height);
    if (!std::isfinite(b.focal) || b.focal <= 0) return {camera.position, {}};
    const float x = (point.x - width * 0.5f) / b.focal;
    const float y = (height * 0.5f - point.y) / b.focal;
    return {camera.position, Add(b.forward, Add(Scale(b.right, x), Scale(b.up, y)))};
}

bool TableCameraController::IntersectPlane(const TableRay& ray, float height, Vector3& hit) noexcept {
    if (!Finite(ray.origin) || !Finite(ray.direction) || !std::isfinite(height) || std::abs(ray.direction.y) < 1e-6f) return false;
    const float t = (height - ray.origin.y) / ray.direction.y;
    if (!std::isfinite(t) || t < 0) return false;
    const Vector3 result = Add(ray.origin, Scale(ray.direction, t));
    if (!Finite(result)) return false;
    hit = result;
    return true;
}

bool TableCameraController::ProjectPoint(const Camera3D& camera, Vector3 point,
                                          float width, float height, Vector2& result) noexcept {
    if (width <= 0 || height <= 0 || !std::isfinite(width) || !std::isfinite(height) || !Finite(point)) return false;
    const Basis b = MakeBasis(camera, height);
    const Vector3 delta = Sub(point, camera.position);
    const float depth = Dot(delta, b.forward);
    if (depth <= 1e-6f || !std::isfinite(depth) || !std::isfinite(b.focal) || b.focal <= 0) return false;
    result = {width * 0.5f + b.focal * Dot(delta, b.right) / depth,
              height * 0.5f - b.focal * Dot(delta, b.up) / depth};
    return std::isfinite(result.x) && std::isfinite(result.y);
}

Vector2 TableCameraController::BackgroundUv(const Camera3D& camera, Vector2 point) const noexcept {
    constexpr float width = static_cast<float>(TableLayout::ImageWidth);
    constexpr float height = static_cast<float>(TableLayout::ImageHeight);
    Vector3 world{}; Vector2 source = point;
    if (IntersectPlane(ScreenRay(camera, point, width, height), 0, world)) {
        Vector2 projected{};
        if (ProjectPoint(overview_, world, width, height, projected)) source = projected;
    }
    // The city above the table has no depth data. Blend to the unwarped photo
    // in the sky, keeping the entire tabletop region fully reprojected.
    float t = std::clamp((point.y - 70.0f) / 100.0f, 0.0f, 1.0f);
    t = t*t*(3.0f-2.0f*t);
    const float x = point.x + (source.x - point.x) * t;
    const float y = point.y + (source.y - point.y) * t;
    return {std::clamp(x / width, 0.0f, 1.0f), std::clamp(y / height, 0.0f, 1.0f)};
}
