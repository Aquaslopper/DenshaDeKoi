#pragma once

#include <array>
#include <cmath>
#include <cstddef>

// The placement reference is an 840 x 720 top-down plan. Its rectangles are
// mapped to an actual horizontal plane viewed through a perspective camera.
// Nothing here depends on Raylib, so gameplay and hit testing can reuse it.
namespace TableLayout {
inline constexpr int ImageWidth = 1360;
inline constexpr int ImageHeight = 768;
inline constexpr int MatchRounds = 3;
inline constexpr float PlanWidth = 840.0f;
inline constexpr float PlanHeight = 720.0f;

struct Rect { float x, y, width, height; };
struct Point3 { float x, y, z; };
struct Point2 { float x, y; };

// Camera and table plane calibrated against the supplied perspective mockup.
inline constexpr float CameraHeight = 1.0f;
inline constexpr float PitchRadians = 0.370f;
inline constexpr float FovDegrees = 45.0f;
inline constexpr float PlaneHalfWidth = 0.93f;
inline constexpr float NearDepth = 1.09f;
inline constexpr float FarDepth = 2.51f;
inline constexpr float CardElevation = 0.002f;

[[nodiscard]] inline Point3 OnPlane(float x, float y, float elevation = 0.0f) {
    return {-PlaneHalfWidth + 2.0f * PlaneHalfWidth * x / PlanWidth,
            elevation, -(FarDepth - (FarDepth - NearDepth) * y / PlanHeight)};
}

// Equivalent perspective projection, used by headless geometry tests.
[[nodiscard]] inline Point2 Project(Point3 p) {
    const float pitchSin = std::sin(PitchRadians);
    const float pitchCos = std::cos(PitchRadians);
    const float depth = -p.z;
    const float forward = (CameraHeight - p.y) * pitchSin + depth * pitchCos;
    const float vertical = (CameraHeight - p.y) * pitchCos - depth * pitchSin;
    const float focal = ImageHeight * 0.5f / std::tan(FovDegrees * 0.5f * 0.017453292519943295f);
    return {ImageWidth * 0.5f + focal * p.x / forward,
            ImageHeight * 0.5f + focal * vertical / forward};
}

[[nodiscard]] constexpr std::array<Rect, 8> MakeHand(float x, float y, float step,
                                                     float width, float height) {
    std::array<Rect, 8> result{};
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = {x + static_cast<float>(i) * step, y, width, height};
    return result;
}

inline constexpr auto OpponentHand = MakeHand(185.0f, 12.0f, 62.0f, 56.0f, 91.0f);
inline constexpr auto PlayerHand = MakeHand(49.0f, 567.0f, 96.0f, 88.0f, 142.0f);
inline constexpr Rect Stock{49.0f, 261.0f, 76.0f, 126.0f};

// Sixteen field positions for later gameplay, two rows of eight as in the plan.
[[nodiscard]] constexpr std::array<Rect, 16> MakeField() {
    std::array<Rect, 16> result{};
    for (std::size_t row = 0; row < 2; ++row)
        for (std::size_t col = 0; col < 8; ++col)
            result[row * 8 + col] = {156.0f + static_cast<float>(col) * 81.0f,
                                     184.0f + static_cast<float>(row) * 139.0f,
                                     75.0f, 125.0f};
    return result;
}
inline constexpr auto Field = MakeField();
// Static example only: the central four slots of each row contain example cards.
inline constexpr std::array<std::size_t, 8> PreviewFieldSlots{2, 3, 4, 5, 10, 11, 12, 13};
inline constexpr std::array<int, 8> PreviewFieldCards{0, 5, 8, 16, 20, 28, 32, 40};
inline constexpr std::array<int, 8> PreviewHandCards{1, 4, 12, 21, 24, 29, 36, 44};
} // namespace TableLayout
