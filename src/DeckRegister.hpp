#pragma once

#include "CardData.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string_view>

// Canonical month-major order; this is the single source of card metadata.
namespace DeckRegister {
using enum CardType;
using enum RibbonType;
inline constexpr std::array<Card, 48> Cards{{
    { 0,  1, 0, HIKARI,  NONE,       false, "Pine and Crane", "01-B"},
    { 1,  1, 1, TANZAKU, RED_POETRY, false, "Pine Poetry Ribbon", "01-RP"},
    { 2,  1, 2, KASU,    NONE,       false, "Pine Plain 1", "01-P1"},
    { 3,  1, 3, KASU,    NONE,       false, "Pine Plain 2", "01-P2"},
    { 4,  2, 0, TANE,    NONE,       false, "Plum and Bush Warbler", "02-A"},
    { 5,  2, 1, TANZAKU, RED_POETRY, false, "Plum Poetry Ribbon", "02-RP"},
    { 6,  2, 2, KASU,    NONE,       false, "Plum Plain 1", "02-P1"},
    { 7,  2, 3, KASU,    NONE,       false, "Plum Plain 2", "02-P2"},
    { 8,  3, 0, HIKARI,  NONE,       false, "Cherry Blossom Curtain", "03-B"},
    { 9,  3, 1, TANZAKU, RED_POETRY, false, "Cherry Poetry Ribbon", "03-RP"},
    {10,  3, 2, KASU,    NONE,       false, "Cherry Plain 1", "03-P1"},
    {11,  3, 3, KASU,    NONE,       false, "Cherry Plain 2", "03-P2"},
    {12,  4, 0, TANE,    NONE,       false, "Wisteria and Cuckoo", "04-A"},
    {13,  4, 1, TANZAKU, RED_PLAIN,  false, "Wisteria Red Ribbon", "04-R"},
    {14,  4, 2, KASU,    NONE,       false, "Wisteria Plain 1", "04-P1"},
    {15,  4, 3, KASU,    NONE,       false, "Wisteria Plain 2", "04-P2"},
    {16,  5, 0, TANE,    NONE,       false, "Iris and Bridge", "05-A"},
    {17,  5, 1, TANZAKU, RED_PLAIN,  false, "Iris Red Ribbon", "05-R"},
    {18,  5, 2, KASU,    NONE,       false, "Iris Plain 1", "05-P1"},
    {19,  5, 3, KASU,    NONE,       false, "Iris Plain 2", "05-P2"},
    {20,  6, 0, TANE,    NONE,       false, "Peony and Butterflies", "06-A"},
    {21,  6, 1, TANZAKU, BLUE,       false, "Peony Blue Ribbon", "06-RB"},
    {22,  6, 2, KASU,    NONE,       false, "Peony Plain 1", "06-P1"},
    {23,  6, 3, KASU,    NONE,       false, "Peony Plain 2", "06-P2"},
    {24,  7, 0, TANE,    NONE,       false, "Clover and Boar", "07-A"},
    {25,  7, 1, TANZAKU, RED_PLAIN,  false, "Clover Red Ribbon", "07-R"},
    {26,  7, 2, KASU,    NONE,       false, "Clover Plain 1", "07-P1"},
    {27,  7, 3, KASU,    NONE,       false, "Clover Plain 2", "07-P2"},
    {28,  8, 0, HIKARI,  NONE,       false, "Pampas Grass and Moon", "08-B"},
    {29,  8, 1, TANE,    NONE,       false, "Pampas Grass and Geese", "08-A"},
    {30,  8, 2, KASU,    NONE,       false, "Pampas Grass Plain 1", "08-P1"},
    {31,  8, 3, KASU,    NONE,       false, "Pampas Grass Plain 2", "08-P2"},
    {32,  9, 0, TANE,    NONE,       true,  "Chrysanthemum and Sake Cup", "09-A"},
    {33,  9, 1, TANZAKU, BLUE,       false, "Chrysanthemum Blue Ribbon", "09-RB"},
    {34,  9, 2, KASU,    NONE,       false, "Chrysanthemum Plain 1", "09-P1"},
    {35,  9, 3, KASU,    NONE,       false, "Chrysanthemum Plain 2", "09-P2"},
    {36, 10, 0, TANE,    NONE,       false, "Maple and Deer", "10-A"},
    {37, 10, 1, TANZAKU, BLUE,       false, "Maple Blue Ribbon", "10-RB"},
    {38, 10, 2, KASU,    NONE,       false, "Maple Plain 1", "10-P1"},
    {39, 10, 3, KASU,    NONE,       false, "Maple Plain 2", "10-P2"},
    {40, 11, 0, HIKARI,  NONE,       false, "Willow and Rainman", "11-B"},
    {41, 11, 1, TANE,    NONE,       false, "Willow and Swallow", "11-A"},
    {42, 11, 2, TANZAKU, RED_PLAIN,  false, "Willow Red Ribbon", "11-R"},
    {43, 11, 3, KASU,    NONE,       false, "Willow and Lightning", "11-P1"},
    {44, 12, 0, HIKARI,  NONE,       false, "Paulownia and Phoenix", "12-B"},
    {45, 12, 1, KASU,    NONE,       false, "Paulownia Plain 1", "12-P1"},
    {46, 12, 2, KASU,    NONE,       false, "Paulownia Plain 2", "12-P2"},
    {47, 12, 3, KASU,    NONE,       false, "Paulownia Plain 3", "12-P3"},
}};

[[nodiscard]] inline const Card& At(int id) {
    if (id < 0 || id >= static_cast<int>(Cards.size())) {
        throw std::out_of_range("Card ID out of range");
    }
    return Cards[static_cast<std::size_t>(id)];
}

[[nodiscard]] inline const Card* Find(std::string_view stableId) noexcept {
    for (const Card& card : Cards) {
        if (stableId == card.stableId) return &card;
    }
    return nullptr;
}
} // namespace DeckRegister
