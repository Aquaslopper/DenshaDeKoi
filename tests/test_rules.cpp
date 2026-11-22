#include "Deck.hpp"
#include "DeckRegister.hpp"
#include "KoiKoiRules.hpp"
#include "KoiKoiStateMachine.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename F> void MustThrow(F action) {
    bool threw = false;
    try { action(); } catch (const std::exception&) { threw = true; }
    Check(threw, "Invalid action was not rejected");
}

CardId Id(std::string_view id) {
    const Card* card = DeckRegister::Find(id);
    if (!card) throw std::runtime_error("Unknown test card ID");
    return card->id;
}
std::vector<CardId> Ids(std::initializer_list<std::string_view> ids) {
    std::vector<CardId> result;
    for (const auto id : ids) result.push_back(Id(id));
    return result;
}

void TestRegister() {
    constexpr std::array<std::string_view, 48> expected{{
        "01-B", "01-RP", "01-P1", "01-P2", "02-A", "02-RP", "02-P1", "02-P2",
        "03-B", "03-RP", "03-P1", "03-P2", "04-A", "04-R", "04-P1", "04-P2",
        "05-A", "05-R", "05-P1", "05-P2", "06-A", "06-RB", "06-P1", "06-P2",
        "07-A", "07-R", "07-P1", "07-P2", "08-B", "08-A", "08-P1", "08-P2",
        "09-A", "09-RB", "09-P1", "09-P2", "10-A", "10-RB", "10-P1", "10-P2",
        "11-B", "11-A", "11-R", "11-P1", "12-B", "12-P1", "12-P2", "12-P3"
    }};
    std::array<int, 4> counts{};
    int cups = 0, poetry = 0, blue = 0, plainRibbons = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const Card& card = DeckRegister::At(static_cast<int>(i));
        Check(card.id == static_cast<int>(i) && card.month == static_cast<int>(i / 4) + 1 &&
              card.cardIndex == static_cast<int>(i % 4) && card.stableId == expected[i] &&
              DeckRegister::Find(expected[i]) == &card, "Incorrect stable register mapping");
        const auto suffix = expected[i].substr(3);
        CardType expectedType = CardType::KASU;
        RibbonType expectedRibbon = RibbonType::NONE;
        if (suffix == "B") expectedType = CardType::HIKARI;
        else if (suffix == "A") expectedType = CardType::TANE;
        else if (suffix == "RP") { expectedType = CardType::TANZAKU; expectedRibbon = RibbonType::RED_POETRY; }
        else if (suffix == "RB") { expectedType = CardType::TANZAKU; expectedRibbon = RibbonType::BLUE; }
        else if (suffix == "R") { expectedType = CardType::TANZAKU; expectedRibbon = RibbonType::RED_PLAIN; }
        Check(card.type == expectedType && card.ribbon == expectedRibbon, "Wrong register category or ribbon");
        Check(card.isSakeCup == (expected[i] == "09-A"), "Wrong sake cup flag");
        ++counts[static_cast<std::size_t>(card.type)];
        cups += card.isSakeCup ? 1 : 0;
        poetry += card.ribbon == RibbonType::RED_POETRY ? 1 : 0;
        blue += card.ribbon == RibbonType::BLUE ? 1 : 0;
        plainRibbons += card.ribbon == RibbonType::RED_PLAIN ? 1 : 0;
    }
    Check(counts == std::array<int, 4>{5, 9, 10, 24} && cups == 1 && poetry == 3 &&
          blue == 3 && plainRibbons == 4, "Wrong register totals");
    Check(Deck::ValidateCardDatabase() && DeckRegister::Find("05-B") == nullptr &&
          DeckRegister::Find("10-B") == nullptr, "Register invariants failed");
    MustThrow([] { (void)DeckRegister::At(-1); });
    MustThrow([] { (void)DeckRegister::At(48); });
}

