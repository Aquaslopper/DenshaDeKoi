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

// Oya-gime: earlier month wins, then higher card category. Equal month and
// category requires a redraw. The caller supplies two distinct drawn cards.
[[nodiscard]] std::optional<Player> DetermineFirstDealer(CardId playerCard, CardId opponentCard);

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
// Xeno-koi deal: player 8, opponent 8, field 8, then 24 stock cards.
// The last shuffled card is the first stock card revealed; dealer plays first.
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
    int akatanAotan = 0; // Replaces the individual fixed sets when both are present.
    int tanzaku = 0;
    int kasu = 0;
    int animals = 0;
    int ribbons = 0;
    int physicalPlains = 0;
    int kasuUnits = 0;
    int base = 0;
};
[[nodiscard]] YakuScore ScoreYaku(std::span<const CardId> captured);

enum class YakuId {
    GOKO, SHIKO, AME_SHIKO, SANKO, HANAMI, TSUKIMI, INO_SHIKA_CHO,
    AKATAN, AOTAN, AKATAN_AOTAN, TANE, TANZAKU, KASU, COUNT
};
using YakuSnapshot = std::array<int, static_cast<std::size_t>(YakuId::COUNT)>;
[[nodiscard]] YakuSnapshot SnapshotYaku(const YakuScore& score) noexcept;
[[nodiscard]] bool HasNewOrGrownYaku(const YakuSnapshot& current, const YakuSnapshot& previous) noexcept;

struct Settlement {
    int base = 0;
    int sevenFactor = 1;
    int counterFactor = 1;
    int callerFactor = 1;
    int award = 0;
};
// Each caller's own Koi-Koi increases their factor by one, up to x6.
[[nodiscard]] Settlement CalculateSettlement(int base, bool opponentCalledKoi, int selfKoiCalls = 0);
