#include "TableGame.hpp"

#include "DeckRegister.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
using TableLayout::Point2;
using TableLayout::Rect;

constexpr float CardFlightSeconds = 0.60f;
constexpr float TableDropSeconds = 0.15f;
constexpr float HandSlideSeconds = 0.30f;
constexpr float CaptureFlightSeconds = 0.50f;
constexpr float CaptureHitFreezeSeconds = 0.50f;
// Round-start bullet time: the first second slows the table (deal, flights, cue sounds)
// while the logo beat plays, then the table eases back to full speed.
constexpr float BulletTimeSeconds = 1.0f;
constexpr float BulletTimeFloor = 0.25f;     // Table speed at the start of the beat.
constexpr float RoundIntroMinSeconds = 1.5f;
constexpr float DealCompleteSeconds = 1.40f; // Last deal card: 23 * 0.04 s stagger + 0.45 s flight.
float BulletTimeScale(float realElapsed) {
    const float t = std::clamp(realElapsed / BulletTimeSeconds, 0.0f, 1.0f);
    return BulletTimeFloor + (1.0f - BulletTimeFloor) * t * t * (3.0f - 2.0f * t);
}
constexpr std::array<Rect, 2> DealerCardRects{{
    {280.0f, 200.0f, 100.0f, 225.0f},
    {460.0f, 200.0f, 100.0f, 225.0f}
}};

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
struct StackOffset { float dx, dy, yaw; };
StackOffset StackJitter(int key, float xyScale = 1.0f, float yawScale = 1.0f) {
    std::uint32_t h = static_cast<std::uint32_t>(key) * 2654435761u;
    h ^= h >> 16;
    h *= 2246822519u;
    h ^= h >> 13;
    const float u = static_cast<float>(h & 0xFFu) / 255.0f - 0.5f;
    const float v = static_cast<float>((h >> 8) & 0xFFu) / 255.0f - 0.5f;
    const float w = static_cast<float>((h >> 16) & 0xFFu) / 255.0f - 0.5f;
    return {u * 3.8f * xyScale, v * 5.0f * xyScale, w * 0.13f * yawScale};
}
Rect JitteredRect(Rect base, StackOffset j) {
    return {base.x + j.dx, base.y + j.dy, base.width, base.height};
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
Player DrawDealer(std::uint32_t seed) { Deck deck(seed ^ 0x9e3779b9u); return DrawDealer(deck); }
std::array<int, 2> DrawDealerCards() {
    Deck deck;
    deck.Shuffle();
    for (;;) {
        const auto first = deck.Draw();
        const auto second = deck.Draw();
        if (!first || !second) { deck.Reset(); deck.Shuffle(); continue; }
        if (DetermineFirstDealer(first->id, second->id)) return {first->id, second->id};
    }
}
} // namespace

TableGame::TableGame() : game_(Player::PLAYER), openingStage_(OpeningStage::DEALER_PICK),
                         dealerCards_(DrawDealerCards()) {}
TableGame::TableGame(std::uint32_t seed) : game_(DrawDealer(seed), seed) { Start(); }

void TableGame::Start() {
    slots_.fill(-1);
    moving_.clear();
    handSlides_ = {};
    deferredCapture_.clear();
    animation_ = Animation::NONE;
    compactField_ = false;
    game_.Advance();
    const auto& field = game_.Field();
    constexpr std::array<int, 8> initial{2, 3, 4, 5, 10, 11, 12, 13};
    for (std::size_t i = 0; i < field.size(); ++i) slots_[static_cast<std::size_t>(field[i])] = initial[i];
    RecordResult();
}

void TableGame::StartRoundPresentation() {
    openingStage_ = OpeningStage::ROUND_INTRO;
    openingTimer_ = 0.0f;
    dealElapsed_ = 0.0f;
    openingDealCue_ = 0;
}

std::vector<GameSfx> TableGame::TakeSfx() {
    std::vector<GameSfx> result;
    result.swap(sfx_);
    return result;
}

bool TableGame::HasCardAt(Point2 p) const {
    const auto cards = Cards();
    return std::any_of(cards.rbegin(), cards.rend(), [p](const TableCard& card) {
        return Contains(card.rect, p);
    });
}