void TestScoring() {
    const auto bright = Ids({"01-B", "03-B", "08-B", "11-B", "12-B"});
    for (int mask = 0; mask < 32; ++mask) {
        std::vector<CardId> cards;
        int count = 0;
        for (int bit = 0; bit < 5; ++bit) if (mask & (1 << bit)) {
            cards.push_back(bright[static_cast<std::size_t>(bit)]); ++count;
        }
        const bool rain = (mask & 8) != 0;
        const int expected = count == 5 ? 10 : count == 4 ? (rain ? 7 : 8) :
                             count == 3 && !rain ? 5 : 0;
        const YakuScore score = ScoreYaku(cards);
        Check(score.bright == expected && score.base == expected, "Bright precedence or rain rule failed");
        if (count == 4) Check(score.brightYaku == (rain ? BrightYaku::AME_SHIKO : BrightYaku::SHIKO),
                              "Four-bright classification failed");
    }
    const auto cupCards = Ids({"09-A", "02-A", "04-A", "05-A", "07-A",
                               "01-P1", "01-P2", "02-P1", "02-P2", "03-P1",
                               "03-P2", "04-P1", "04-P2", "05-P1"});
    const YakuScore cup = ScoreYaku(cupCards);
    Check(cup.animals == 5 && cup.tane == 1 && cup.physicalPlains == 9 &&
          cup.kasuUnits == 10 && cup.kasu == 1 && cup.base == 2,
          "Sake cup must count as an animal and a phantom plain simultaneously");
    const auto viewings = ScoreYaku(Ids({"03-B", "08-B", "11-B", "09-A"}));
    Check(viewings.bright == 0 && viewings.hanami == 5 && viewings.tsukimi == 5 && viewings.base == 10,
          "Rain must not cancel either viewing");
    const auto sets = ScoreYaku(Ids({"06-A", "07-A", "10-A", "02-A", "09-A",
                                     "01-RP", "02-RP", "03-RP", "06-RB", "09-RB", "10-RB"}));
    Check(sets.inoShikaCho == 5 && sets.tane == 1 && sets.akatan == 5 &&
          sets.aotan == 5 && sets.tanzaku == 2 && sets.base == 18,
          "Fixed and generic sets must stack");
    std::vector<CardId> all;
    for (int i = 0; i < 48; ++i) all.push_back(i);
    Check(ScoreYaku(all).base == 62, "All-card additive score incorrect");
    auto invalid = Ids({"09-A", "09-A"});
    MustThrow([&] { (void)ScoreYaku(invalid); });
    MustThrow([] { (void)ScoreYaku(std::array<CardId, 1>{48}); });
    struct Example { int base; bool counter; int award; };
    constexpr std::array<Example, 7> examples{{{1,false,1}, {5,true,10}, {6,false,6},
                                                {7,false,14}, {7,true,28}, {8,true,32}, {12,false,24}}};
    for (const auto& e : examples) {
        const auto result = CalculateSettlement(e.base, e.counter);
        Check(result.base == e.base && result.sevenFactor == (e.base >= 7 ? 2 : 1) &&
              result.counterFactor == (e.counter ? 2 : 1) && result.award == e.award,
              "Incorrect settlement multiplier");
    }
    MustThrow([] { (void)CalculateSettlement(-1, false); });
}

void TestCapture() {
    const CardId incoming = Id("01-B");
    auto field = Ids({"02-P1"});
    auto result = ResolveCapture(incoming, field);
    Check(!result.needsChoice && result.captured.empty() && result.field == Ids({"02-P1", "01-B"}),
          "Zero-match discard failed");
    field = Ids({"02-P1", "01-P1"});
    result = ResolveCapture(incoming, field);
    Check(result.captured == Ids({"01-B", "01-P1"}) && result.field == Ids({"02-P1"}),
          "One-match capture failed");
    field = Ids({"01-P1", "02-P1", "01-P2"});
    result = ResolveCapture(incoming, field);
    Check(result.needsChoice && result.choices == Ids({"01-P1", "01-P2"}) &&
          result.field == field && result.captured.empty(), "Two matches require a choice");
    MustThrow([&] { (void)ResolveCapture(incoming, field, Id("02-P1")); });
    result = ResolveCapture(incoming, field, Id("01-P2"));
    Check(result.captured == Ids({"01-B", "01-P2"}) && result.field == Ids({"01-P1", "02-P1"}),
          "Two-match selection must take exactly one");
    field = Ids({"01-P1", "01-P2", "01-RP", "02-P1"});
    result = ResolveCapture(incoming, field);
    Check(result.captured == Ids({"01-B", "01-P1", "01-P2", "01-RP"}) &&
          result.field == Ids({"02-P1"}), "Three matches must capture all four");
    MustThrow([&] { (void)ResolveCapture(incoming, field, Id("01-P1")); });
    Check(ResolveCapture(Id("11-P1"), Ids({"01-P1"})).captured.empty(), "Lightning must not be wild");
    MustThrow([&] { (void)ResolveCapture(incoming, Ids({"01-B"})); });
}

