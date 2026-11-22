#pragma once
#include <raylib.h>

struct TableRay { Vector3 origin{}, direction{}; };

// Fixed-camera projection helpers. These functions never modify the camera.
namespace TableView {
[[nodiscard]] Camera3D DefaultOverview();
[[nodiscard]] Rectangle CanvasViewport(float width, float height) noexcept;
[[nodiscard]] bool WindowToCanvas(Vector2 windowPoint, Rectangle viewport, Vector2& canvasPoint) noexcept;
[[nodiscard]] TableRay ScreenRay(const Camera3D& camera, Vector2 canvasPoint, float width, float height) noexcept;
[[nodiscard]] bool IntersectPlane(const TableRay& ray, float height, Vector3& hit) noexcept;
[[nodiscard]] bool ProjectPoint(const Camera3D& camera, Vector3 point, float width, float height, Vector2& screen) noexcept;
}