bool TableGame::AwaitingDealerChoice() const {
    return openingStage_ == OpeningStage::DEALER_PICK;
}
bool TableGame::DealerSelectionActive() const {
    return openingStage_ == OpeningStage::DEALER_PICK || openingStage_ == OpeningStage::DEALER_REVEAL;
}
bool TableGame::RoundIntroActive() const {
    return openingStage_ == OpeningStage::ROUND_INTRO;
}
float TableGame::RoundIntroBulletProgress() const {
    if (!RoundIntroActive()) return 1.0f;
    return std::clamp(openingTimer_ / BulletTimeSeconds, 0.0f, 1.0f);
}
void TableGame::SelectDealerCard(int index) {
    if (!AwaitingDealerChoice() || (index != 0 && index != 1)) return;
    dealerChoice_ = index;
    openingDealer_ = DetermineFirstDealer(dealerCards_[static_cast<std::size_t>(index)],
                                          dealerCards_[static_cast<std::size_t>(1 - index)]);
    openingStage_ = OpeningStage::DEALER_REVEAL;
    openingTimer_ = 0.0f;
    secondDealerFlipCue_ = false;
    sfx_.push_back(GameSfx::CARD_FLIP);
}

Rect TableGame::HandRect(Player player, std::size_t i) const {
    const auto& layout = player == Player::PLAYER ? TableLayout::PlayerHand : TableLayout::OpponentHand;
    const auto& slide = handSlides_[PlayerIndex(player)];
    if (i >= slide.gap && slide.elapsed < HandSlideSeconds) {
        return Interpolate(layout.at(i + 1), layout.at(i), slide.elapsed / HandSlideSeconds);
    }
    return layout.at(i);
}
Rect TableGame::CaptureRect(Player player) const {
    return player == Player::PLAYER ? TableLayout::PlayerCapture : TableLayout::OpponentCapture;
}
float TableGame::StockHeight() const {
    const std::size_t count = game_.StockRemaining();
    if (count == 0) return 0.0f;
    return TableLayout::CardThickness +
           static_cast<float>(std::min<std::size_t>(count, 24) - 1) * TableLayout::StackStep;
}
float TableGame::CaptureHeight(Player player, std::size_t countOffset) const {
    const std::size_t raw = game_.Captured(player).size();
    const std::size_t count = raw > countOffset ? raw - countOffset : 0;
    if (count == 0) return 0.0f;
    return TableLayout::CardThickness +
           static_cast<float>(std::min<std::size_t>(count, 24) - 1) * (TableLayout::StackStep * 0.85f);
}
Rect TableGame::FieldRect(CardId id) const {
    if (id < 0 || id >= 48 || slots_[static_cast<std::size_t>(id)] < 0) throw std::out_of_range("Card not on field");
    return TableLayout::FieldSlot(static_cast<std::size_t>(slots_[static_cast<std::size_t>(id)]), compactField_);
}
int TableGame::FindFreeSlot(Point2 near) const {
    std::array<bool, TableLayout::FieldSlotCount> used{};
    for (int slot : slots_) if (slot >= 0) used[static_cast<std::size_t>(slot)] = true;
    const bool compact = compactField_ || game_.Field().size() >= 16;

    // Deferred captures are already removed from the rules field, but remain
    // visibly on the table until the stock card resolves. Their positions are
    // therefore occupied and must not be offered to an unmatched stock card.
    for (std::size_t i = 0; i < used.size(); ++i) {
        const Rect candidate = TableLayout::FieldSlot(i, compact);
        for (const auto& deferred : deferredCapture_) {
            if (Contains(candidate, Center(deferred.rect))) {
                used[i] = true;
                break;
            }
        }
    }

    float best = std::numeric_limits<float>::infinity(); int found = -1;
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
        slots_[static_cast<std::size_t>(id)] = FindFreeSlot({431.0f, 355.0f});
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
    switch (type) {
        case Animation::CPU_FLIP: case Animation::STOCK_FLIP: sfx_.push_back(GameSfx::CARD_FLIP); break;
        case Animation::CARD_FLIGHT: sfx_.push_back(GameSfx::CARD_DEAL); break;
        case Animation::HAND_MOVE: case Animation::STOCK_MOVE: sfx_.push_back(GameSfx::CARD_PLACE); break;
        case Animation::HIT_FREEZE: sfx_.push_back(GameSfx::CARD_MATCH); break;
        case Animation::CAPTURE: sfx_.push_back(GameSfx::CARD_CAPTURE); break;
        default: break;
    }
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
        dragOffset_ = {p.x - dragRect_.x, p.y - dragRect_.y};
        dragDeltaX_ = dragDeltaY_ = dragVelX_ = dragVelY_ = dragPitch_ = dragRoll_ = 0.0f;
        return;
    }
}
void TableGame::PointerMove(Point2 p) {
    if (dragged_) {
        const float nx = p.x - dragOffset_.x;
        const float ny = p.y - dragOffset_.y;
        dragDeltaX_ += nx - dragRect_.x;
        dragDeltaY_ += ny - dragRect_.y;
        dragRect_.x = nx;
        dragRect_.y = ny;
    }
}
void TableGame::CancelDrag() {
    if (!dragged_) return;
    const auto& hand = game_.Hand(Player::PLAYER);
    auto it = std::find(hand.begin(), hand.end(), *dragged_);
    if (it != hand.end()) {
        playerSnap_ = false;
        moving_ = {{*dragged_, dragRect_, HandRect(Player::PLAYER, static_cast<std::size_t>(it - hand.begin())),
                    TableLayout::CardElevation + 0.042f, TableLayout::CardElevation}};
        BeginAnimation(Animation::RETURN, CardFlightSeconds);
    }
    dragged_.reset();
    dragDeltaX_ = dragDeltaY_ = dragVelX_ = dragVelY_ = dragPitch_ = dragRoll_ = 0.0f;
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
    dragDeltaX_ = dragDeltaY_ = dragVelX_ = dragVelY_ = dragPitch_ = dragRoll_ = 0.0f;
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
    landingElevation_ = TableLayout::CardElevation +
        (matched_ ? TableLayout::CardThickness + 0.001f : 0.0f);
    stockPlacement_ = false;
    if (game_.CurrentPlayer() == Player::OPPONENT) {
        playerSnap_ = false;
        moving_ = {{id, from, dest, TableLayout::CardElevation, landingElevation_ + 0.018f}};
        BeginAnimation(Animation::CPU_FLIP, 0.30f);
    } else {
        // The player already performed the flight by dragging the card. Only
        // the shared, snappy table-drop phase remains after release.
        playerSnap_ = false;
        moving_ = {{id, from, dest, TableLayout::CardElevation + 0.042f, landingElevation_}};
        BeginAnimation(Animation::HAND_MOVE, TableDropSeconds);
    }
}
void TableGame::StartStockMove(std::optional<CardId> target) {
    const int id = *game_.PendingCard();
    matched_ = target.has_value();
    pendingSlot_ = matched_ ? -1 : FindFreeSlot({431.0f, 355.0f});
    const Rect dest = target ? FieldRect(*target) : TableLayout::FieldSlot(static_cast<std::size_t>(pendingSlot_), compactField_ || game_.Field().size() >= 16);
    const float fromElev = TableLayout::CardElevation + StockHeight();
    landingElevation_ = TableLayout::CardElevation +
        (matched_ ? TableLayout::CardThickness + 0.001f : 0.0f);
    stockPlacement_ = true;
    const auto j = StackJitter(100 + static_cast<int>(game_.StockRemaining()), 0.9f, 0.9f);
    playerSnap_ = false;
    moving_ = {{id, JitteredRect(TableLayout::Stock, j), dest, fromElev,
                landingElevation_ + 0.018f, j.yaw, 0.0f}};
    stockReady_ = false;
    BeginAnimation(Animation::CARD_FLIGHT, CardFlightSeconds);
}
void TableGame::ResolveMove() {
    const Phase resolvedPhase = game_.GetPhase();
    const int incoming = moving_.front().id;
    const Rect arrived = moving_.front().to;
    const Player player = game_.CurrentPlayer();
    const std::size_t previousCount = game_.Captured(player).size();
    std::array<Rect, 48> previousField{};
    for (int id : game_.Field()) previousField[static_cast<std::size_t>(id)] = FieldRect(id);

    // Remember the slot before the rules remove the committed card.
    const auto& hand = game_.Hand(player);
    const auto played = std::find(hand.begin(), hand.end(), incoming);
    const std::size_t gap = static_cast<std::size_t>(played - hand.begin());
    game_.Advance();
    if (resolvedPhase == Phase::RESOLVE_HAND) {
        handSlides_[PlayerIndex(player)] = {gap, 0.0f};
    }
    if (pendingSlot_ >= 0) {
        slots_[static_cast<std::size_t>(incoming)] = pendingSlot_;
        if (pendingSlot_ >= 16) compactField_ = true;
    }
    moving_.clear();

    // Captures from the hand card remain visibly at their field contact point.
    // They are collected together with any stock-card capture only after the
    // stock card has flipped, flown, landed, and completed its own hit freeze.
    const auto& captured = game_.Captured(player);
    for (std::size_t i = previousCount; i < captured.size(); ++i) {
        const int id = captured[i];
        const bool isIncoming = id == incoming;
        deferredCapture_.push_back({id,
            isIncoming ? arrived : previousField[static_cast<std::size_t>(id)],
            TableLayout::CardElevation + (isIncoming ? TableLayout::CardThickness + 0.001f : 0.0f),
            0.0f});
    }

    SyncField();
    animation_ = Animation::NONE;
    if (resolvedPhase == Phase::RESOLVE_DRAW && !deferredCapture_.empty()) {
        StartDeferredCapture(player);
    }
}