// Assemble a valid permutation with specified 8-card zones and stock prefix.
std::vector<CardId> OrderForDeal(const std::vector<CardId>& player, const std::vector<CardId>& opponent,
                                 const std::vector<CardId>& field, const std::vector<CardId>& stockPrefix,
                                 Player dealer) {
    Check(player.size() == 8 && opponent.size() == 8 && field.size() == 8, "Bad fixture dimensions");
    std::vector<CardId> result;
    const auto& nonDealer = dealer == Player::PLAYER ? opponent : player;
    const auto& dealerHand = dealer == Player::PLAYER ? player : opponent;
    for (std::size_t pass = 0; pass < 4; ++pass) {
        for (std::size_t j = 0; j < 2; ++j) result.push_back(nonDealer[2 * pass + j]);
        for (std::size_t j = 0; j < 2; ++j) result.push_back(field[2 * pass + j]);
        for (std::size_t j = 0; j < 2; ++j) result.push_back(dealerHand[2 * pass + j]);
    }
    result.insert(result.end(), stockPrefix.begin(), stockPrefix.end());
    for (int id = 0; id < 48; ++id) {
        if (std::find(result.begin(), result.end(), id) == result.end()) result.push_back(id);
    }
    Check(result.size() == 48, "Fixture duplicates a card");
    return result;
}

std::vector<CardId> ExampleOrder(std::string_view stockTop = "01-RP") {
    return OrderForDeal(Ids({"01-B", "08-B", "09-A", "10-A", "11-B", "12-B", "03-B", "04-A"}),
                        Ids({"02-A", "03-RP", "04-R", "05-A", "06-A", "07-A", "08-A", "09-RB"}),
                        Ids({"01-P1", "01-P2", "02-P1", "03-P1", "04-P1", "05-P1", "06-P1", "07-P1"}),
                        Ids({stockTop}), Player::PLAYER);
}

