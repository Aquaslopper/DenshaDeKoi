#pragma once

#include "KoiKoiStateMachine.hpp"
#include "TableLayout.hpp"

#include <array>
#include <optional>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

struct TableCard {
    int id = -1; // -1 means a solid black back.
    TableLayout::Rect rect{};
    float flip = 1.0f; // Horizontal width factor for a card flip.
    float elevation = TableLayout::CardElevation;
};

// At most 48 physical cards plus a handful of simultaneous presentation
// overlays. Kept inline so rendering does not allocate a vector every frame.
struct TableCardList {
    std::array<TableCard, 64> items{};
    std::size_t count = 0;
    void push_back(TableCard card) {
        if (count == items.size()) throw std::length_error("Too many table card visuals");
        items[count++] = card;
    }
    [[nodiscard]] std::size_t size() const noexcept { return count; }
    [[nodiscard]] const TableCard& operator[](std::size_t i) const { return items[i]; }
    [[nodiscard]] const TableCard& back() const { return items[count - 1]; }
    [[nodiscard]] const TableCard* begin() const noexcept { return items.data(); }
    [[nodiscard]] const TableCard* end() const noexcept { return items.data() + count; }
};

// Match coordinator and presentation state; independent of Raylib and the clock.
// Update() receives elapsed seconds; all rule decisions belong to the core.
class TableGame {
public:
    TableGame();
    explicit TableGame(std::uint32_t seed);
    void Update(float seconds);
    void PointerDown(TableLayout::Point2 p);
    void PointerMove(TableLayout::Point2 p);
    // Re-anchor the grabbed point when a presentation layer raises the card.
    void ReanchorDrag(TableLayout::Point2 p);
    void PointerUp(TableLayout::Point2 p);
    void CancelDrag();
    void Choose(Decision decision);
    void NextRound();

    [[nodiscard]] TableCardList CardVisuals() const; // Allocation-free render snapshot.
    [[nodiscard]] std::vector<TableCard> Cards() const; // Compatibility convenience copy.
    [[nodiscard]] const KoiKoiStateMachine& Rules() const { return game_; }
    [[nodiscard]] const std::array<int, 2>& Scores() const { return scores_; }
    [[nodiscard]] int Round() const { return round_; }
    [[nodiscard]] bool MatchFinished() const { return finished_; }
    [[nodiscard]] bool Busy() const { return animation_ != Animation::NONE; }
    [[nodiscard]] bool Dragging() const { return dragged_.has_value(); }
    void FormatStatus(char* output, std::size_t size) const;
    [[nodiscard]] std::string Status() const; // Compatibility convenience copy.
    [[nodiscard]] TableLayout::Rect FieldRect(CardId id) const;

    // Xeno-koi-style drop target: edge gap <=10 plan pixels, with a guard
    // against predominantly overlapping a different month.
    [[nodiscard]] std::optional<CardId> FindDropTarget(CardId card, TableLayout::Rect drop) const;

private:
    enum class Animation { NONE, RETURN, CPU_FLIP, HAND_MOVE, STOCK_FLIP, STOCK_MOVE, HIT_FREEZE, CAPTURE };
    struct Moving { int id; TableLayout::Rect from, to; };
    void Start();
    void SyncField();
    [[nodiscard]] int FindFreeSlot(TableLayout::Point2 near) const;
    void StartHand(CardId id, TableLayout::Rect from, std::optional<CardId> target);
    void StartStockMove(std::optional<CardId> target);
    void ResolveMove();
    void FinishAnimation();
    void BeginAnimation(Animation type, float duration);
    void RecordResult();
    [[nodiscard]] TableLayout::Rect HandRect(Player player, std::size_t i) const;
    [[nodiscard]] TableLayout::Rect CaptureRect(Player player) const;

    KoiKoiStateMachine game_;
    std::array<int, 48> slots_{};
    std::array<int, 2> scores_{};
    int round_ = 1;
    bool resultRecorded_ = false;
    bool finished_ = false;
    float cpuDelay_ = 0;
    std::optional<int> dragged_;
    TableLayout::Rect dragRect_{};
    TableLayout::Point2 dragOffset_{};
    Animation animation_ = Animation::NONE;
    float elapsed_ = 0, duration_ = 0;
    bool matched_ = false;
    bool stockReady_ = false;
    bool compactField_ = false;
    int pendingSlot_ = -1;
    std::vector<Moving> moving_;
};
