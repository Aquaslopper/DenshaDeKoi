#include "Deck.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using enum CardType;
using enum RibbonType;

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Card DrawRequired(Deck& deck) {
    const auto card = deck.Draw();
    Check(card.has_value(), "Deck ran out early");
    return *card;
}

void TestDatabase() {
    Check(Deck::ValidateCardDatabase(), "Database validation failed");
    // Independent expectations for each atlas row, not inferred from the database.
    constexpr std::array<std::array<CardType, 4>, 12> types{{
        {HIKARI, TANZAKU, KASU, KASU}, {TANE, TANZAKU, KASU, KASU},
        {HIKARI, TANZAKU, KASU, KASU}, {TANE, TANZAKU, KASU, KASU},
        {TANE, TANZAKU, KASU, KASU}, {TANE, TANZAKU, KASU, KASU},
        {TANE, TANZAKU, KASU, KASU}, {HIKARI, TANE, KASU, KASU},
        {TANE, TANZAKU, KASU, KASU}, {TANE, TANZAKU, KASU, KASU},
        {HIKARI, TANE, TANZAKU, KASU}, {HIKARI, KASU, KASU, KASU}
    }};
    constexpr std::array<std::array<RibbonType, 4>, 12> ribbons{{
        {NONE, RED_POETRY, NONE, NONE}, {NONE, RED_POETRY, NONE, NONE},
        {NONE, RED_POETRY, NONE, NONE}, {NONE, RED_PLAIN, NONE, NONE},
        {NONE, RED_PLAIN, NONE, NONE}, {NONE, BLUE, NONE, NONE},
        {NONE, RED_PLAIN, NONE, NONE}, {NONE, NONE, NONE, NONE},
        {NONE, BLUE, NONE, NONE}, {NONE, BLUE, NONE, NONE},
        {NONE, NONE, RED_PLAIN, NONE}, {NONE, NONE, NONE, NONE}
    }};
    Deck deck(1);
    Check(deck.Remaining() == 48, "New deck must contain 48 cards");
    int sakeCount = 0;
    for (int id = 0; id < 48; ++id) {
        const Card card = DrawRequired(deck);
        const int month = id / 4;
        const int row = id % 4;
        Check(card.id == id && card.month == month + 1 && card.cardIndex == row,
              "Card ID and atlas coordinates do not match");
        Check(card.type == types[month][row], "Incorrect card type for atlas cell");
        Check(card.ribbon == ribbons[month][row], "Incorrect ribbon type for atlas cell");
        Check(card.isSakeCup == (month == 8 && row == 0), "Incorrect sake cup flag");
        Check(card.name != nullptr && std::string_view(card.name).size() > 0, "Missing card name");
        sakeCount += card.isSakeCup ? 1 : 0;
        Check(deck.Remaining() == 47 - static_cast<std::size_t>(id), "Wrong remaining count after draw");
    }
    Check(sakeCount == 1, "Expected exactly one sake cup");
    Check(!deck.Draw().has_value() && !deck.Draw().has_value() && deck.Remaining() == 0,
          "Exhausted draw must remain empty");
    deck.Shuffle();
    Check(!deck.Draw().has_value(), "Shuffling an empty deck must not restore cards");
    deck.Reset();
    Check(deck.Remaining() == 48 && DrawRequired(deck).id == 0, "Reset must restore ordered deck");
}

void TestShuffle() {
    Deck first(12345);
    Deck second(12345);
    Deck ordered(1);
    std::array<Card, 48> expected{};
    for (auto& card : expected) card = DrawRequired(ordered);
    first.Shuffle();
    second.Shuffle();
    std::array<bool, 48> seen{};
    bool changedOrder = false;
    for (int i = 0; i < 48; ++i) {
        const Card a = DrawRequired(first);
        const Card b = DrawRequired(second);
        Check(a.id == b.id, "Identical seeds and operations must reproduce a shuffle");
        Check(a.id >= 0 && a.id < 48 && !seen[static_cast<std::size_t>(a.id)],
              "Shuffle lost or duplicated a card");
        seen[static_cast<std::size_t>(a.id)] = true;
        const Card& original = expected[static_cast<std::size_t>(a.id)];
        Check(a.month == original.month && a.cardIndex == original.cardIndex &&
              a.type == original.type && a.ribbon == original.ribbon &&
              a.isSakeCup == original.isSakeCup && std::string_view(a.name) == original.name,
              "Shuffle changed card metadata");
        changedOrder = changedOrder || a.id != i;
    }
    Check(changedOrder, "Shuffle did not change the card order");
    Check(first.Remaining() == 0, "Shuffled deck should be exhausted");
    first.Reset();
    Check(first.Remaining() == 48, "Reset after shuffle must restore all cards");
    for (int id = 0; id < 48; ++id) {
        Check(DrawRequired(first).id == id, "Reset must restore canonical order");
    }
}

void TestShuffleAfterDrawing() {
    Deck deck(6789);
    deck.Shuffle();
    std::array<bool, 48> seen{};
    for (int i = 0; i < 11; ++i) {
        const Card card = DrawRequired(deck);
        Check(card.id >= 0 && card.id < 48 && !seen[static_cast<std::size_t>(card.id)],
              "Invalid or duplicate card before reshuffle");
        seen[static_cast<std::size_t>(card.id)] = true;
    }
    Check(deck.Remaining() == 37, "Incorrect remaining count before reshuffle");
    deck.Shuffle();
    Check(deck.Remaining() == 37, "Shuffling remaining cards changed their count");
    for (int i = 0; i < 37; ++i) {
        const Card card = DrawRequired(deck);
        Check(card.id >= 0 && card.id < 48 && !seen[static_cast<std::size_t>(card.id)],
              "Reshuffle reintroduced or duplicated a drawn card");
        seen[static_cast<std::size_t>(card.id)] = true;
    }
    for (bool found : seen) Check(found, "Card was lost during reshuffle");
    Check(!deck.Draw().has_value(), "Reshuffled deck should be exhausted");
}
} // namespace

int main() {
    try {
        TestDatabase();
        TestShuffle();
        TestShuffleAfterDrawing();
        // Exercise construction using std::random_device as well as deterministic seeds.
        Deck defaultDeck;
        Check(defaultDeck.Remaining() == 48, "Default constructor failed");
        std::cout << "Deck database, shuffle, draw, exhaustion, and reset checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Deck test failed: " << error.what() << '\n';
        return 1;
    }
}
