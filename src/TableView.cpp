#include "TableView.hpp"

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

Camera3D TableView::DefaultOverview() {
    return {{0, TableLayout::CameraHeight, 0},
            {0, TableLayout::CameraHeight - std::sin(TableLayout::PitchRadians),
             -std::cos(TableLayout::PitchRadians)},
            {0, 1, 0}, TableLayout::FovDegrees, CAMERA_PERSPECTIVE};
}
Rectangle TableView::CanvasViewport(float windowWidth, float windowHeight) noexcept {
    if (!std::isfinite(windowWidth) || !std::isfinite(windowHeight) || windowWidth <= 0 || windowHeight <= 0) return {};
    const float scale = std::min(windowWidth / TableLayout::ImageWidth, windowHeight / TableLayout::ImageHeight);
    const float width = TableLayout::ImageWidth * scale, height = TableLayout::ImageHeight * scale;
    return {(windowWidth - width) * 0.5f, (windowHeight - height) * 0.5f, width, height};
}

bool TableView::WindowToCanvas(Vector2 point, Rectangle viewport, Vector2& result) noexcept {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(viewport.x) ||
        !std::isfinite(viewport.y) || !std::isfinite(viewport.width) || !std::isfinite(viewport.height) ||
        viewport.width <= 0 || viewport.height <= 0) return false;
    result = {(point.x - viewport.x) * TableLayout::ImageWidth / viewport.width,
              (point.y - viewport.y) * TableLayout::ImageHeight / viewport.height};
    return std::isfinite(result.x) && std::isfinite(result.y);
}

TableRay TableView::ScreenRay(const Camera3D& camera, Vector2 point,
                                           float width, float height) noexcept {
    if (width <= 0 || height <= 0 || !std::isfinite(width) || !std::isfinite(height) ||
        !std::isfinite(point.x) || !std::isfinite(point.y)) return {camera.position, {}};
    const Basis b = MakeBasis(camera, height);
    if (!std::isfinite(b.focal) || b.focal <= 0) return {camera.position, {}};
    const float x = (point.x - width * 0.5f) / b.focal;
    const float y = (height * 0.5f - point.y) / b.focal;
    return {camera.position, Add(b.forward, Add(Scale(b.right, x), Scale(b.up, y)))};
}

bool TableView::IntersectPlane(const TableRay& ray, float height, Vector3& hit) noexcept {
    if (!Finite(ray.origin) || !Finite(ray.direction) || !std::isfinite(height) || std::abs(ray.direction.y) < 1e-6f) return false;
    const float t = (height - ray.origin.y) / ray.direction.y;
    if (!std::isfinite(t) || t < 0) return false;
    const Vector3 result = Add(ray.origin, Scale(ray.direction, t));
    if (!Finite(result)) return false;
    hit = result;
    return true;
}

bool TableView::ProjectPoint(const Camera3D& camera, Vector3 point,
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