void TestOpenings() {
    Check(CheckOpeningHand(Ids({"01-B", "01-RP", "01-P1", "01-P2", "02-A", "03-B", "04-A", "05-A"})) == OpeningHand::TESHI,
          "Teshi detection failed");
    const auto fourPairs = Ids({"01-B", "01-RP", "02-A", "02-RP", "03-B", "03-RP", "04-A", "04-R"});
    Check(CheckOpeningHand(fourPairs) == OpeningHand::KUTTSUKI && IsFieldMisdeal(fourPairs),
          "Four pairs detection failed");
    const auto threeTwoTwoOne = Ids({"01-B", "01-RP", "01-P1", "02-A", "02-RP", "03-B", "03-RP", "04-A"});
    Check(CheckOpeningHand(threeTwoTwoOne) == OpeningHand::NONE && !IsFieldMisdeal(threeTwoTwoOne),
          "3-2-2-1 must not be treated as four pairs");
    Check(IsFieldMisdeal(Ids({"01-B", "01-RP", "01-P1", "01-P2", "02-A", "03-B", "04-A", "05-A"})),
          "Field quad must be a misdeal");
    MustThrow([] { (void)CheckOpeningHand(Ids({"01-B"})); });
    MustThrow([] { (void)IsFieldMisdeal(Ids({"01-B", "01-B", "02-A", "03-B", "04-A", "05-A", "06-A", "07-A"})); });

    const auto player = Ids({"01-B", "01-RP", "01-P1", "01-P2", "03-B", "04-A", "05-A", "09-A"});
    const auto ordinaryOpponent = Ids({"02-RP", "03-RP", "04-R", "06-A", "07-A", "08-B", "10-A", "11-A"});
    const auto luckyOpponent = Ids({"02-A", "02-RP", "02-P1", "02-P2", "03-RP", "04-R", "06-A", "07-A"});
    const auto field = Ids({"06-P1", "07-P1", "08-P1", "09-P1", "10-P1", "11-P1", "12-P1", "05-P1"});
    KoiKoiStateMachine one(Player::OPPONENT, 1);
    Check(one.TryStartDeal(OrderForDeal(player, ordinaryOpponent, field, {}, Player::OPPONENT)), "Valid field rejected");
    Check(one.GetPhase() == Phase::ROUND_END && one.Result()->reason == EndReason::OPENING_WIN &&
          one.Result()->winner == Player::PLAYER && one.Result()->points == std::array<int, 2>{0,6} &&
          one.Result()->nextDealer == Player::PLAYER && one.ValidateConservation(), "Opening winner wrong");
    one.StartNextRound();
    Check(one.GetPhase() == Phase::DEAL_CHECK && one.Dealer() == Player::PLAYER && one.ValidateConservation(),
          "Winner must deal next");
    KoiKoiStateMachine both(Player::OPPONENT, 1);
    Check(both.TryStartDeal(OrderForDeal(player, luckyOpponent, field, {}, Player::OPPONENT)), "Valid field rejected");
    Check(both.GetPhase() == Phase::ROUND_END && both.Result()->reason == EndReason::BOTH_OPENING_DRAW &&
          !both.Result()->winner && both.Result()->points == std::array<int, 2>{0,0} &&
          both.Result()->nextDealer == Player::OPPONENT && both.ValidateConservation(), "Both-lucky draw wrong");
    const auto playerPairs = Ids({"01-B", "01-RP", "02-A", "02-RP", "03-B", "03-RP", "04-A", "04-R"});
    const auto opponentPairs = Ids({"05-A", "05-R", "06-A", "06-RB", "07-A", "07-R", "08-B", "08-A"});
    const auto pairField = Ids({"09-A", "09-RB", "09-P1", "10-A", "10-RB", "11-B", "11-A", "12-B"});
    KoiKoiStateMachine bothPairs(Player::PLAYER, 1);
    Check(bothPairs.TryStartDeal(OrderForDeal(playerPairs, opponentPairs, pairField, {}, Player::PLAYER)) &&
          bothPairs.Result()->reason == EndReason::BOTH_OPENING_DRAW && bothPairs.ValidateConservation(),
          "Both Kuttsuki hands must end as a draw");
    const auto invalidField = Ids({"08-B", "08-A", "08-P1", "08-P2", "09-P1", "10-P1", "11-P1", "12-P1"});
    KoiKoiStateMachine misdeal(Player::OPPONENT, 1);
    Check(!misdeal.TryStartDeal(OrderForDeal(player, luckyOpponent, invalidField, {}, Player::OPPONENT)) &&
          misdeal.GetPhase() == Phase::DEAL_CHECK && !misdeal.Result() && misdeal.Misdeals() == 1 &&
          misdeal.Dealer() == Player::OPPONENT && misdeal.ValidateConservation(),
          "Field misdeal must take precedence over opening wins");
    misdeal.Advance();
    Check(misdeal.GetPhase() != Phase::DEAL_CHECK && misdeal.Dealer() == Player::OPPONENT &&
          misdeal.ValidateConservation(), "Misdeal must reshuffle and redeal same dealer");
}