void TableGame::StartDeferredCapture(Player player) {
    const auto& captured = game_.Captured(player);
    constexpr float capStep = TableLayout::StackStep * 0.85f;
    moving_.clear();
    moving_.reserve(deferredCapture_.size());

    for (const auto& deferred : deferredCapture_) {
        const auto it = std::find(captured.begin(), captured.end(), deferred.id);
        if (it == captured.end()) throw std::logic_error("Deferred capture missing from captured pile");
        const std::size_t index = static_cast<std::size_t>(it - captured.begin());
        const float toElev = TableLayout::CardElevation +
            static_cast<float>(std::min<std::size_t>(index, 23)) * capStep;
        const auto jitter = StackJitter(deferred.id, 1.25f, 1.45f);
        moving_.push_back({deferred.id, deferred.rect,
                           JitteredRect(CaptureRect(player), jitter),
                           deferred.elevation, toElev, deferred.yaw, jitter.yaw});
    }
    deferredCapture_.clear();
    BeginAnimation(Animation::CAPTURE, CaptureFlightSeconds);
}
void TableGame::FinishAnimation() {
    switch (animation_) {
        case Animation::RETURN: case Animation::CAPTURE:
            animation_ = Animation::NONE; moving_.clear(); break;
        case Animation::CPU_FLIP:
            BeginAnimation(Animation::CARD_FLIGHT, CardFlightSeconds);
            break;
        case Animation::CARD_FLIGHT:
            for (auto& card : moving_) {
                card.from = card.to;
                card.fromElev = card.toElev;
                card.fromYaw = card.toYaw;
                card.toElev = landingElevation_;
            }
            BeginAnimation(stockPlacement_ ? Animation::STOCK_MOVE : Animation::HAND_MOVE,
                           TableDropSeconds);
            break;
        case Animation::HAND_MOVE: case Animation::STOCK_MOVE:
            if (matched_) BeginAnimation(Animation::HIT_FREEZE, CaptureHitFreezeSeconds);
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
        sfx_.push_back(game_.Result()->points[0] > game_.Result()->points[1]
                           ? GameSfx::ROUND_WIN : GameSfx::ROUND_LOSE);
        resultRecorded_ = true;
        finished_ = round_ == TableLayout::MatchRounds;
    }
}
void TableGame::Update(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("Invalid frame time");
    // Keep sliding during stock flips, flights and capture animations as well.
    for (auto& slide : handSlides_) {
        slide.elapsed = std::min(HandSlideSeconds, slide.elapsed + seconds);
    }
    if (openingStage_ == OpeningStage::DEALER_PICK) return;
    if (openingStage_ == OpeningStage::DEALER_REVEAL) {
        openingTimer_ += seconds;
        if (!secondDealerFlipCue_ && openingTimer_ >= 0.45f) {
            secondDealerFlipCue_ = true;
            sfx_.push_back(GameSfx::CARD_FLIP);
        }
        if (openingTimer_ >= 3.4f) {
            game_ = KoiKoiStateMachine(*openingDealer_);
            Start();
            StartRoundPresentation();
        }
        return;
    }
    if (openingStage_ == OpeningStage::ROUND_INTRO) {
        // Real time drives the logo beat; the deal advances in table time, which is slowed during bullet time.
        const float tableSeconds = seconds * BulletTimeScale(openingTimer_);
        openingTimer_ += seconds;
        dealElapsed_ += tableSeconds;
        constexpr int OpeningCardCount = 24;
        while (openingDealCue_ < OpeningCardCount &&
               dealElapsed_ >= static_cast<float>(openingDealCue_) * 0.04f) {
            sfx_.push_back(GameSfx::CARD_DEAL);
            ++openingDealCue_;
        }
        if (openingTimer_ >= RoundIntroMinSeconds && dealElapsed_ >= DealCompleteSeconds)
            openingStage_ = OpeningStage::PLAYING;
        return;
    }
    if (Busy()) {
        elapsed_ += seconds;
        if (elapsed_ >= duration_) FinishAnimation();
        return;
    }
    if (dragged_) {
        if (seconds > 0.0f) {
            const float dt = std::max(seconds, 1.0f / 120.0f);
            const float vx = dragDeltaX_ / dt;
            const float vy = dragDeltaY_ / dt;
            const float blend = 1.0f - std::exp(-18.0f * seconds);
            dragVelX_ += (vx - dragVelX_) * blend;
            dragVelY_ += (vy - dragVelY_) * blend;
            dragRoll_ = std::clamp(dragVelX_ * 0.00022f, -0.24f, 0.24f);
            dragPitch_ = std::clamp(-dragVelY_ * 0.00020f, -0.20f, 0.20f);
            dragDeltaX_ = dragDeltaY_ = 0.0f;
        }
        return;
    }
    if (finished_) return;
    switch (game_.GetPhase()) {
        case Phase::REVEAL_DRAW: {
            game_.Advance();
            const float stockTop = TableLayout::CardElevation + StockHeight();
            const auto j = StackJitter(100 + static_cast<int>(game_.StockRemaining()), 0.9f, 0.9f);
            const Rect r = JitteredRect(TableLayout::Stock, j);
            moving_ = {{*game_.PendingCard(), r, r, stockTop, stockTop, j.yaw, j.yaw}};
            BeginAnimation(Animation::STOCK_FLIP, 0.50f); break;
        }
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
        sfx_.push_back(decision == Decision::KOI_KOI ? GameSfx::KOI_KOI : GameSfx::SHOUBU);
        game_.ChooseDecision(decision); RecordResult();
    }
}
void TableGame::NextRound() {
    if (Busy() || finished_ || game_.GetPhase() != Phase::ROUND_END) return;
    game_.StartNextRound(); ++round_; resultRecorded_ = false; stockReady_ = false; cpuDelay_ = 0;
    Start();
    StartRoundPresentation();
}

