#pragma once

#include "DeckRegister.hpp"
#include "KoiKoiRules.hpp"

#include <array>
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
} // namespace Ui