void TestStateChoices() {
    KoiKoiStateMachine hand(Player::PLAYER, 1);
    Check(hand.TryStartDeal(ExampleOrder()) && hand.GetPhase() == Phase::AWAIT_HAND_PLAY, "Fixture opening failed");
    MustThrow([&] { hand.ChooseDecision(Decision::SHOUBU); });
    MustThrow([&] { hand.PlayHandCard(Id("02-A")); });
    hand.PlayHandCard(Id("01-B"));
    hand.CancelHandChoice();
    Check(hand.GetPhase() == Phase::AWAIT_HAND_PLAY && !hand.PendingCard() &&
          hand.Hand(Player::PLAYER).size() == 8 && hand.ValidateConservation(), "Cancelled hand choice changed cards");
    hand.PlayHandCard(Id("01-B"));
    Check(hand.GetPhase() == Phase::AWAIT_HAND_CHOICE && hand.Choices() == Ids({"01-P1", "01-P2"}) &&
          hand.Hand(Player::PLAYER).size() == 8 && hand.ValidateConservation(), "Hand choice must be compulsory and uncommitted");
    MustThrow([&] { hand.ChooseFieldCard(Id("02-P1")); });
    Check(hand.GetPhase() == Phase::AWAIT_HAND_CHOICE && hand.ValidateConservation(), "Invalid choice changed state");
    hand.ChooseFieldCard(Id("01-P1"));
    Check(hand.GetPhase() == Phase::RESOLVE_HAND && hand.ValidateConservation(), "Wrong hand resolution phase");
    hand.Advance();
    Check(hand.GetPhase() == Phase::REVEAL_DRAW && hand.Captured(Player::PLAYER) == Ids({"01-B", "01-P1"}) &&
          hand.Hand(Player::PLAYER).size() == 7 && hand.ValidateConservation(), "Hand capture failed");
    hand.Advance();
    Check(hand.GetPhase() == Phase::RESOLVE_DRAW && hand.PendingCard() == Id("01-RP") &&
          hand.StockRemaining() == 23 && hand.ValidateConservation(), "Stock draw not mandatory or duplicated");
    hand.Advance();
    Check(hand.Captured(Player::PLAYER) == Ids({"01-B", "01-P1", "01-RP", "01-P2"}) &&
          hand.GetPhase() == Phase::CHECK_YAKU && hand.ValidateConservation(), "Stock capture must use updated field");

    KoiKoiStateMachine draw(Player::PLAYER, 1);
    Check(draw.TryStartDeal(ExampleOrder()), "Fixture opening failed");
    draw.PlayHandCard(Id("08-B"));
    Check(draw.GetPhase() == Phase::RESOLVE_HAND, "No-match hand play failed");
    draw.Advance(); draw.Advance();
    Check(draw.GetPhase() == Phase::AWAIT_DRAW_CHOICE && draw.PendingCard() == Id("01-RP") &&
          draw.Choices() == Ids({"01-P1", "01-P2"}) && draw.ValidateConservation(), "Stock choice must be compulsory");
    MustThrow([&] { draw.PlayHandCard(Id("01-B")); });
    MustThrow([&] { draw.CancelHandChoice(); });
    MustThrow([&] { draw.ChooseFieldCard(Id("02-P1")); });
    draw.ChooseFieldCard(Id("01-P2"));
    draw.Advance();
    Check(draw.Captured(Player::PLAYER) == Ids({"01-RP", "01-P2"}) && draw.ValidateConservation(),
          "Stock two-match choice failed");

    KoiKoiStateMachine pickup(Player::PLAYER, 1);
    Check(pickup.TryStartDeal(ExampleOrder("08-P2")), "Fixture opening failed");
    pickup.PlayHandCard(Id("08-B")); pickup.Advance(); pickup.Advance(); pickup.Advance();
    Check(pickup.Captured(Player::PLAYER) == Ids({"08-P2", "08-B"}) && pickup.ValidateConservation(),
          "Stock must be able to capture just-discarded hand card");
}

