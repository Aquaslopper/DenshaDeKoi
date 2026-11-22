#pragma once

#include <array>
#include <cmath>
#include <cstddef>

// The placement reference is the 1360 x 768 plan in assets/blueprints/README.md.
// Its rectangles are mapped to a horizontal plane viewed through a perspective
// camera fitted to the table quad of assets/blueprints/grid example-2.png.
// Nothing here depends on Raylib, so gameplay and hit testing can reuse it.
namespace TableLayout {
inline constexpr int ImageWidth = 1360;
inline constexpr int ImageHeight = 768;
inline constexpr int MatchRounds = 3;
inline constexpr float PlanWidth = 1360.0f;
inline constexpr float PlanHeight = 768.0f;

struct Rect { float x, y, width, height; };
struct Point3 { float x, y, z; };
struct Point2 { float x, y; };

// Camera fitted so the plan lands on the table quad of the projected blueprint:
// (417.8, 275.7) (979.5, 275.4) (1272.8, 744.3) (110.9, 739.4) at 1360x768.
// The lateral shift and yaw reproduce the blueprint's slight off-centre quad.
inline constexpr float PlaneHalfWidth = 0.799834f;
inline constexpr float CameraHeight = 0.843715f;
inline constexpr float PitchRadians = 0.227149f;
inline constexpr float YawRadians = -0.010617f;
inline constexpr float CameraShift = 0.012594f;
inline constexpr float NearDepth = 2.126004f;
inline constexpr float FarDepth = 4.605817f;
inline constexpr float FovDegrees = 18.503222f;
inline constexpr float CardElevation = 0.002f;

[[nodiscard]] inline Point3 OnPlane(float x, float y, float elevation = 0.0f) {
    return {-PlaneHalfWidth + 2.0f * PlaneHalfWidth * x / PlanWidth - CameraShift,
            elevation, -(FarDepth - (FarDepth - NearDepth) * y / PlanHeight)};
}

// Equivalent perspective projection, used by headless geometry tests.
[[nodiscard]] inline Point2 Project(Point3 p) {
    const float pitchSin = std::sin(PitchRadians), pitchCos = std::cos(PitchRadians);
    const float yawSin = std::sin(YawRadians), yawCos = std::cos(YawRadians);
    const float depth = -p.z;
    const float right = p.x * yawCos - depth * yawSin;
    const float ahead = p.x * yawSin + depth * yawCos;
    const float height = CameraHeight - p.y;
    const float forward = height * pitchSin + ahead * pitchCos;
    const float vertical = height * pitchCos - ahead * pitchSin;
    const float focal = ImageHeight * 0.5f / std::tan(FovDegrees * 0.5f * 0.017453292519943295f);
    return {ImageWidth * 0.5f + focal * right / forward,
            ImageHeight * 0.5f + focal * vertical / forward};
}

// Inverse of Project() for pointer hit testing on the elevated card plane.
[[nodiscard]] inline Point2 Unproject(Point2 screen, float elevation = CardElevation) {
    const float pitchSin = std::sin(PitchRadians), pitchCos = std::cos(PitchRadians);
    const float yawSin = std::sin(YawRadians), yawCos = std::cos(YawRadians);
    const float focal = ImageHeight * 0.5f / std::tan(FovDegrees * 0.5f * 0.017453292519943295f);
    const float height = CameraHeight - elevation;
    const float kx = (screen.x - ImageWidth * 0.5f) / focal;
    const float ky = (screen.y - ImageHeight * 0.5f) / focal;
    const float denominator = ky * pitchCos + pitchSin;
    if (std::abs(denominator) <= 0.0001f) return {-10000.0f, -10000.0f};
    const float ahead = height * (pitchCos - ky * pitchSin) / denominator;
    const float forward = height * pitchSin + ahead * pitchCos;
    const float right = kx * forward;
    const float worldX = right * yawCos + ahead * yawSin + CameraShift;
    const float depth = -right * yawSin + ahead * yawCos;
    return {(worldX + PlaneHalfWidth) * PlanWidth / (2.0f * PlaneHalfWidth),
            (FarDepth - depth) * PlanHeight / (FarDepth - NearDepth)};
}

// One card footprint for the whole table, taken from the plan.
inline constexpr float CardWidth = 62.1f;
inline constexpr float CardHeight = 101.6f;
inline constexpr float ColumnPitch = 70.02f;
inline constexpr float FirstColumn = 415.2f;

[[nodiscard]] constexpr Rect ColumnRect(std::size_t column, float y) {
    return {FirstColumn + static_cast<float>(column) * ColumnPitch, y, CardWidth, CardHeight};
}

[[nodiscard]] constexpr std::array<Rect, 8> MakeHand(std::size_t, float y) {
    std::array<Rect, 8> result{};
    for (std::size_t i = 0; i < result.size(); ++i) result[i] = ColumnRect(i, y);
    return result;
}

inline constexpr auto OpponentHand = MakeHand(0, 14.7f);
inline constexpr auto PlayerHand = MakeHand(0, 651.7f);

inline constexpr Rect Stock{261.6f, 295.9f, 87.0f, 141.2f};
inline constexpr Rect OpponentCaptures{261.6f, 140.0f, CardWidth, CardHeight};
inline constexpr Rect PlayerCaptures{261.6f, 500.0f, CardWidth, CardHeight};

// Two rows of the same eight columns as the hands, plus one spare row.
[[nodiscard]] constexpr std::array<Rect, 16> MakeField() {
    std::array<Rect, 16> result{};
    for (std::size_t row = 0; row < 2; ++row)
        for (std::size_t col = 0; col < 8; ++col)
            result[row * 8 + col] = ColumnRect(col, row == 0 ? 198.8f : 458.5f);
    return result;
}
inline constexpr auto Field = MakeField();
inline constexpr std::size_t FieldSlotCount = 24;
inline constexpr float OverflowRowY = 330.0f;
[[nodiscard]] constexpr Rect FieldSlot(std::size_t index) {
    if (index < Field.size()) return Field[index];
    return ColumnRect((index - Field.size()) % 8, OverflowRowY);
}
} // namespace TableLayout
