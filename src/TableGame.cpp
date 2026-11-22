#include "TableGame.hpp"

#include "DeckRegister.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
using TableLayout::Point2;
using TableLayout::Rect;

bool Contains(Rect r, Point2 p) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}
float Overlap(Rect a, Rect b) {
    return std::max(0.0f, std::min(a.x + a.width, b.x + b.width) - std::max(a.x, b.x)) *
           std::max(0.0f, std::min(a.y + a.height, b.y + b.height) - std::max(a.y, b.y));
}
float EdgeDistanceSq(Rect a, Rect b) {
    const float dx = std::max({0.0f, b.x - (a.x + a.width), a.x - (b.x + b.width)});
    const float dy = std::max({0.0f, b.y - (a.y + a.height), a.y - (b.y + b.height)});
    return dx * dx + dy * dy;
}
Point2 Center(Rect r) { return {r.x + r.width * 0.5f, r.y + r.height * 0.5f}; }
float DistanceSq(Point2 a, Point2 b) { return (a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y); }
Rect Interpolate(Rect a, Rect b, float t) {
    t = std::clamp(t, 0.0f, 1.0f); t = t * t * (3.0f - 2.0f * t);
    return {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.width+(b.width-a.width)*t, a.height+(b.height-a.height)*t};
}
Player DrawDealer(Deck& deck) {
    deck.Shuffle();
    for (;;) {
        auto a = deck.Draw();
        auto b = deck.Draw();
        if (!a || !b) { deck.Reset(); deck.Shuffle(); continue; }
        if (auto dealer = DetermineFirstDealer(a->id, b->id)) return *dealer;
    }
}
Player DrawDealer() { Deck deck; return DrawDealer(deck); }
Player DrawDealer(std::uint32_t seed) { Deck deck(seed ^ 0x9e3779b9u); return DrawDealer(deck); }
} // namespace

TableGame::TableGame() : game_(DrawDealer()) { Start(); }
TableGame::TableGame(std::uint32_t seed) : game_(DrawDealer(seed), seed) { Start(); }

void TableGame::Start() {
    slots_.fill(-1);
    compactField_ = false;
    game_.Advance();
    const auto& field = game_.Field();
    constexpr std::array<int, 8> initial{2, 3, 4, 5, 10, 11, 12, 13};
    for (std::size_t i = 0; i < field.size(); ++i) slots_[static_cast<std::size_t>(field[i])] = initial[i];
    RecordResult();
}

Rect TableGame::HandRect(Player player, std::size_t i) const {
    return player == Player::PLAYER ? TableLayout::PlayerHand.at(i) : TableLayout::OpponentHand.at(i);
}
Rect TableGame::CaptureRect(Player player) const {
    return player == Player::PLAYER ? TableLayout::PlayerCapture : TableLayout::OpponentCapture;
}
Rect TableGame::FieldRect(CardId id) const {
    if (id < 0 || id >= 48 || slots_[static_cast<std::size_t>(id)] < 0) throw std::out_of_range("Card not on field");
    return TableLayout::FieldSlot(static_cast<std::size_t>(slots_[static_cast<std::size_t>(id)]), compactField_);
}
int TableGame::FindFreeSlot(Point2 near) const {
    std::array<bool, TableLayout::FieldSlotCount> used{};
    for (int slot : slots_) if (slot >= 0) used[static_cast<std::size_t>(slot)] = true;
    float best = std::numeric_limits<float>::infinity(); int found = -1;
    const bool compact = compactField_ || game_.Field().size() >= 16;
    for (std::size_t i = 0; i < used.size(); ++i) if (!used[i]) {
        // Prefer the reference layout until its slots are full.
        if (!compact && i >= 16) continue;
        const float d = DistanceSq(Center(TableLayout::FieldSlot(i, compact)), near);
        if (d < best) { best = d; found = static_cast<int>(i); }
    }
    if (found < 0) throw std::logic_error("No free table field slot");
    return found;
}
void TableGame::SyncField() {
    for (std::size_t id = 0; id < slots_.size(); ++id)
        if (slots_[id] >= 0 && std::find(game_.Field().begin(), game_.Field().end(), static_cast<int>(id)) == game_.Field().end()) slots_[id] = -1;
    for (int id : game_.Field()) if (slots_[static_cast<std::size_t>(id)] < 0)
        slots_[static_cast<std::size_t>(id)] = FindFreeSlot({431.5f, 371.0f});
}