void TestSettlementRound() {
    const auto order = OrderForDeal(
        Ids({"03-P1", "01-B", "02-A", "04-A", "05-A", "06-A", "07-A", "08-B"}),
        Ids({"01-RP", "02-RP", "03-RP", "04-R", "05-R", "06-RB", "07-R", "08-A"}),
        Ids({"03-B", "09-A", "01-P1", "02-P1", "04-P1", "05-P1", "06-P1", "07-P1"}),
        Ids({"09-P1"}), Player::PLAYER);
    KoiKoiStateMachine game(Player::PLAYER, 1);
    Check(game.TryStartDeal(order) && game.GetPhase() == Phase::AWAIT_HAND_PLAY, "Settlement fixture failed");
    game.PlayHandCard(Id("03-P1"));
    game.Advance();
    Check(game.GetPhase() == Phase::REVEAL_DRAW, "Hand capture must be followed by stock draw");
    MustThrow([&] { game.ChooseDecision(Decision::SHOUBU); });
    game.Advance(); game.Advance();
    Check(game.GetPhase() == Phase::CHECK_YAKU && game.StockRemaining() == 23 && game.ValidateConservation(),
          "Mandatory stock capture failed");
    game.Advance();
    Check(game.GetPhase() == Phase::AWAIT_DECISION && game.CurrentScore().hanami == 5,
          "Completed turn should offer Hanami decision");
    game.ChooseDecision(Decision::SHOUBU);
    Check(game.Result()->reason == EndReason::SHOUBU && game.Result()->points == std::array<int, 2>{0,5} &&
          game.Result()->settlement.sevenFactor == 1 && game.Result()->settlement.counterFactor == 1 &&
          game.Result()->nextDealer == Player::PLAYER && game.ValidateConservation(), "Ordinary settlement failed");
}

void TestThreeMatchState() {
    const auto order = OrderForDeal(
        Ids({"01-B", "08-B", "09-A", "10-A", "11-B", "12-B", "03-B", "04-A"}),
        Ids({"02-A", "03-RP", "04-R", "05-A", "06-A", "07-A", "08-A", "09-RB"}),
        Ids({"01-P1", "01-P2", "01-RP", "02-P1", "03-P1", "04-P1", "05-P1", "06-P1"}),
        Ids({"07-P1"}), Player::PLAYER);
    KoiKoiStateMachine game(Player::PLAYER, 1);
    Check(game.TryStartDeal(order) && game.GetPhase() == Phase::AWAIT_HAND_PLAY, "Triple field fixture failed");
    game.PlayHandCard(Id("01-B"));
    Check(game.GetPhase() == Phase::RESOLVE_HAND, "Three matches must be automatic");
    game.Advance();
    Check(game.Captured(Player::PLAYER) == Ids({"01-B", "01-P1", "01-P2", "01-RP"}) &&
          game.GetPhase() == Phase::REVEAL_DRAW && game.ValidateConservation(), "Triple hand capture failed");

    const auto stockOrder = OrderForDeal(
        Ids({"08-B", "09-A", "10-A", "11-B", "12-B", "03-B", "04-A", "05-A"}),
        Ids({"02-A", "03-RP", "04-R", "05-R", "06-A", "07-A", "08-A", "09-RB"}),
        Ids({"01-P1", "01-P2", "01-RP", "02-P1", "03-P1", "04-P1", "05-P1", "06-P1"}),
        Ids({"01-B"}), Player::PLAYER);
    KoiKoiStateMachine stock(Player::PLAYER, 1);
    Check(stock.TryStartDeal(stockOrder) && stock.GetPhase() == Phase::AWAIT_HAND_PLAY, "Triple stock fixture failed");
    stock.PlayHandCard(Id("08-B")); stock.Advance(); stock.Advance();
    Check(stock.GetPhase() == Phase::RESOLVE_DRAW, "Three stock matches must be automatic");
    stock.Advance();
    Check(stock.Captured(Player::PLAYER) == Ids({"01-B", "01-P1", "01-P2", "01-RP"}) &&
          stock.ValidateConservation(), "Triple stock capture failed");
}

