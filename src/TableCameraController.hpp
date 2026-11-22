#pragma once

#include <raylib.h>

// Presentation-only camera controls. No Raylib types are used by hanafuda_core.
struct CameraSettings {
    bool enableDynamicZoom = true;
    bool enableParallax = true;
    float dollyIntensity = 1.0f; // Clamped to [0, 1]; zero returns to overview.
};

enum class TableCameraState { OVERVIEW, PLAYER_TURN, DRAGGING, DECISION };

struct TableCameraContext {
    bool playerTurn = false;
    bool dragging = false;
    bool decision = false;
    bool roundTransition = false;
    // Raw cursor position normalized to the canvas, [-1, 1] on each axis.
    Vector2 mouseNormalized{};
};

struct TableRay { Vector3 origin{}, direction{}; };

class TableCameraController {
public:
    explicit TableCameraController(const Camera3D& overview);
    [[nodiscard]] static Camera3D DefaultOverview();
    void Update(Camera3D& camera, float dt, const TableCameraContext& context,
                const CameraSettings& settings);
    [[nodiscard]] TableCameraState State() const noexcept { return state_; }
    [[nodiscard]] const Camera3D& Overview() const noexcept { return overview_; }

    // exp(-lambda * dt) decay is independent of frame rate for a fixed target.
    [[nodiscard]] static float ExponentialBlend(float lambda, float dt) noexcept;
    [[nodiscard]] static Rectangle CanvasViewport(float windowWidth, float windowHeight) noexcept;
    // Returns false for an invalid/zero-sized viewport; points outside it still
    // map to outside canvas coordinates so an active drag can be cancelled.
    [[nodiscard]] static bool WindowToCanvas(Vector2 windowPoint, Rectangle viewport, Vector2& canvasPoint) noexcept;
    [[nodiscard]] static TableRay ScreenRay(const Camera3D& camera, Vector2 canvasPoint,
                                            float viewportWidth, float viewportHeight) noexcept;
    // Forward ray only: false for a parallel ray, a plane behind it, or invalid input.
    [[nodiscard]] static bool IntersectPlane(const TableRay& ray, float height, Vector3& hit) noexcept;
    [[nodiscard]] static bool ProjectPoint(const Camera3D& camera, Vector3 point,
                                           float viewportWidth, float viewportHeight, Vector2& result) noexcept;
    // Background texture coordinate for a canvas pixel. The table area uses a
    // ground-plane reprojection; the sky smoothly retains the original photo.
    [[nodiscard]] Vector2 BackgroundUv(const Camera3D& camera, Vector2 canvasPoint) const noexcept;

private:
    Camera3D overview_{};
    TableCameraState state_ = TableCameraState::OVERVIEW;
};
