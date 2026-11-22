#include "KoiKoiStateMachine.hpp"

#include "KoiKoiProfile.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <stdexcept>
#include <utility>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::logic_error(message);
}
} // namespace

KoiKoiStateMachine::KoiKoiStateMachine(Player dealer)
    : dealer_(dealer), current_(dealer) {
    (void)PlayerIndex(dealer);
}

KoiKoiStateMachine::KoiKoiStateMachine(Player dealer, std::uint32_t seed)
    : deck_(seed), dealer_(dealer), current_(dealer) {
    (void)PlayerIndex(dealer);
}

const std::vector<CardId>& KoiKoiStateMachine::Hand(Player player) const {
    return hands_[PlayerIndex(player)];
}

const std::vector<CardId>& KoiKoiStateMachine::Captured(Player player) const {
    return captured_[PlayerIndex(player)];
}

bool KoiKoiStateMachine::HasCalledKoi(Player player) const {
    return KoiCalls(player) > 0;
}

int KoiKoiStateMachine::KoiCalls(Player player) const {
    return koiCalls_[PlayerIndex(player)];
}

YakuScore KoiKoiStateMachine::CurrentScore() const {
    return ScoreYaku(captured_[PlayerIndex(current_)]);
}

bool KoiKoiStateMachine::TryStartDeal(std::span<const CardId> shuffled) {
    Require(phase_ == Phase::DEAL_CHECK, "Not awaiting a deal");
    Deal deal = DealCards(shuffled, dealer_);
    // Never inspect either opening hand until the field has passed validation.
    if (IsFieldMisdeal(deal.field)) {
        ++misdeals_;
        return false;
    }
    const bool opponentOpening = CheckOpeningHand(deal.hands[0]) != OpeningHand::NONE;
    const bool playerOpening = CheckOpeningHand(deal.hands[1]) != OpeningHand::NONE;
    hands_ = std::move(deal.hands);
    field_ = std::move(deal.field);
    stock_ = std::move(deal.stock);
    current_ = dealer_;
    if (opponentOpening && playerOpening) {
        EndDraw(EndReason::BOTH_OPENING_DRAW);
    } else if (opponentOpening || playerOpening) {
        const Player winner = opponentOpening ? Player::OPPONENT : Player::PLAYER;
        RoundResult result{EndReason::OPENING_WIN, winner, {}, winner, {}};
        result.points[PlayerIndex(winner)] = KoiKoiProfile::OpeningPoints;
        result_ = result;
        phase_ = Phase::ROUND_END;
    } else {
        phase_ = Phase::AWAIT_HAND_PLAY;
    }
    return true;
}

void KoiKoiStateMachine::Advance() {
    switch (phase_) {
        case Phase::DEAL_CHECK: {
            // Every attempt begins with all 48 cards; a misdeal never advances the dealer.
            for (;;) {
                deck_.Reset();
                deck_.Shuffle();
                std::array<CardId, Deck::CardCount> shuffled{};
                for (CardId& id : shuffled) id = deck_.Draw().value().id;
                if (TryStartDeal(shuffled)) break;
            }
            break;
        }
        case Phase::RESOLVE_HAND:
            ResolvePending(false);
            phase_ = Phase::REVEAL_DRAW;
            break;
        case Phase::REVEAL_DRAW: {
            Require(!stock_.empty(), "Stock exhausted before mandatory draw");
            const CardId incoming = stock_.front();
            CaptureResult preview = ResolveCapture(incoming, field_);
            stock_.erase(stock_.begin());
            pending_ = incoming;
            pendingIsStock_ = true;
            selection_.reset();
            choices_ = std::move(preview.choices);
            phase_ = preview.needsChoice ? Phase::AWAIT_DRAW_CHOICE : Phase::RESOLVE_DRAW;
            break;
        }
        case Phase::RESOLVE_DRAW:
            ResolvePending(true);
            phase_ = Phase::CHECK_YAKU;
            break;
        case Phase::CHECK_YAKU: {
            const auto index = PlayerIndex(current_);
            const YakuSnapshot currentYaku = SnapshotYaku(CurrentScore());
            const bool eligible = HasNewOrGrownYaku(currentYaku, lastDecisionYaku_[index]);
            // Xeno-koi automatically banks an eligible yaku when the current
            // player's hand is empty, even if the other player has one card left.
            if (eligible && hands_[index].empty()) SettleShoubu();
            else if (eligible) phase_ = Phase::AWAIT_DECISION;
            else if (BothHandsEmpty()) EndDraw(EndReason::EXHAUSTIVE_DRAW);
            else phase_ = Phase::NEXT_PLAYER;
            break;
        }
        case Phase::NEXT_PLAYER: {
            const Player next = OtherPlayer(current_);
            Require(!hands_[PlayerIndex(next)].empty(), "Next player has no cards");
            current_ = next;
            phase_ = Phase::AWAIT_HAND_PLAY;
            break;
        }
        default:
            throw std::logic_error("Current phase requires a player action or round is over");
    }
}