void TestThousandDeals() {
    Deck deck(1729);
    int misdeals = 0;
    int valid = 0;
    int openings = 0;
    for (int trial = 0; trial < 1000; ++trial) {
        deck.Reset();
        deck.Shuffle();
        std::array<CardId, 48> order{};
        for (auto& id : order) id = deck.Draw().value().id;
        const Player dealer = trial % 2 == 0 ? Player::PLAYER : Player::OPPONENT;
        const Deal deal = DealCards(order, dealer);
        Check(deal.hands[0].size() == 8 && deal.hands[1].size() == 8 &&
              deal.field.size() == 8 && deal.stock.size() == 24 && deal.stock.front() == order[24],
              "Wrong deal distribution or stock top");
        const auto independentCounts = [](const std::vector<CardId>& cards) {
            std::array<int, 12> counts{};
            for (CardId id : cards) ++counts[static_cast<std::size_t>(id / 4)];
            return counts;
        };
        const auto counts = independentCounts(deal.field);
        const bool fieldInvalid = std::count(counts.begin(), counts.end(), 4) > 0 ||
                                  std::count(counts.begin(), counts.end(), 2) == 4;
        Check(IsFieldMisdeal(deal.field) == fieldInvalid, "Field detection disagrees with independent count");
        if (fieldInvalid) { ++misdeals; continue; }
        ++valid;
        for (const auto& hand : deal.hands) {
            const auto handCounts = independentCounts(hand);
            const OpeningHand expected = std::count(handCounts.begin(), handCounts.end(), 4) ? OpeningHand::TESHI :
                (std::count(handCounts.begin(), handCounts.end(), 2) == 4 ? OpeningHand::KUTTSUKI : OpeningHand::NONE);
            Check(CheckOpeningHand(hand) == expected, "Opening hand disagrees with independent count");
            if (expected != OpeningHand::NONE) ++openings;
        }
    }
    Check(misdeals > 0 && valid > 0 && openings > 0, "Deal exercise failed to cover misdeals or openings");
    std::array<CardId, 48> ordered{};
    for (int i = 0; i < 48; ++i) ordered[static_cast<std::size_t>(i)] = i;
    const Deal precise = DealCards(ordered, Player::PLAYER);
    Check(precise.hands[0] == std::vector<CardId>({0, 1, 6, 7, 12, 13, 18, 19}) &&
          precise.field == std::vector<CardId>({2, 3, 8, 9, 14, 15, 20, 21}) &&
          precise.hands[1] == std::vector<CardId>({4, 5, 10, 11, 16, 17, 22, 23}) &&
          precise.stock.front() == 24, "Deal cadence or stock order changed");
    std::array<CardId, 48> duplicate{};
    MustThrow([&] { (void)DealCards(duplicate, Player::PLAYER); });
}

struct SimulationStats {
    int draws = 0;
    int finalKoi = 0;
    int unchangedBase = 0;
    int counterWins = 0;
    int doubleFactorWins = 0;
    int finalStop = 0;
};

