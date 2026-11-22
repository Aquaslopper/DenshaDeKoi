#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

// Ordinals index DeckRegister::Cards; stable MM-X IDs are exposed by the register.
using CardId = int;

enum class Player { OPPONENT = 0, PLAYER = 1 };
[[nodiscard]] Player OtherPlayer(Player player);
[[nodiscard]] std::size_t PlayerIndex(Player player);

enum class OpeningHand { NONE, TESHI, KUTTSUKI };

struct Deal {
    std::array<std::vector<CardId>, 2> hands;
    std::vector<CardId> field;
    // The first element is the top of the stock.
    std::vector<CardId> stock;
};

// Inputs must contain distinct, valid register IDs; malformed input throws.
[[nodiscard]] bool IsFieldMisdeal(std::span<const CardId> field);
[[nodiscard]] OpeningHand CheckOpeningHand(std::span<const CardId> hand);
// 48 cards in shuffled order; 2 to non-dealer, field, dealer, repeated four times.
[[nodiscard]] Deal DealCards(std::span<const CardId> shuffled, Player dealer);

struct CaptureResult {
    bool needsChoice = false;
    std::vector<CardId> choices;
    std::vector<CardId> field;
    std::vector<CardId> captured;
};
// No mutation. For two matches and no selection, returns needsChoice and the
// unchanged field. A selection is permitted only for a two-match situation.
[[nodiscard]] CaptureResult ResolveCapture(CardId incoming, std::span<const CardId> field,
                                           std::optional<CardId> selection = std::nullopt);

enum class BrightYaku { NONE, SANKO, AME_SHIKO, SHIKO, GOKO };
struct YakuScore {
    BrightYaku brightYaku = BrightYaku::NONE;
    int bright = 0;
    int hanami = 0;
    int tsukimi = 0;
    int inoShikaCho = 0;
    int tane = 0;
    int akatan = 0;
    int aotan = 0;
    int tanzaku = 0;
    int kasu = 0;
    int animals = 0;
    int ribbons = 0;
    int physicalPlains = 0;
    int kasuUnits = 0;
    int base = 0;
};
[[nodiscard]] YakuScore ScoreYaku(std::span<const CardId> captured);

struct Settlement {
    int base = 0;
    int sevenFactor = 1;
    int counterFactor = 1;
    int award = 0;
};
[[nodiscard]] Settlement CalculateSettlement(int base, bool opponentCalledKoi);
