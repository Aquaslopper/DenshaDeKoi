#pragma once

#include "DeckRegister.hpp"
#include "KoiKoiRules.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace Ui {
// Xeno Koi's bright, animal, ribbon, plain order. Preserve capture order;
// the sake cup belongs to the animal lane even though it also scores as kasu.
inline std::array<std::vector<CardId>, 4> MatchCaptureLanes(std::span<const CardId> captured) {
    std::array<std::vector<CardId>, 4> lanes;
    for (CardId id : captured) {
        switch (DeckRegister::At(id).type) {
            case CardType::HIKARI: lanes[0].push_back(id); break;
            case CardType::TANE: lanes[1].push_back(id); break;
            case CardType::TANZAKU: lanes[2].push_back(id); break;
            case CardType::KASU: lanes[3].push_back(id); break;
        }
    }
    return lanes;
}
// Visible vertical interiors in the 81-pixel artwork, excluding the frame and
// central colored stripe. Center in each one instead of anchoring at the top.
inline float MatchCaptureY(std::size_t row, float cardHeight) {
    constexpr std::array<float, 2> centers{(7.0f + 38.0f) * 0.5f, (43.5f + 73.5f) * 0.5f};
    return 10.0f + centers[row] - cardHeight * 0.5f;
}

// Bounds follow the actual inner cell edges in MatchUI.png drawn un-sliced at (12, 10).
// The right panel is not a simple mirror of the left panel's two cell widths.
struct CaptureLaneLayout { float x; float width; float step; };
inline CaptureLaneLayout MatchCaptureLayout(bool you, std::size_t column, std::size_t count,
                                             float cardWidth) {
    struct Cell { float left; float right; };
    const std::array<Cell, 2> cells = you
        ? std::array<Cell, 2>{{{12.0f + 62.0f, 12.0f + 216.0f}, {12.0f + 220.0f, 12.0f + 386.0f}}}
        : std::array<Cell, 2>{{{12.0f + 946.0f, 12.0f + 1112.0f}, {12.0f + 1117.0f, 12.0f + 1271.0f}}};
    constexpr float padding = 4.0f;
    constexpr float gap = 2.0f;
    const float x = cells[column].left + padding;
    const float width = cells[column].right - cells[column].left - 2.0f * padding;
    const float step = count > 1 ? std::min(cardWidth + gap, (width - cardWidth) / static_cast<float>(count - 1)) : 0.0f;
    return {x, width, step};
}
} // namespace Ui
