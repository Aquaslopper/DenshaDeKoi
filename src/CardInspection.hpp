#pragma once

#include <cstddef>
#include <cmath>
#include <span>

// The capture browser fits above the table in the 1360x768 canvas. It makes
// every captured card available, not only the top card of each 3D pile.
namespace CardInspection {
inline constexpr float Left = 712, Top = 94, Width = 246, Height = 282;
inline constexpr float GridX = Left+7, GridY = Top+25, CellWidth = 29, CellHeight = 42;
inline constexpr std::size_t Columns = 8, Capacity = 48;
// Both browser and preview use logical screen pixels, so they remain beside
// one another and the preview remains 1:1 when the canvas is scaled.
[[nodiscard]] inline int PreviewLeft(float viewportX, float viewportWidth) noexcept {
    return static_cast<int>(std::round(viewportX + viewportWidth - 394.0f));
}
[[nodiscard]] inline int PanelTop(float viewportY, float viewportWidth) noexcept {
    return static_cast<int>(std::round(viewportY + 94.0f*viewportWidth/1360.0f));
}
[[nodiscard]] inline int BrowserLeft(float viewportX, float viewportWidth) noexcept {
    return PreviewLeft(viewportX,viewportWidth) - 254;
}
[[nodiscard]] inline bool InBrowser(float x, float y) noexcept {
    return x >= Left && x < Left+Width && y >= Top && y < Top+Height;
}
[[nodiscard]] inline int CapturedAt(std::span<const int> player, std::span<const int> opponent,
                                    float x, float y) noexcept {
    if (!std::isfinite(x) || !std::isfinite(y) || x < GridX || y < GridY ||
        x >= GridX + Columns*CellWidth || y >= GridY + (Capacity/Columns)*CellHeight) return -1;
    const auto column = static_cast<std::size_t>((x-GridX)/CellWidth);
    const auto row = static_cast<std::size_t>((y-GridY)/CellHeight);
    if (column >= Columns || row >= Capacity/Columns) return -1;
    const std::size_t index = row*Columns+column;
    if (index < player.size()) return player[index];
    const std::size_t other = index-player.size();
    return other < opponent.size() ? opponent[other] : -1;
}
}