std::optional<CardId> TableGame::FindDropTarget(CardId card, Rect drop) const {
    const int month = DeckRegister::At(card).month;
    float otherOverlap = 0;
    for (int id : game_.Field()) if (DeckRegister::At(id).month != month)
        otherOverlap = std::max(otherOverlap, Overlap(drop, FieldRect(id)));
    float bestOverlap = -1, bestDistance = 0;
    std::optional<int> best;
    for (int id : game_.Field()) {
        if (DeckRegister::At(id).month != month) continue;
        const Rect r = FieldRect(id);
        if (EdgeDistanceSq(drop, r) > 100.0f) continue;
        const float overlap = Overlap(drop, r);
        if (overlap < r.width * r.height * 0.15f && otherOverlap > overlap) continue;
        const float dist = DistanceSq(Center(drop), Center(r));
        if (!best || overlap > bestOverlap + 0.5f || (std::abs(overlap - bestOverlap) <= 0.5f && dist < bestDistance)) {
            best = id; bestOverlap = overlap; bestDistance = dist;
        }
    }
    return best;
}

void TableGame::BeginAnimation(Animation type, float duration) {
    animation_ = type; elapsed_ = 0; duration_ = duration;
}
void TableGame::PointerDown(Point2 p) {
    if (Busy() || finished_) return;
    if (game_.GetPhase() == Phase::AWAIT_DRAW_CHOICE && game_.CurrentPlayer() == Player::PLAYER && stockReady_) {
        for (int id : game_.Choices()) if (Contains(FieldRect(id), p)) {
            game_.ChooseFieldCard(id); StartStockMove(id); return;
        }
        return;
    }
    if (game_.GetPhase() != Phase::AWAIT_HAND_PLAY || game_.CurrentPlayer() != Player::PLAYER) return;
    const auto& hand = game_.Hand(Player::PLAYER);
    for (std::size_t i = hand.size(); i-- > 0;) if (Contains(HandRect(Player::PLAYER, i), p)) {
        dragged_ = hand[i]; dragRect_ = HandRect(Player::PLAYER, i);
        dragOffset_ = {p.x - dragRect_.x, p.y - dragRect_.y}; return;
    }
}
void TableGame::PointerMove(Point2 p) {
    if (dragged_) { dragRect_.x = p.x - dragOffset_.x; dragRect_.y = p.y - dragOffset_.y; }
}
void TableGame::CancelDrag() {
    if (!dragged_) return;
    const auto& hand = game_.Hand(Player::PLAYER);
    auto it = std::find(hand.begin(), hand.end(), *dragged_);
    if (it != hand.end()) {
        moving_ = {{*dragged_, dragRect_, HandRect(Player::PLAYER, static_cast<std::size_t>(it - hand.begin()))}};
        BeginAnimation(Animation::RETURN, 0.4f);
    }
    dragged_.reset();
}
void TableGame::PointerUp(Point2 p) {
    if (!dragged_) return;
    PointerMove(p);
    const int id = *dragged_;
    const bool hasMatch = std::any_of(game_.Field().begin(), game_.Field().end(), [id](int x) {
        return DeckRegister::At(x).month == DeckRegister::At(id).month;
    });
    const auto target = FindDropTarget(id, dragRect_);
    if ((hasMatch && !target) || (!hasMatch && !Contains(TableLayout::FieldDropZone, Center(dragRect_)))) {
        CancelDrag(); return;
    }
    const Rect from = dragRect_;
    dragged_.reset();
    StartHand(id, from, target);
}
void TableGame::StartHand(CardId id, Rect from, std::optional<CardId> target) {
    game_.PlayHandCard(id);
    if (game_.GetPhase() == Phase::AWAIT_HAND_CHOICE) {
        if (!target) throw std::logic_error("Two matches require a target");
        game_.ChooseFieldCard(*target);
    }
    matched_ = target.has_value();
    pendingSlot_ = matched_ ? -1 : FindFreeSlot(Center(from));
    const Rect dest = target ? FieldRect(*target) : TableLayout::FieldSlot(static_cast<std::size_t>(pendingSlot_), compactField_ || game_.Field().size() >= 16);
    moving_ = {{id, from, dest}};
    if (game_.CurrentPlayer() == Player::OPPONENT) BeginAnimation(Animation::CPU_FLIP, 0.30f);
    else BeginAnimation(Animation::HAND_MOVE, 0.40f);
}
void TableGame::StartStockMove(std::optional<CardId> target) {
    const int id = *game_.PendingCard();
    matched_ = target.has_value();
    pendingSlot_ = matched_ ? -1 : FindFreeSlot({431.5f, 371.0f});
    const Rect dest = target ? FieldRect(*target) : TableLayout::FieldSlot(static_cast<std::size_t>(pendingSlot_), compactField_ || game_.Field().size() >= 16);
    moving_ = {{id, TableLayout::Stock, dest}};
    stockReady_ = false;
    BeginAnimation(Animation::STOCK_MOVE, 0.50f);
}
void TableGame::ResolveMove() {
    const int incoming = moving_.front().id;
    const Rect arrived = moving_.front().to;
    const Player player = game_.CurrentPlayer();
    const std::size_t previousCount = game_.Captured(player).size();
    std::array<Rect, 48> previousField{};
    for (int id : game_.Field()) previousField[static_cast<std::size_t>(id)] = FieldRect(id);
    game_.Advance();
    if (pendingSlot_ >= 0) {
        slots_[static_cast<std::size_t>(incoming)] = pendingSlot_;
        if (pendingSlot_ >= 16) compactField_ = true;
    }
    moving_.clear();
    const auto& captured = game_.Captured(player);
    for (std::size_t i = previousCount; i < captured.size(); ++i) {
        int id = captured[i];
        moving_.push_back({id, id == incoming ? arrived : previousField[static_cast<std::size_t>(id)], CaptureRect(player)});
    }
    SyncField();
    if (!moving_.empty()) BeginAnimation(Animation::CAPTURE, 0.50f);
    else animation_ = Animation::NONE;
}
void TableGame::FinishAnimation() {
    switch (animation_) {
        case Animation::RETURN: case Animation::CAPTURE:
            animation_ = Animation::NONE; moving_.clear(); break;
        case Animation::CPU_FLIP: BeginAnimation(Animation::HAND_MOVE, 0.40f); break;
        case Animation::HAND_MOVE: case Animation::STOCK_MOVE:
            if (matched_) BeginAnimation(Animation::HIT_FREEZE, 0.05f);
            else ResolveMove();
            break;
        case Animation::HIT_FREEZE: ResolveMove(); break;
        case Animation::STOCK_FLIP:
            animation_ = Animation::NONE; moving_.clear(); stockReady_ = true; break;
        case Animation::NONE: break;
    }
}
void TableGame::RecordResult() {
    if (game_.GetPhase() == Phase::ROUND_END && !resultRecorded_) {
        for (std::size_t i = 0; i < scores_.size(); ++i) scores_[i] += game_.Result()->points[i];
        resultRecorded_ = true;
        finished_ = round_ == TableLayout::MatchRounds;
    }
}
void TableGame::Update(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("Invalid frame time");
    if (Busy()) {
        elapsed_ += seconds;
        if (elapsed_ >= duration_) FinishAnimation();
        return;
    }
    if (dragged_ || finished_) return;
    switch (game_.GetPhase()) {
        case Phase::REVEAL_DRAW:
            game_.Advance();
            moving_ = {{*game_.PendingCard(), TableLayout::Stock, TableLayout::Stock}};
            BeginAnimation(Animation::STOCK_FLIP, 0.50f); break;
        case Phase::AWAIT_DRAW_CHOICE:
            if (stockReady_ && game_.CurrentPlayer() == Player::OPPONENT) {
                const int id = game_.Choices().front(); game_.ChooseFieldCard(id); StartStockMove(id);
            }
            break;
        case Phase::RESOLVE_DRAW:
            if (stockReady_) {
                std::optional<int> target;
                const int id = *game_.PendingCard();
                for (int field : game_.Field()) if (DeckRegister::At(field).month == DeckRegister::At(id).month) { target = field; break; }
                StartStockMove(target);
            }
            break;
        case Phase::CHECK_YAKU: case Phase::NEXT_PLAYER:
            game_.Advance(); RecordResult(); break;
        case Phase::AWAIT_HAND_PLAY:
            if (game_.CurrentPlayer() == Player::OPPONENT) {
                cpuDelay_ += seconds;
                if (cpuDelay_ >= 0.35f) {
                    cpuDelay_ = 0;
                    const auto& hand = game_.Hand(Player::OPPONENT);
                    std::size_t chosen = 0; std::optional<int> target;
                    for (std::size_t i = 0; i < hand.size() && !target; ++i)
                        for (int field : game_.Field()) if (DeckRegister::At(field).month == DeckRegister::At(hand[i]).month) {
                            chosen = i; target = field; break;
                        }
                    StartHand(hand[chosen], HandRect(Player::OPPONENT, chosen), target);
                }
            }
            break;
        case Phase::AWAIT_DECISION:
            if (game_.CurrentPlayer() == Player::OPPONENT) { game_.ChooseDecision(Decision::SHOUBU); RecordResult(); }
            break;
        case Phase::ROUND_END: RecordResult(); break;
        default: break;
    }
}
void TableGame::Choose(Decision decision) {
    if (!Busy() && !finished_ && game_.GetPhase() == Phase::AWAIT_DECISION && game_.CurrentPlayer() == Player::PLAYER) {
        game_.ChooseDecision(decision); RecordResult();
    }
}
void TableGame::NextRound() {
    if (Busy() || finished_ || game_.GetPhase() != Phase::ROUND_END) return;
    game_.StartNextRound(); ++round_; resultRecorded_ = false; stockReady_ = false; cpuDelay_ = 0;
    Start();
}

