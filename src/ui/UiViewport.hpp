#pragma once

#include "TableLayout.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace Ui {

// Maps the fixed 1360x768 UI design space onto the letterboxed game viewport.
// Every menu can keep its own visual identity while sharing predictable scaling.
struct Viewport {
    Rectangle box{};
    float scale = 1.0f;

    [[nodiscard]] int X(float x) const {
        return static_cast<int>(std::lround(box.x + x * scale));
    }
    [[nodiscard]] int Y(float y) const {
        return static_cast<int>(std::lround(box.y + y * scale));
    }
    [[nodiscard]] int W(float width) const {
        return std::max(1, static_cast<int>(std::lround(width * scale)));
    }
    [[nodiscard]] int H(float height) const {
        return std::max(1, static_cast<int>(std::lround(height * scale)));
    }
    [[nodiscard]] int Font(float size) const { return W(size); }
    [[nodiscard]] Rectangle Rec(TableLayout::Rect rect) const {
        return {box.x + rect.x * scale, box.y + rect.y * scale,
                rect.width * scale, rect.height * scale};
    }
};

} // namespace Ui