void Simulate(std::uint32_t seed, bool stopAgainstKoi, bool stopOnFinal, SimulationStats& stats) {
    KoiKoiStateMachine game(Player::PLAYER, seed);
    game.Advance();
    Check(game.ValidateConservation(), "Conservation failed after deal");
    if (game.GetPhase() == Phase::ROUND_END) return; // Opening hands.
    Check(game.CurrentPlayer() == Player::PLAYER && game.Hand(Player::PLAYER).size() == 8 &&
          game.Hand(Player::OPPONENT).size() == 8 && game.Field().size() == 8 &&
          game.StockRemaining() == 24 && !IsFieldMisdeal(game.Field()), "Invalid initial game state");
    std::array<int, 2> lastBase{};
    int turns = 0;
    int reveals = 0;
    bool finalDecisionKoi = false;
    for (int steps = 0; game.GetPhase() != Phase::ROUND_END && steps < 300; ++steps) {
        Check(game.ValidateConservation(), "Card lost or duplicated during round");
        switch (game.GetPhase()) {
            case Phase::AWAIT_HAND_PLAY:
                Check(!game.Hand(game.CurrentPlayer()).empty(), "Active player has empty hand");
                game.PlayHandCard(game.Hand(game.CurrentPlayer()).front());
                ++turns;
                break;
            case Phase::AWAIT_HAND_CHOICE:
            case Phase::AWAIT_DRAW_CHOICE:
                Check(game.Choices().size() == 2, "Choice must have exactly two targets");
                game.ChooseFieldCard(game.Choices().front());
                break;
            case Phase::REVEAL_DRAW:
                game.Advance();
                ++reveals;
                break;
            case Phase::CHECK_YAKU: {
                const auto player = PlayerIndex(game.CurrentPlayer());
                const int base = game.CurrentScore().base;
                const bool shouldDecide = base > lastBase[player];
                if (base > 0 && base == lastBase[player]) ++stats.unchangedBase;
                lastBase[player] = base;
                game.Advance();
                Check((game.GetPhase() == Phase::AWAIT_DECISION) == shouldDecide,
                      "Decision eligibility must require a higher base score");
                break;
            }
            case Phase::AWAIT_DECISION: {
                const Player current = game.CurrentPlayer();
                const bool otherKoi = game.HasCalledKoi(OtherPlayer(current));
                const bool finalTurn = game.Hand(Player::PLAYER).empty() && game.Hand(Player::OPPONENT).empty();
                if ((stopAgainstKoi && otherKoi) || (stopOnFinal && finalTurn)) {
                    const int base = game.CurrentScore().base;
                    const Settlement expected = CalculateSettlement(base, otherKoi);
                    game.ChooseDecision(Decision::SHOUBU);
                    Check(game.Result()->reason == EndReason::SHOUBU && game.Result()->winner == current &&
                          game.Result()->points[PlayerIndex(current)] == expected.award &&
                          game.Result()->points[PlayerIndex(OtherPlayer(current))] == 0 &&
                          game.Result()->nextDealer == current &&
                          game.Result()->settlement.counterFactor == expected.counterFactor,
                          "Shoubu settlement failed");
                    if (otherKoi) ++stats.counterWins;
                    if (otherKoi && expected.sevenFactor == 2) ++stats.doubleFactorWins;
                    if (finalTurn) {
                        Check(turns == 16 && reveals == 16 && game.StockRemaining() == 8,
                              "Final-turn Shoubu settled before the mandatory draw");
                        ++stats.finalStop;
                    }
                } else {
                    finalDecisionKoi = finalTurn;
                    game.ChooseDecision(Decision::KOI_KOI);
                    Check(game.HasCalledKoi(current), "Koi flag must remain sticky");
                }
                break;
            }
            case Phase::RESOLVE_HAND:
            case Phase::RESOLVE_DRAW:
            case Phase::NEXT_PLAYER:
                game.Advance();
                break;
            default:
                throw std::runtime_error("Unexpected phase during simulation");
        }
    }
    Check(game.GetPhase() == Phase::ROUND_END && game.Result().has_value() && game.ValidateConservation(),
          "Round did not terminate cleanly");
    if (game.Result()->reason == EndReason::EXHAUSTIVE_DRAW) {
        Check(turns == 16 && reveals == 16 && game.StockRemaining() == 8 &&
              !game.Result()->winner && game.Result()->points == std::array<int, 2>{0,0} &&
              game.Result()->nextDealer == game.Dealer(), "Exhaustive round must be a 0-0 draw after 16 turns");
        ++stats.draws;
        if (finalDecisionKoi) ++stats.finalKoi;
    }
    const Player nextDealer = game.Result()->nextDealer;
    game.StartNextRound();
    Check(game.GetPhase() == Phase::DEAL_CHECK && game.Dealer() == nextDealer &&
          !game.HasCalledKoi(Player::PLAYER) && !game.HasCalledKoi(Player::OPPONENT) &&
          !game.Result() && game.ValidateConservation(), "Next round did not reset round state");
    game.Advance();
    Check(game.ValidateConservation(), "Next round did not deal a complete deck");
}

void TestRoundSimulations() {
    SimulationStats stats;
    for (std::uint32_t seed = 1; seed <= 200; ++seed) {
        Simulate(seed, false, false, stats);
        Simulate(seed, true, false, stats);
        Simulate(seed, false, true, stats);
    }
    Check(stats.draws > 0 && stats.finalKoi > 0 && stats.unchangedBase > 0 &&
          stats.counterWins > 0 && stats.doubleFactorWins > 0 && stats.finalStop > 0,
          "Simulations did not cover draws, final decisions, unchanged scores, and multipliers");
}
} // namespace

int main() {
    try {
        TestRegister();
        TestScoring();
        TestCapture();
        TestOpenings();
        TestStateChoices();
        TestSettlementRound();
        TestThreeMatchState();
        TestThousandDeals();
        TestRoundSimulations();
        std::cout << "KK-ADD-10 register, deals, captures, scoring and state machine passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Rules test failed: " << error.what() << '\n';
        return 1;
    }
}
