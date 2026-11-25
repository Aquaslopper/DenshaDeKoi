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
// Bounds follow the actual dividers in each half of MatchUI.png. The right
// panel is not a simple mirror of the left panel's two cell widths.
struct CaptureLaneLayout { float x; float width; float step; };
inline CaptureLaneLayout MatchCaptureLayout(bool you, std::size_t column, std::size_t count,
                                             float cardWidth) {
    constexpr float leftScale = 398.0f / 397.0f;
    const std::array<float, 3> edges = you
        ? std::array<float, 3>{12.0f + 60.0f * leftScale, 12.0f + 218.0f * leftScale, 12.0f + 387.0f * leftScale}
        : std::array<float, 3>{950.0f + 7.0f, 950.0f + 179.0f, 950.0f + 337.0f};
    constexpr float padding = 3.0f;
    const float x = edges[column] + padding;
    const float width = edges[column + 1] - edges[column] - 2.0f * padding;
    const float step = count > 1 ? std::min(cardWidth, (width - cardWidth) / static_cast<float>(count - 1)) : 0.0f;
    return {x, width, step};
}
} // namespace Ui
