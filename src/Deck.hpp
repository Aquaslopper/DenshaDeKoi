#pragma once

#include "CardData.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>

// Pure deck state; no rendering or game rules are involved.
class Deck {
public:
    static constexpr std::size_t CardCount = 48;

    Deck();
    // A fixed seed is useful for repeatable tests or replays.
    explicit Deck(std::uint32_t seed);

    // Restore all cards to month/atlas-row order without reseeding the generator.
    void Reset() noexcept;
    // Shuffle only the cards that have not been drawn.
    void Shuffle();
    [[nodiscard]] std::optional<Card> Draw() noexcept;
    [[nodiscard]] std::size_t Remaining() const noexcept;

    // Headless database invariant check, also callable from a debugger.
    [[nodiscard]] static bool ValidateCardDatabase() noexcept;

private:
    std::array<Card, CardCount> cards_{};
    std::size_t nextCard_ = 0;
    std::mt19937 random_;
};