std::vector<TableCard> TableGame::Cards() const {
    std::vector<TableCard> cards;
    if (DealerSelectionActive()) {
        constexpr float Pi = 3.14159265f;
        for (std::size_t i = 0; i < dealerCards_.size(); ++i) {
            float progress = 0.0f;
            if (dealerChoice_ >= 0) {
                const bool chosen = static_cast<int>(i) == dealerChoice_;
                progress = std::clamp((openingTimer_ - (chosen ? 0.0f : 0.45f)) / 0.45f, 0.0f, 1.0f);
            }
            const float eased = progress * progress * (3.0f - 2.0f * progress);
            const float angle = Pi * (1.0f - eased);
            TableCard card{progress < 0.5f ? -1 : dealerCards_[i], DealerCardRects[i]};
            card.flip = std::max(0.01f, std::abs(std::cos(angle)));
            card.flipAngle = angle;
            card.elevation = TableLayout::CardElevation + 0.025f + 0.025f * std::sin(Pi * progress);
            card.thickness = TableLayout::CardThickness;
            cards.push_back(card);
        }
        return cards;
    }
    const auto isMoving = [this](int id) { return std::any_of(moving_.begin(), moving_.end(), [id](const Moving& m){ return m.id == id; }); };
    const auto isDeferred = [this](int id) {
        return std::any_of(deferredCapture_.begin(), deferredCapture_.end(),
                           [id](const DeferredCapture& capture) { return capture.id == id; });
    };
    for (Player p : {Player::OPPONENT, Player::PLAYER}) {
        const auto& hand = game_.Hand(p);
        for (std::size_t i = 0; i < hand.size(); ++i) {
            const int id = hand[i];
            if ((dragged_ && *dragged_ == id) || isMoving(id)) continue;
            cards.push_back({p == Player::PLAYER ? id : -1, HandRect(p, i)});
        }
    }
    if (const std::size_t stockCount = game_.StockRemaining(); stockCount > 0) {
        constexpr std::size_t maxLayers = 10;
        const std::size_t visible = std::min(stockCount, maxLayers);
        const std::size_t start = stockCount - visible;
        if (start > 0) {
            const auto baseJ = StackJitter(100, 0.9f, 0.9f);
            TableCard baseBlock{-1, JitteredRect(TableLayout::Stock, baseJ)};
            baseBlock.elevation = TableLayout::CardElevation;
            baseBlock.thickness = static_cast<float>(start) * TableLayout::StackStep - 0.0001f;
            baseBlock.yaw = baseJ.yaw;
            baseBlock.shadow = true;
            cards.push_back(baseBlock);
        }
        for (std::size_t k = start; k < stockCount; ++k) {
            const auto j = StackJitter(100 + static_cast<int>(k), 0.9f, 0.9f);
            const bool isTop = (k + 1 == stockCount);
            TableCard layer{-1, JitteredRect(TableLayout::Stock, j)};
            layer.elevation = TableLayout::CardElevation + static_cast<float>(k) * TableLayout::StackStep;
            layer.thickness = isTop ? TableLayout::CardThickness : (TableLayout::StackStep - 0.0001f);
            layer.yaw = j.yaw;
            layer.shadow = (k == 0 || isTop);
            cards.push_back(layer);
        }
    }
    for (int id : game_.Field()) cards.push_back({id, FieldRect(id)});
    for (Player p : {Player::OPPONENT, Player::PLAYER}) {
        const auto& pile = game_.Captured(p);
        std::vector<int> settled;
        settled.reserve(pile.size());
        for (int cid : pile) if (!isMoving(cid) && !isDeferred(cid)) settled.push_back(cid);
        if (settled.empty()) continue;

        constexpr float capStep = TableLayout::StackStep * 0.85f;
        constexpr std::size_t maxCapLayers = 8;
        const std::size_t count = settled.size();
        const std::size_t visible = std::min(count, maxCapLayers);
        const std::size_t start = count - visible;
        const Rect baseRect = CaptureRect(p);
        if (start > 0) {
            const auto baseJ = StackJitter(settled[0], 1.25f, 1.45f);
            TableCard baseBlock{settled[start - 1], JitteredRect(baseRect, baseJ)};
            baseBlock.elevation = TableLayout::CardElevation;
            baseBlock.thickness = static_cast<float>(start) * capStep - 0.0001f;
            baseBlock.yaw = baseJ.yaw;
            baseBlock.shadow = true;
            cards.push_back(baseBlock);
        }
        for (std::size_t k = start; k < count; ++k) {
            const int cid = settled[k];
            const auto j = StackJitter(cid, 1.25f, 1.45f);
            const bool isTop = (k + 1 == count);
            TableCard layer{cid, JitteredRect(baseRect, j)};
            layer.elevation = TableLayout::CardElevation + static_cast<float>(std::min<std::size_t>(k, 23)) * capStep;
            layer.thickness = isTop ? TableLayout::CardThickness : (capStep - 0.0001f);
            layer.yaw = j.yaw;
            layer.shadow = (k == 0 || isTop);
            cards.push_back(layer);
        }
    }
    if (dragged_) {
        TableCard draggedCard{*dragged_, dragRect_};
        draggedCard.elevation = TableLayout::CardElevation + 0.042f;
        draggedCard.pitch = dragPitch_;
        draggedCard.roll = dragRoll_;
        cards.push_back(draggedCard);
    }
    if (stockReady_ && game_.GetPhase() == Phase::AWAIT_DRAW_CHOICE) {
        const auto j = StackJitter(100 + static_cast<int>(game_.StockRemaining()), 0.9f, 0.9f);
        TableCard pending{*game_.PendingCard(), JitteredRect(TableLayout::Stock, j)};
        pending.elevation = TableLayout::CardElevation + StockHeight() + 0.002f;
        pending.yaw = j.yaw;
        cards.push_back(pending);
    }
    for (const auto& deferred : deferredCapture_) {
        TableCard held{deferred.id, deferred.rect};
        held.elevation = deferred.elevation;
        held.yaw = deferred.yaw;
        cards.push_back(held);
    }
    constexpr float Pi = 3.14159265f;
    for (const auto& m : moving_) {
        const float t = duration_ > 0 ? std::min(1.0f, elapsed_ / duration_) : 1.0f;
        int id = m.id;
        float flip = 1.0f;
        float flipAngle = 0.0f;
        float pitch = 0.0f;
        float roll = 0.0f;
        float yaw = m.toYaw;
        float elev = m.toElev;
        Rect r = m.to;

        if (animation_ == Animation::HIT_FREEZE) {
            r = m.to;
            elev = m.toElev;
            yaw = m.toYaw;
        } else if (animation_ == Animation::STOCK_FLIP || animation_ == Animation::CPU_FLIP) {
            r = m.from;
            const float eased = t * t * (3.0f - 2.0f * t);
            flipAngle = Pi * (1.0f - eased);
            flip = std::max(0.01f, std::abs(std::cos(flipAngle)));
            if (t < 0.5f) id = -1;
            elev = m.fromElev + 0.022f * std::sin(Pi * t);
            yaw = m.fromYaw + (m.toYaw - m.fromYaw) * eased;
        } else {
            const bool tableDrop = animation_ == Animation::HAND_MOVE ||
                                   animation_ == Animation::STOCK_MOVE;
            r = Interpolate(m.from, m.to, t);
            const float easeT = t * t * (3.0f - 2.0f * t);
            const float arc = tableDrop ? 0.0f
                : (animation_ == Animation::RETURN ? 0.018f : 0.052f) * std::sin(Pi * t);
            elev = m.fromElev + (m.toElev - m.fromElev) * easeT + arc;
            yaw = m.fromYaw + (m.toYaw - m.fromYaw) * easeT;
            const float dx = m.to.x - m.from.x;
            const float dy = m.to.y - m.from.y;
            const float tiltEnv = std::sin(Pi * t);
            roll = std::clamp(dx * 0.00045f, -0.18f, 0.18f) * tiltEnv;
            pitch = std::clamp(-dy * 0.00045f, -0.18f, 0.18f) * tiltEnv;
        }

        TableCard movingCard{id, r, flip, elev, flipAngle, pitch, roll, yaw, TableLayout::CardThickness, true};
        cards.push_back(movingCard);
    }
    if (openingStage_ == OpeningStage::ROUND_INTRO) {
        constexpr float Pi = 3.14159265f;
        constexpr float DealFlight = 0.45f;
        constexpr float DealStep = 0.04f;
        std::size_t dealIndex = 0;
        for (auto& card : cards) {
            const bool stockCard = std::abs(card.rect.x - TableLayout::Stock.x) < 12.0f &&
                                   std::abs(card.rect.y - TableLayout::Stock.y) < 12.0f;
            if (stockCard) continue;
            const int faceId = card.id;
            const float delay = static_cast<float>(dealIndex++) * DealStep;
            const float t = std::clamp((dealElapsed_ - delay) / DealFlight, 0.0f, 1.0f);
            card.rect = Interpolate(TableLayout::Stock, card.rect, t);
            card.elevation += 0.045f * std::sin(Pi * t);
            if (faceId >= 0) {
                const float flip = std::clamp((t - 0.64f) / 0.36f, 0.0f, 1.0f);
                const float eased = flip * flip * (3.0f - 2.0f * flip);
                card.flipAngle = Pi * (1.0f - eased);
                card.flip = std::max(0.01f, std::abs(std::cos(card.flipAngle)));
                if (flip < 0.5f) card.id = -1;
            }
        }
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