void KoiKoiStateMachine::PlayHandCard(CardId card) {
    Require(phase_ == Phase::AWAIT_HAND_PLAY, "Not awaiting a hand play");
    const auto& hand = hands_[PlayerIndex(current_)];
    if (std::find(hand.begin(), hand.end(), card) == hand.end()) {
        throw std::invalid_argument("Card is not in the current player's hand");
    }
    CaptureResult preview = ResolveCapture(card, field_);
    pending_ = card;
    pendingIsStock_ = false;
    selection_.reset();
    choices_ = std::move(preview.choices);
    phase_ = preview.needsChoice ? Phase::AWAIT_HAND_CHOICE : Phase::RESOLVE_HAND;
}

void KoiKoiStateMachine::ChooseFieldCard(CardId target) {
    Require(phase_ == Phase::AWAIT_HAND_CHOICE || phase_ == Phase::AWAIT_DRAW_CHOICE,
            "Not awaiting a capture choice");
    if (std::find(choices_.begin(), choices_.end(), target) == choices_.end()) {
        throw std::invalid_argument("Target is not one of the matching cards");
    }
    selection_ = target;
    choices_.clear();
    phase_ = pendingIsStock_ ? Phase::RESOLVE_DRAW : Phase::RESOLVE_HAND;
}

void KoiKoiStateMachine::CancelHandChoice() {
    Require(phase_ == Phase::AWAIT_HAND_CHOICE, "Only an uncommitted hand choice can be cancelled");
    pending_.reset();
    selection_.reset();
    choices_.clear();
    phase_ = Phase::AWAIT_HAND_PLAY;
}

void KoiKoiStateMachine::ResolvePending(bool fromStock) {
    Require(pending_.has_value() && pendingIsStock_ == fromStock, "Missing pending card");
    CaptureResult capture = ResolveCapture(*pending_, field_, selection_);
    Require(!capture.needsChoice, "A capture choice is required");
    auto& hand = hands_[PlayerIndex(current_)];
    auto handCard = hand.end();
    if (!fromStock) {
        handCard = std::find(hand.begin(), hand.end(), *pending_);
        Require(handCard != hand.end(), "Pending card not in hand");
    }
    auto& won = captured_[PlayerIndex(current_)];
    // Allocate before changing zones so allocation failure leaves the turn intact.
    won.reserve(won.size() + capture.captured.size());
    if (!fromStock) hand.erase(handCard);
    field_ = std::move(capture.field);
    won.insert(won.end(), capture.captured.begin(), capture.captured.end());
    pending_.reset();
    pendingIsStock_ = false;
    selection_.reset();
    choices_.clear();
}

void KoiKoiStateMachine::ChooseDecision(Decision decision) {
    Require(phase_ == Phase::AWAIT_DECISION, "Not awaiting a Koi-Koi decision");
    if (decision == Decision::KOI_KOI) {
        const auto index = PlayerIndex(current_);
        lastDecisionYaku_[index] = SnapshotYaku(CurrentScore());
        ++koiCalls_[index];
        phase_ = Phase::NEXT_PLAYER;
    } else if (decision == Decision::SHOUBU) {
        SettleShoubu();
    } else {
        throw std::invalid_argument("Invalid decision");
    }
}

void KoiKoiStateMachine::SettleShoubu() {
    const Settlement settlement = CalculateSettlement(CurrentScore().base, HasCalledKoi(OtherPlayer(current_)),
                                                       KoiCalls(current_));
    RoundResult result{EndReason::SHOUBU, current_, {}, current_, settlement};
    result.points[PlayerIndex(current_)] = settlement.award;
    result_ = result;
    phase_ = Phase::ROUND_END;
}

void KoiKoiStateMachine::EndDraw(EndReason reason) {
    result_ = RoundResult{reason, std::nullopt, {}, dealer_, {}};
    phase_ = Phase::ROUND_END;
}

bool KoiKoiStateMachine::BothHandsEmpty() const noexcept {
    return hands_[0].empty() && hands_[1].empty();
}

void KoiKoiStateMachine::StartNextRound() {
    Require(phase_ == Phase::ROUND_END && result_.has_value(), "Round has not ended");
    dealer_ = result_->nextDealer;
    current_ = dealer_;
    hands_ = {};
    captured_ = {};
    field_.clear();
    stock_.clear();
    pending_.reset();
    pendingIsStock_ = false;
    selection_.reset();
    choices_.clear();
    koiCalls_ = {};
    lastDecisionYaku_ = {};
    result_.reset();
    misdeals_ = 0;
    phase_ = Phase::DEAL_CHECK;
}

bool KoiKoiStateMachine::ValidateConservation() const noexcept {
    if (phase_ == Phase::DEAL_CHECK) return hands_[0].empty() && hands_[1].empty() &&
        captured_[0].empty() && captured_[1].empty() && field_.empty() && stock_.empty() && !pending_;
    std::bitset<48> seen;
    const auto add = [&seen](CardId id) {
        if (id < 0 || id >= 48 || seen.test(static_cast<std::size_t>(id))) return false;
        seen.set(static_cast<std::size_t>(id));
        return true;
    };
    for (const auto& hand : hands_) for (CardId id : hand) if (!add(id)) return false;
    for (const auto& pile : captured_) for (CardId id : pile) if (!add(id)) return false;
    for (CardId id : field_) if (!add(id)) return false;
    for (CardId id : stock_) if (!add(id)) return false;
    if (pendingIsStock_ && (!pending_ || !add(*pending_))) return false;
    return seen.count() == 48;
}
