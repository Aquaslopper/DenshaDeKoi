#pragma once

#include <stdexcept>

// Shared, graphics-independent atlas geometry. Coordinates are in source pixels.
namespace AtlasLayout {
inline constexpr int MonthCount = 12;
inline constexpr int CardsPerMonth = 4;

struct Rect {
    float x;
    float y;
    float width;
    float height;
};

[[nodiscard]] inline Rect GetCardSourceRect(float atlasWidth, float atlasHeight,
                                             int month, int cardIndex) {
    if (atlasWidth <= 0.0f || atlasHeight <= 0.0f) {
        throw std::invalid_argument("Atlas dimensions must be positive");
    }
    if (month < 1 || month > MonthCount || cardIndex < 0 || cardIndex >= CardsPerMonth) {
        throw std::out_of_range("Card month or index is outside the atlas");
    }
    const float cardWidth = atlasWidth / MonthCount;
    const float cardHeight = atlasHeight / CardsPerMonth;
    return {static_cast<float>(month - 1) * cardWidth,
            static_cast<float>(cardIndex) * cardHeight, cardWidth, cardHeight};
}
} // namespace AtlasLayout
