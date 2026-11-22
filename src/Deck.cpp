#include "Deck.hpp"
#include "DeckRegister.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <random>
#include <string_view>

static_assert(Deck::CardCount == DeckRegister::Cards.size());

Deck::Deck() : random_(std::random_device{}()) {
    Reset();
}

Deck::Deck(std::uint32_t seed) : random_(seed) {
    Reset();
}

void Deck::Reset() noexcept {
    cards_ = DeckRegister::Cards;
    nextCard_ = 0;
}

void Deck::Shuffle() {
    // Already-drawn cards are never returned to the deck by shuffling.
    std::shuffle(std::next(cards_.begin(), static_cast<std::ptrdiff_t>(nextCard_)),
                 cards_.end(), random_);
}

std::optional<Card> Deck::Draw() noexcept {
    if (nextCard_ == cards_.size()) {
        return std::nullopt;
    }
    return cards_[nextCard_++];
}

std::size_t Deck::Remaining() const noexcept {
    return cards_.size() - nextCard_;
}

bool Deck::ValidateCardDatabase() noexcept {
    std::array<int, 12> months{};
    int hikari = 0;
    int tane = 0;
    int tanzaku = 0;
    int kasu = 0;
    int sakeCups = 0;

    for (std::size_t i = 0; i < DeckRegister::Cards.size(); ++i) {
        const Card& card = DeckRegister::Cards[i];
        if (card.id != static_cast<int>(i) || card.month != static_cast<int>(i / 4) + 1 ||
            card.cardIndex != static_cast<int>(i % 4) || card.name == nullptr || card.name[0] == '\0' ||
            card.stableId == nullptr || card.stableId[0] == '\0') {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (std::string_view(card.stableId) == DeckRegister::Cards[j].stableId) return false;
        }
        ++months[static_cast<std::size_t>(card.month - 1)];
        switch (card.type) {
            case CardType::HIKARI: ++hikari; break;
            case CardType::TANE: ++tane; break;
            case CardType::TANZAKU: ++tanzaku; break;
            case CardType::KASU: ++kasu; break;
            default: return false;
        }
        switch (card.ribbon) {
            case RibbonType::NONE:
                if (card.type == CardType::TANZAKU) return false;
                break;
            case RibbonType::RED_POETRY:
            case RibbonType::BLUE:
            case RibbonType::RED_PLAIN:
                if (card.type != CardType::TANZAKU) return false;
                break;
            default: return false;
        }
        if (card.isSakeCup) {
            if (card.month != 9 || card.cardIndex != 0 || card.type != CardType::TANE) return false;
            ++sakeCups;
        }
    }
    return hikari == 5 && tane == 9 && tanzaku == 10 && kasu == 24 && sakeCups == 1 &&
           std::all_of(months.begin(), months.end(), [](int count) { return count == 4; });
}