std::vector<TableCard> TableGame::Cards() const {
    std::vector<TableCard> cards;
    const auto isMoving = [this](int id) { return std::any_of(moving_.begin(), moving_.end(), [id](const Moving& m){ return m.id == id; }); };
    for (Player p : {Player::OPPONENT, Player::PLAYER}) {
        const auto& hand = game_.Hand(p);
        for (std::size_t i = 0; i < hand.size(); ++i) {
            const int id = hand[i];
            if ((dragged_ && *dragged_ == id) || isMoving(id)) continue;
            cards.push_back({p == Player::PLAYER ? id : -1, HandRect(p, i)});
        }
    }
    if (game_.StockRemaining() > 0) cards.push_back({-1, TableLayout::Stock});
    for (int id : game_.Field()) cards.push_back({id, FieldRect(id)});
    for (Player p : {Player::OPPONENT, Player::PLAYER}) {
        const auto& pile = game_.Captured(p);
        if (!pile.empty() && !isMoving(pile.back())) cards.push_back({pile.back(), CaptureRect(p)});
    }
    if (dragged_) cards.push_back({*dragged_, dragRect_, 1, 0.008f});
    if (stockReady_ && game_.GetPhase() == Phase::AWAIT_DRAW_CHOICE) cards.push_back({*game_.PendingCard(), TableLayout::Stock, 1, 0.008f});
    for (const auto& m : moving_) {
        float t = duration_ > 0 ? std::min(1.0f, elapsed_ / duration_) : 1;
        int id = m.id; float flip = 1;
        Rect r = (animation_ == Animation::HIT_FREEZE) ? m.to :
                 (animation_ == Animation::CPU_FLIP ? m.from : Interpolate(m.from, m.to, t));
        if (animation_ == Animation::STOCK_FLIP || animation_ == Animation::CPU_FLIP) {
            flip = std::max(0.01f, std::abs(std::cos(t * 3.14159265f))); if (t < 0.5f) id = -1;
        }
        cards.push_back({id, r, flip, 0.009f});
    }
    return cards;
}
std::string TableGame::Status() const {
    if (game_.GetPhase() == Phase::ROUND_END) {
        if (finished_) {
            const int you = scores_[PlayerIndex(Player::PLAYER)], cpu = scores_[PlayerIndex(Player::OPPONENT)];
            return you == cpu ? "Match drawn" : (you > cpu ? "You won the match" : "CPU won the match");
        }
        if (!game_.Result()->winner) return "Round drawn";
        return (*game_.Result()->winner == Player::PLAYER ? "You won: +" : "CPU won: +") +
               std::to_string(game_.Result()->points[PlayerIndex(*game_.Result()->winner)]);
    }
    if (game_.GetPhase() == Phase::AWAIT_DECISION && game_.CurrentPlayer() == Player::PLAYER) {
        const int base = game_.CurrentScore().base;
        const auto award = CalculateSettlement(base, game_.HasCalledKoi(Player::OPPONENT), game_.KoiCalls(Player::PLAYER));
        return "Yaku " + std::to_string(base) + "  |  Shobu " + std::to_string(award.award) + " points";
    }
    if (game_.GetPhase() == Phase::AWAIT_DRAW_CHOICE && stockReady_ && game_.CurrentPlayer() == Player::PLAYER)
        return "Choose a matching field card";
    return game_.CurrentPlayer() == Player::PLAYER ? "Your turn" : "CPU turn";
}
