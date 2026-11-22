#include "Deck.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <random>

namespace {
using enum CardType;
using enum RibbonType;

// Each group is one month, in atlas row order. The names describe the
// traditional cards; the railway-themed atlas reinterprets some motifs.
constexpr std::array<Card, Deck::CardCount> CardDatabase{{
    { 0,  1, 0, HIKARI,  NONE,       false, "Pine and Crane"},
    { 1,  1, 1, TANZAKU, RED_POETRY, false, "Pine Poetry Ribbon"},
    { 2,  1, 2, KASU,    NONE,       false, "Pine Plain 1"},
    { 3,  1, 3, KASU,    NONE,       false, "Pine Plain 2"},
    { 4,  2, 0, TANE,    NONE,       false, "Plum and Bush Warbler"},
    { 5,  2, 1, TANZAKU, RED_POETRY, false, "Plum Poetry Ribbon"},
    { 6,  2, 2, KASU,    NONE,       false, "Plum Plain 1"},
    { 7,  2, 3, KASU,    NONE,       false, "Plum Plain 2"},
    { 8,  3, 0, HIKARI,  NONE,       false, "Cherry Blossom Curtain"},
    { 9,  3, 1, TANZAKU, RED_POETRY, false, "Cherry Poetry Ribbon"},
    {10,  3, 2, KASU,    NONE,       false, "Cherry Plain 1"},
    {11,  3, 3, KASU,    NONE,       false, "Cherry Plain 2"},
    {12,  4, 0, TANE,    NONE,       false, "Wisteria and Cuckoo"},
    {13,  4, 1, TANZAKU, RED_PLAIN,  false, "Wisteria Red Ribbon"},
    {14,  4, 2, KASU,    NONE,       false, "Wisteria Plain 1"},
    {15,  4, 3, KASU,    NONE,       false, "Wisteria Plain 2"},
    {16,  5, 0, TANE,    NONE,       false, "Iris and Bridge"},
    {17,  5, 1, TANZAKU, RED_PLAIN,  false, "Iris Red Ribbon"},
    {18,  5, 2, KASU,    NONE,       false, "Iris Plain 1"},
    {19,  5, 3, KASU,    NONE,       false, "Iris Plain 2"},
    {20,  6, 0, TANE,    NONE,       false, "Peony and Butterflies"},
    {21,  6, 1, TANZAKU, BLUE,       false, "Peony Blue Ribbon"},
    {22,  6, 2, KASU,    NONE,       false, "Peony Plain 1"},
    {23,  6, 3, KASU,    NONE,       false, "Peony Plain 2"},
    {24,  7, 0, TANE,    NONE,       false, "Clover and Boar"},
    {25,  7, 1, TANZAKU, RED_PLAIN,  false, "Clover Red Ribbon"},
    {26,  7, 2, KASU,    NONE,       false, "Clover Plain 1"},
    {27,  7, 3, KASU,    NONE,       false, "Clover Plain 2"},
    {28,  8, 0, HIKARI,  NONE,       false, "Pampas Grass and Moon"},
    {29,  8, 1, TANE,    NONE,       false, "Pampas Grass and Geese"},
    {30,  8, 2, KASU,    NONE,       false, "Pampas Grass Plain 1"},
    {31,  8, 3, KASU,    NONE,       false, "Pampas Grass Plain 2"},
    {32,  9, 0, TANE,    NONE,       true,  "Chrysanthemum and Sake Cup"},
    {33,  9, 1, TANZAKU, BLUE,       false, "Chrysanthemum Blue Ribbon"},
    {34,  9, 2, KASU,    NONE,       false, "Chrysanthemum Plain 1"},
    {35,  9, 3, KASU,    NONE,       false, "Chrysanthemum Plain 2"},
    {36, 10, 0, TANE,    NONE,       false, "Maple and Deer"},
    {37, 10, 1, TANZAKU, BLUE,       false, "Maple Blue Ribbon"},
    {38, 10, 2, KASU,    NONE,       false, "Maple Plain 1"},
    {39, 10, 3, KASU,    NONE,       false, "Maple Plain 2"},
    {40, 11, 0, HIKARI,  NONE,       false, "Willow and Rainman"},
    {41, 11, 1, TANE,    NONE,       false, "Willow and Swallow"},
    {42, 11, 2, TANZAKU, RED_PLAIN,  false, "Willow Red Ribbon"},
    {43, 11, 3, KASU,    NONE,       false, "Willow Plain"},
    {44, 12, 0, HIKARI,  NONE,       false, "Paulownia and Phoenix"},
    {45, 12, 1, KASU,    NONE,       false, "Paulownia Plain 1"},
    {46, 12, 2, KASU,    NONE,       false, "Paulownia Plain 2"},
    {47, 12, 3, KASU,    NONE,       false, "Paulownia Plain 3"},
}};
} // namespace

Deck::Deck() : random_(std::random_device{}()) {
    Reset();
}

Deck::Deck(std::uint32_t seed) : random_(seed) {
    Reset();
}

void Deck::Reset() noexcept {
    cards_ = CardDatabase;
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

    for (std::size_t i = 0; i < CardDatabase.size(); ++i) {
        const Card& card = CardDatabase[i];
        if (card.id != static_cast<int>(i) || card.month != static_cast<int>(i / 4) + 1 ||
            card.cardIndex != static_cast<int>(i % 4) || card.name == nullptr || card.name[0] == '\0') {
            return false;
        }
        ++months[static_cast<std::size_t>(card.month - 1)];
        switch (card.type) {
            case HIKARI: ++hikari; break;
            case TANE: ++tane; break;
            case TANZAKU: ++tanzaku; break;
            case KASU: ++kasu; break;
            default: return false;
        }
        switch (card.ribbon) {
            case NONE:
                if (card.type == TANZAKU) return false;
                break;
            case RED_POETRY:
            case BLUE:
            case RED_PLAIN:
                if (card.type != TANZAKU) return false;
                break;
            default: return false;
        }
        if (card.isSakeCup) {
            if (card.month != 9 || card.cardIndex != 0 || card.type != TANE) return false;
            ++sakeCups;
        }
    }
    return hikari == 5 && tane == 9 && tanzaku == 10 && kasu == 24 && sakeCups == 1 &&
           std::all_of(months.begin(), months.end(), [](int count) { return count == 4; });
}
