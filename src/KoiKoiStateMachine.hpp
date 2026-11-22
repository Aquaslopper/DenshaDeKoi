#pragma once

#include "Deck.hpp"
#include "KoiKoiRules.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

enum class Phase {
    DEAL_CHECK, AWAIT_HAND_PLAY, AWAIT_HAND_CHOICE, RESOLVE_HAND,
    REVEAL_DRAW, AWAIT_DRAW_CHOICE, RESOLVE_DRAW, CHECK_YAKU,
    AWAIT_DECISION, NEXT_PLAYER, ROUND_END
};
enum class Decision { KOI_KOI, SHOUBU };
enum class EndReason { OPENING_WIN, BOTH_OPENING_DRAW, SHOUBU, EXHAUSTIVE_DRAW };

struct RoundResult {
    EndReason reason;
    std::optional<Player> winner;
    std::array<int, 2> points{};
    Player nextDealer;
    Settlement settlement{}; // Opening wins are fixed 6 points, not multiplied.
};

class KoiKoiStateMachine {
public:
    explicit KoiKoiStateMachine(Player dealer = Player::PLAYER);
    KoiKoiStateMachine(Player dealer, std::uint32_t seed);

    [[nodiscard]] Phase GetPhase() const noexcept { return phase_; }
    [[nodiscard]] Player Dealer() const noexcept { return dealer_; }
    [[nodiscard]] Player CurrentPlayer() const noexcept { return current_; }
    [[nodiscard]] const std::vector<CardId>& Hand(Player player) const;
    [[nodiscard]] const std::vector<CardId>& Captured(Player player) const;
    [[nodiscard]] const std::vector<CardId>& Field() const noexcept { return field_; }
    [[nodiscard]] std::size_t StockRemaining() const noexcept { return stock_.size(); }
    [[nodiscard]] std::optional<CardId> PendingCard() const noexcept { return pending_; }
    [[nodiscard]] const std::vector<CardId>& Choices() const noexcept { return choices_; }
    [[nodiscard]] bool HasCalledKoi(Player player) const;
    [[nodiscard]] const std::optional<RoundResult>& Result() const noexcept { return result_; }
    [[nodiscard]] std::size_t Misdeals() const noexcept { return misdeals_; }
    [[nodiscard]] YakuScore CurrentScore() const;

    // Start a deterministic deal from a permutation, for replays/tests. If the
    // field is invalid, remain in DEAL_CHECK; Advance() will reshuffle/redeal.
    [[nodiscard]] bool TryStartDeal(std::span<const CardId> shuffled);
    // Advance one automatic phase. Calling in a decision/input phase throws.
    void Advance();
    void PlayHandCard(CardId card);
    void ChooseFieldCard(CardId target);
    // Only an uncommitted hand choice may be cancelled; stock choices are mandatory.
    void CancelHandChoice();
    void ChooseDecision(Decision decision);
    void StartNextRound();

    // Checks the 48 physical cards, including a revealed pending stock card.
    [[nodiscard]] bool ValidateConservation() const noexcept;

private:
    void EndDraw(EndReason reason);
    void ResolvePending(bool fromStock);
    [[nodiscard]] bool BothHandsEmpty() const noexcept;

    Deck deck_;
    Phase phase_ = Phase::DEAL_CHECK;
    Player dealer_;
    Player current_;
    std::array<std::vector<CardId>, 2> hands_;
    std::array<std::vector<CardId>, 2> captured_;
    std::vector<CardId> field_;
    std::vector<CardId> stock_;
    std::optional<CardId> pending_;
    bool pendingIsStock_ = false;
    std::optional<CardId> selection_;
    std::vector<CardId> choices_;
    std::array<bool, 2> calledKoi_{};
    std::array<int, 2> lastEvaluatedBase_{};
    std::optional<RoundResult> result_;
    std::size_t misdeals_ = 0;
};
