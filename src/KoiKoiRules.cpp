#include "KoiKoiRules.hpp"

#include "DeckRegister.hpp"
#include "KoiKoiProfile.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
constexpr std::size_t TotalCards = 48;

std::bitset<TotalCards> ValidateCards(std::span<const CardId> cards) {
    std::bitset<TotalCards> seen;
    for (const CardId id : cards) {
        if (id < 0 || id >= static_cast<int>(TotalCards)) {
            throw std::invalid_argument("Card ID out of range");
        }
        const auto index = static_cast<std::size_t>(id);
        if (seen.test(index)) throw std::invalid_argument("Duplicate card ID");
        seen.set(index);
    }
    return seen;
}

std::array<int, 12> MonthCounts(std::span<const CardId> cards) {
    if (cards.size() != 8) throw std::invalid_argument("Expected exactly eight cards");
    (void)ValidateCards(cards);
    std::array<int, 12> counts{};
    for (const CardId id : cards) ++counts[static_cast<std::size_t>(DeckRegister::At(id).month - 1)];
    return counts;
}

bool FourPairs(const std::array<int, 12>& counts) {
    return std::count(counts.begin(), counts.end(), 2) == 4;
}
} // namespace

std::size_t PlayerIndex(Player player) {
    switch (player) {
        case Player::OPPONENT: return 0;
        case Player::PLAYER: return 1;
    }
    throw std::invalid_argument("Invalid player");
}

Player OtherPlayer(Player player) {
    return PlayerIndex(player) == 0 ? Player::PLAYER : Player::OPPONENT;
}

bool IsFieldMisdeal(std::span<const CardId> field) {
    const auto counts = MonthCounts(field);
    return std::find(counts.begin(), counts.end(), 4) != counts.end() || FourPairs(counts);
}

OpeningHand CheckOpeningHand(std::span<const CardId> hand) {
    const auto counts = MonthCounts(hand);
    if (std::find(counts.begin(), counts.end(), 4) != counts.end()) return OpeningHand::TESHI;
    if (FourPairs(counts)) return OpeningHand::KUTTSUKI;
    return OpeningHand::NONE;
}

Deal DealCards(std::span<const CardId> shuffled, Player dealer) {
    if (shuffled.size() != TotalCards) throw std::invalid_argument("Deal needs 48 cards");
    (void)ValidateCards(shuffled);
    const auto dealerIndex = PlayerIndex(dealer);
    const auto nonDealerIndex = PlayerIndex(OtherPlayer(dealer));
    Deal deal;
    std::size_t next = 0;
    for (int pass = 0; pass < 4; ++pass) {
        for (int i = 0; i < 2; ++i) deal.hands[nonDealerIndex].push_back(shuffled[next++]);
        for (int i = 0; i < 2; ++i) deal.field.push_back(shuffled[next++]);
        for (int i = 0; i < 2; ++i) deal.hands[dealerIndex].push_back(shuffled[next++]);
    }
    deal.stock.assign(shuffled.begin() + static_cast<std::ptrdiff_t>(next), shuffled.end());
    return deal;
}

CaptureResult ResolveCapture(CardId incoming, std::span<const CardId> field,
                             std::optional<CardId> selection) {
    const Card& card = DeckRegister::At(incoming);
    const auto seen = ValidateCards(field);
    if (seen.test(static_cast<std::size_t>(incoming))) {
        throw std::invalid_argument("Incoming card is already on field");
    }
    std::vector<CardId> matches;
    for (CardId candidate : field) {
        if (DeckRegister::At(candidate).month == card.month) matches.push_back(candidate);
    }
    if (matches.size() > 3) throw std::logic_error("More than three field matches");
    CaptureResult result;
    result.field.assign(field.begin(), field.end());
    if (matches.size() == 2) {
        if (!selection) {
            result.needsChoice = true;
            result.choices = matches;
            return result;
        }
        if (std::find(matches.begin(), matches.end(), *selection) == matches.end()) {
            throw std::invalid_argument("Selected card is not a matching target");
        }
        matches = {*selection};
    } else if (selection) {
        throw std::invalid_argument("Selection is valid only for two matches");
    }
    if (matches.empty()) {
        result.field.push_back(incoming);
    } else {
        result.captured.push_back(incoming);
        for (CardId match : matches) {
            result.captured.push_back(match);
            result.field.erase(std::find(result.field.begin(), result.field.end(), match));
        }
    }
    return result;
}

YakuScore ScoreYaku(std::span<const CardId> captured) {
    const auto seen = ValidateCards(captured);
    const auto has = [&seen](std::string_view stableId) {
        const Card* card = DeckRegister::Find(stableId);
        if (!card) throw std::logic_error("Required card missing from register");
        return seen.test(static_cast<std::size_t>(card->id));
    };
    YakuScore result;
    int brights = 0;
    for (CardId id : captured) {
        switch (DeckRegister::At(id).type) {
            case CardType::HIKARI: ++brights; break;
            case CardType::TANE: ++result.animals; break;
            case CardType::TANZAKU: ++result.ribbons; break;
            case CardType::KASU: ++result.physicalPlains; break;
        }
    }
    const bool rain = has("11-B");
    if (brights == 5) {
        result.brightYaku = BrightYaku::GOKO; result.bright = KoiKoiProfile::Goko;
    } else if (brights == 4) {
        result.brightYaku = rain ? BrightYaku::AME_SHIKO : BrightYaku::SHIKO;
        result.bright = rain ? KoiKoiProfile::AmeShiko : KoiKoiProfile::Shiko;
    } else if (brights == 3 && !rain) {
        result.brightYaku = BrightYaku::SANKO; result.bright = KoiKoiProfile::Sanko;
    }
    const bool cup = has("09-A");
    result.hanami = has("03-B") && cup ? KoiKoiProfile::FixedSet : 0;
    result.tsukimi = has("08-B") && cup ? KoiKoiProfile::FixedSet : 0;
    result.inoShikaCho = has("06-A") && has("07-A") && has("10-A") ? KoiKoiProfile::FixedSet : 0;
    result.akatan = has("01-RP") && has("02-RP") && has("03-RP") ? KoiKoiProfile::FixedSet : 0;
    result.aotan = has("06-RB") && has("09-RB") && has("10-RB") ? KoiKoiProfile::FixedSet : 0;
    result.tane = std::max(0, result.animals - (KoiKoiProfile::AnimalThreshold - 1));
    result.tanzaku = std::max(0, result.ribbons - (KoiKoiProfile::RibbonThreshold - 1));
    result.kasuUnits = result.physicalPlains + (cup ? 1 : 0);
    result.kasu = std::max(0, result.kasuUnits - (KoiKoiProfile::PlainThreshold - 1));
    result.base = result.bright + result.hanami + result.tsukimi + result.inoShikaCho +
                  result.tane + result.akatan + result.aotan + result.tanzaku + result.kasu;
    return result;
}

Settlement CalculateSettlement(int base, bool opponentCalledKoi) {
    if (base < 0) throw std::invalid_argument("Negative base score");
    // Ensure the largest possible multiplier cannot overflow.
    if (base > std::numeric_limits<int>::max() / 4) throw std::invalid_argument("Base score out of range");
    const int seven = base >= KoiKoiProfile::SevenThreshold ? 2 : 1;
    const int counter = opponentCalledKoi ? 2 : 1;
    return {base, seven, counter, base * seven * counter};
}
