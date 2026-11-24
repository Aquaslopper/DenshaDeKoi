#include "TableGame.hpp"
#include "DeckRegister.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using TableLayout::Point2;
using TableLayout::Rect;
void Check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
Point2 Center(Rect r) { return {r.x+r.width/2, r.y+r.height/2}; }
void TestOpeningPresentation() {
    TableGame game;
    Check(game.AwaitingDealerChoice() && game.DealerSelectionActive() && game.Busy(),
          "New match must wait for dealer card selection");
    const auto dealerBacks = game.Cards();
    Check(dealerBacks.size() == 2 && dealerBacks[0].id == -1 && dealerBacks[1].id == -1 &&
          dealerBacks[0].thickness == TableLayout::CardThickness,
          "Dealer choice must use two real 3D card backs");
    game.SelectDealerCard(0);
    Check(!game.AwaitingDealerChoice() && game.DealerSelectionActive(),
          "Dealer selection did not begin reveal");
    game.Update(3.39f);
    Check(game.DealerSelectionActive(), "Dealer reveal ended before 3.4 seconds");
    game.Update(0.02f);
    Check(game.RoundIntroActive() && game.Busy(), "Deal and match intro did not start");
    game.Update(1.49f);
    Check(game.RoundIntroActive() && game.Busy(), "Round intro ended during its bullet-time beat");
    for (int i = 0; i < 300 && game.RoundIntroActive(); ++i) game.Update(0.01f);
    Check(!game.RoundIntroActive() && !game.Busy() && game.Rules().ValidateConservation(),
          "Opening presentation did not release gameplay");
}

// Round-start bullet time: a one-second logo beat in real time, a slowed deal during it,
// then normal speed until all 24 dealt cards have landed.
void TestRoundIntroBulletTime() {
    TableGame game;
    game.SelectDealerCard(0);
    game.Update(3.4f);
    Check(game.RoundIntroActive(), "Round intro did not start after the dealer reveal");
    Check(game.RoundIntroBulletProgress() == 0.0f, "Bullet-time beat must start at zero");
    Check(game.RoundIntroCueOpacity() == 1.0f, "Logo and cue must start fully visible");
    float real = 0.0f;
    float beatEnd = -1.0f;
    int dealCues = 0;
    int fadingFrames = 0;
    float previous = 0.0f;
    float previousOpacity = 1.0f;
    while (game.RoundIntroActive() && real < 3.0f) {
        game.Update(0.01f);
        real += 0.01f;
        const float progress = game.RoundIntroBulletProgress();
        Check(progress >= previous && progress <= 1.0f, "Bullet-time progress must be monotonic and clamped");
        previous = progress;
        if (beatEnd < 0.0f && progress >= 1.0f) beatEnd = real;
        const float opacity = game.RoundIntroCueOpacity();
        Check(opacity <= previousOpacity && opacity >= 0.0f, "Cue opacity must only fade out");
        if (opacity > 0.0f && opacity < 1.0f) ++fadingFrames;
        previousOpacity = opacity;
        for (auto sfx : game.TakeSfx()) if (sfx == GameSfx::CARD_DEAL) ++dealCues;
    }
    Check(beatEnd > 0.98f && beatEnd < 1.02f, "Logo beat should last one real second");
    Check(real > 1.6f && real < 1.9f, "Slowed deal should end the intro shortly after the beat");
    Check(dealCues == 24, "Every dealt card must produce one deal cue");
    Check(fadingFrames > 0, "Logo and cue must fade out together before play resumes");
    Check(!game.RoundIntroActive() && game.RoundIntroBulletProgress() == 1.0f && game.RoundIntroCueOpacity() == 0.0f,
          "Bullet time must be over once play resumes");
}

void TestProjection() {
    for (float x : {0.f, 150.f, 420.f, 840.f}) for (float y : {0.f, 200.f, 500.f, 720.f}) {
        const auto p = TableLayout::Unproject(TableLayout::Project(TableLayout::OnPlane(x, y, TableLayout::CardElevation)));
        Check(std::abs(p.x-x)<0.02f && std::abs(p.y-y)<0.02f, "Projection inverse is incorrect");
    }
}
void PlayFirst(TableGame& game) {
    const auto& rules = game.Rules();
    const auto& hand = rules.Hand(Player::PLAYER);
    const int id = hand.front();
    std::optional<int> target;
    for (int f : rules.Field()) if (DeckRegister::At(f).month == DeckRegister::At(id).month) { target = f; break; }
    Check(game.HasCardAt(Center(TableLayout::PlayerHand[0])),
          "Magnification hit test did not find the player card");
    game.PointerDown(Center(TableLayout::PlayerHand[0]));
    Check(game.Dragging(), "Could not grab hand card");
    const auto destination = target ? Center(game.FieldRect(*target)) : Point2{400, 500};
    game.PointerMove(destination);
    game.PointerUp(destination);
    Check(game.Busy() && !game.Dragging(), "Card play did not start animation");
}
void TestReturnAndFreeze() {
    for (unsigned seed = 1; seed < 500; ++seed) {
        TableGame game(seed);
        if (game.Rules().GetPhase() != Phase::AWAIT_HAND_PLAY || game.Rules().CurrentPlayer() != Player::PLAYER) continue;
        auto hand = game.Rules().Hand(Player::PLAYER);
        auto field = game.Rules().Field();
        int id = hand[0];
        auto match = std::find_if(field.begin(), field.end(), [id](int f){ return f/4 == id/4; });
        if (match == field.end()) continue;
        game.PointerDown(Center(TableLayout::PlayerHand[0]));
        Check(game.Cards().back().elevation > TableLayout::CardElevation + 0.03f, "Dragged card must lift above table");
        game.PointerMove({Center(TableLayout::PlayerHand[0]).x + 60.0f, Center(TableLayout::PlayerHand[0]).y - 80.0f});
        game.Update(0.016f);
        Check(std::abs(game.Cards().back().roll) > 0.01f && std::abs(game.Cards().back().pitch) > 0.01f,
              "Dragged card must tilt with pointer motion");
        game.Update(0.5f);
        Check(std::abs(game.Cards().back().roll) < 0.01f && std::abs(game.Cards().back().pitch) < 0.01f,
              "Dragged card must level out when stationary");
        game.PointerUp({0, 0});
        Check(game.Busy() && game.Rules().Hand(Player::PLAYER).size() == 8, "Invalid drop must return without playing");
        game.Update(0.59f);
        Check(game.Busy(), "Return flight shorter than 600 ms");
        game.Update(0.02f);
        Check(!game.Busy() && game.Rules().Hand(Player::PLAYER).size() == 8, "Return animation changed state");
        PlayFirst(game);
        game.Update(0.14f);
        Check(game.Busy() && game.Rules().Hand(Player::PLAYER).size() == 8,
              "Player table drop must last 150 ms");
        game.Update(0.02f); // table drop ends; 500 ms capture hit freeze begins
        Check(game.Busy() && game.Rules().Hand(Player::PLAYER).size() == 8, "Capture committed before hit freeze");
        Check(game.Cards().back().elevation > TableLayout::CardElevation + TableLayout::CardThickness * 0.9f,
              "Landed card must rest on top of its match during hit freeze");
        game.Update(0.499f);
        Check(game.Rules().Hand(Player::PLAYER).size() == 8, "Hit freeze shorter than 500 ms");
        game.Update(0.002f);
        Check(game.Rules().Hand(Player::PLAYER).size() == 7 && game.Rules().ValidateConservation(),
              "Hand capture did not resolve after hit freeze");

        int prematureCaptureLayers = 0;
        for (const auto& card : game.Cards()) {
            if (std::abs(card.rect.x - TableLayout::PlayerCapture.x) < 10.0f &&
                std::abs(card.rect.y - TableLayout::PlayerCapture.y) < 10.0f) ++prematureCaptureLayers;
        }
        Check(prematureCaptureLayers == 0,
              "Hand capture moved to player pile before the stock card resolved");

        for (int step = 0; step < 100; ++step) {
            if (!game.Busy() && game.Rules().GetPhase() == Phase::AWAIT_DRAW_CHOICE) {
                game.PointerDown(Center(game.FieldRect(game.Rules().Choices().front())));
            }
            game.Update(0.1f);
            const Phase phase = game.Rules().GetPhase();
            if (!game.Busy() && phase != Phase::REVEAL_DRAW && phase != Phase::AWAIT_DRAW_CHOICE &&
                phase != Phase::RESOLVE_DRAW) break;
        }
        Check(!game.Busy() && game.Rules().GetPhase() != Phase::REVEAL_DRAW &&
              game.Rules().GetPhase() != Phase::RESOLVE_DRAW,
              "Deferred capture did not finish after stock-card resolution");
        int captureLayers = 0;
        bool captureCrooked = false;
        for (const auto& c : game.Cards()) {
            if (c.id >= 0 && std::abs(c.rect.x - TableLayout::PlayerCapture.x) < 10.0f &&
                std::abs(c.rect.y - TableLayout::PlayerCapture.y) < 10.0f) {
                ++captureLayers;
                if (std::abs(c.yaw) > 0.005f || std::abs(c.rect.x - TableLayout::PlayerCapture.x) > 0.1f)
                    captureCrooked = true;
            }
        }
        Check(captureLayers >= 2 && captureCrooked, "Capture pile must stack crookedly");
        return;
    }
    throw std::runtime_error("Did not find matching fixture");
}
void TestFlips() {
    bool cpuChecked = false, stockChecked = false;
    for (unsigned seed = 1; seed < 1000 && (!cpuChecked || !stockChecked); ++seed) {
        TableGame game(seed);
        if (game.Rules().GetPhase() != Phase::AWAIT_HAND_PLAY) continue;
        if (!cpuChecked && game.Rules().CurrentPlayer() == Player::OPPONENT) {
            game.Update(0.4f);
            Check(game.Busy() && game.Cards().back().id == -1 && game.Cards().back().flipAngle > 3.0f,
                  "CPU flip must start on black back");
            game.Update(0.15f);
            Check(game.Cards().back().flip < 0.02f && game.Cards().back().id >= 0 &&
                  std::abs(game.Cards().back().flipAngle - 1.5707963f) < 0.05f &&
                  game.Cards().back().elevation > TableLayout::CardElevation + 0.01f,
                  "CPU flip midpoint incorrect");
            cpuChecked = true;
        }
        if (!stockChecked && game.Rules().CurrentPlayer() == Player::PLAYER) {
            float initialStockTop = 0.0f;
            int stockLayers = 0;
            bool hasYawVariation = false;
            for (const auto& c : game.Cards()) {
                if (c.id == -1 && std::abs(c.rect.x - TableLayout::Stock.x) < 10.0f &&
                    std::abs(c.rect.y - TableLayout::Stock.y) < 10.0f) {
                    initialStockTop = std::max(initialStockTop, c.elevation + c.thickness);
                    ++stockLayers;
                    if (std::abs(c.yaw) > 0.005f) hasYawVariation = true;
                }
            }
            Check(stockLayers > 2 && hasYawVariation && initialStockTop > TableLayout::CardElevation + TableLayout::CardThickness * 2.0f,
                  "Stock pile must have lifelike crooked 3D stack layers");
            PlayFirst(game);
            for (int i = 0; i < 30 && game.Rules().GetPhase() != Phase::REVEAL_DRAW; ++i) game.Update(0.1f);
            Check(game.Rules().GetPhase() == Phase::REVEAL_DRAW, "No mandatory draw after hand play");
            while (game.Busy()) game.Update(0.1f);
            game.Update(0);
            Check(game.Busy() && game.Cards().back().id == -1 && game.Cards().back().flipAngle > 3.0f,
                  "Stock flip must start on black back");
            game.Update(0.25f);
            Check(game.Cards().back().flip < 0.02f && game.Cards().back().id >= 0 &&
                  std::abs(game.Cards().back().flipAngle - 1.5707963f) < 0.05f,
                  "Stock flip midpoint incorrect");
            float nextStockTop = 0.0f;
            for (const auto& c : game.Cards()) {
                if (c.id == -1 && std::abs(c.rect.x - TableLayout::Stock.x) < 10.0f &&
                    std::abs(c.rect.y - TableLayout::Stock.y) < 10.0f) {
                    nextStockTop = std::max(nextStockTop, c.elevation + c.thickness);
                }
            }
            Check(nextStockTop < initialStockTop, "Stock stack height must shrink as cards are drawn");
            stockChecked = true;
        }
    }
    Check(cpuChecked && stockChecked, "Could not exercise both flip types");
}
void TestStockAvoidsDeferredCapture() {
    for (unsigned seed = 1; seed < 2000; ++seed) {
        TableGame game(seed);
        if (game.Rules().GetPhase() != Phase::AWAIT_HAND_PLAY ||
            game.Rules().CurrentPlayer() != Player::PLAYER) continue;

        const auto hand = game.Rules().Hand(Player::PLAYER);
        const auto field = game.Rules().Field();
        std::size_t handIndex = 0;
        std::optional<int> target;
        for (; handIndex < hand.size() && !target; ++handIndex) {
            for (int fieldCard : field) {
                if (DeckRegister::At(fieldCard).month == DeckRegister::At(hand[handIndex]).month) {
                    target = fieldCard;
                    break;
                }
            }
        }
        if (!target) continue;
        --handIndex;

        game.PointerDown(Center(TableLayout::PlayerHand[handIndex]));
        const Point2 destination = Center(game.FieldRect(*target));
        game.PointerMove(destination);
        game.PointerUp(destination);
        game.Update(0.16f);
        game.Update(0.501f);
        if (game.Rules().GetPhase() != Phase::REVEAL_DRAW) continue;

        game.Update(0.0f); // reveal pending stock card and start its flip
        const int stockCard = *game.Rules().PendingCard();
        const bool stockMatches = std::any_of(game.Rules().Field().begin(), game.Rules().Field().end(),
            [stockCard](int fieldCard) {
                return DeckRegister::At(fieldCard).month == DeckRegister::At(stockCard).month;
            });
        if (stockMatches) continue;

        game.Update(0.51f); // finish flip
        game.Update(0.0f);  // start stock card flight
        game.Update(0.61f); // finish slow flight
        game.Update(0.16f); // finish snappy table drop into a free slot
        const Rect stockRect = game.FieldRect(stockCard);
        const auto captured = game.Rules().Captured(Player::PLAYER);
        const auto cards = game.Cards();
        for (int capturedId : captured) {
            const auto shown = std::find_if(cards.begin(), cards.end(), [capturedId](const TableCard& card) {
                return card.id == capturedId;
            });
            if (shown == cards.end()) continue;
            const float overlapW = std::max(0.0f, std::min(stockRect.x + stockRect.width,
                                                          shown->rect.x + shown->rect.width) -
                                                std::max(stockRect.x, shown->rect.x));
            const float overlapH = std::max(0.0f, std::min(stockRect.y + stockRect.height,
                                                          shown->rect.y + shown->rect.height) -
                                                std::max(stockRect.y, shown->rect.y));
            Check(overlapW * overlapH < stockRect.width * stockRect.height * 0.1f,
                  "Unmatched stock card landed on deferred hand capture");
        }
        return;
    }
    throw std::runtime_error("No unmatched stock fixture after hand capture");
}

void TestDropTolerance() {
    for (unsigned seed = 1; seed < 1000; ++seed) {
        TableGame game(seed);
        if (game.Rules().GetPhase() != Phase::AWAIT_HAND_PLAY || game.Rules().CurrentPlayer() != Player::PLAYER) continue;
        for (int id : game.Rules().Hand(Player::PLAYER)) for (int f : game.Rules().Field()) {
            const auto r = game.FieldRect(f);
            if (id/4 != f/4 || r.y > TableLayout::Field[0].y + 1.0f) continue;
            const Rect atTen{r.x + r.width/2, r.y - 11, 1, 1};
            const Rect atEleven{r.x + r.width/2, r.y - 12, 1, 1};
            Check(game.FindDropTarget(id, atTen).has_value(), "10 pixel edge gap should match");
            Check(!game.FindDropTarget(id, atEleven), "Gap beyond 10 pixels should miss");
            return;
        }
    }
    throw std::runtime_error("No drop tolerance fixture");
}
void TestHandSlides() {
    bool cpuCovered = false;
    for (std::size_t choice : {std::size_t{0}, std::size_t{3}, std::size_t{7}}) {
        bool playerCovered = false;
        for (unsigned seed = 1; seed < 1000 && (!playerCovered || !cpuCovered); ++seed) {
            TableGame game(seed);
            if (game.Rules().GetPhase() != Phase::AWAIT_HAND_PLAY) continue;
            const Player player = game.Rules().CurrentPlayer();
            if (player == Player::PLAYER && playerCovered) continue;
            if (player == Player::OPPONENT && cpuCovered) continue;
            const auto before = game.Rules().Hand(player);
            const auto& layout = player == Player::PLAYER ? TableLayout::PlayerHand : TableLayout::OpponentHand;
            if (player == Player::PLAYER) {
                const int id = before[choice];
                std::optional<int> target;
                for (int f : game.Rules().Field()) if (f / 4 == id / 4) { target = f; break; }
                game.PointerDown(Center(layout[choice]));
                game.PointerMove(target ? Center(game.FieldRect(*target)) : Point2{400, 500});
                game.PointerUp(target ? Center(game.FieldRect(*target)) : Point2{400, 500});
                Check(game.Busy(), "Slide fixture must play a card");
            }
            for (int step = 0; step < 400 && game.Rules().Hand(player).size() == 8; ++step) game.Update(0.01f);
            const auto after = game.Rules().Hand(player);
            Check(after.size() == 7, "Slide fixture did not commit hand play");
            auto checkPositions = [&](float progress) {
                std::size_t index = 0;
                for (const auto& card : game.Cards()) {
                    if (card.rect.y != layout[0].y) continue;
                    Check(index < after.size(), "Unexpected card in sliding hand");
                    const auto old = std::find(before.begin(), before.end(), after[index]);
                    const auto oldIndex = static_cast<std::size_t>(old - before.begin());
                    const float expected = layout[oldIndex].x + (layout[index].x - layout[oldIndex].x) * progress;
                    Check(std::abs(card.rect.x - expected) < 0.02f, "Hand card jumped or slid to the wrong slot");
                    Check(card.rect.width == layout[index].width && card.rect.height == layout[index].height,
                          "Hand slide changed card size");
                    Check(card.id == (player == Player::PLAYER ? after[index] : -1), "Hand slide changed face visibility");
                    ++index;
                }
                Check(index == after.size(), "Sliding hand lost a card");
            };
            checkPositions(0.0f);
            game.Update(0.15f);
            checkPositions(0.5f);
            game.Update(0.15f);
            checkPositions(1.0f);
            Check(game.Rules().ValidateConservation(), "Hand slide changed card conservation");
            if (player == Player::PLAYER) playerCovered = true;
            else cpuCovered = true;
        }
        Check(playerCovered, "Missing player hand slide fixture");
    }
    Check(cpuCovered, "Missing CPU hand slide fixture");
}
void TestMatches() {
    int totalTurns = 0, decisions = 0, stockChoices = 0;
    for (unsigned seed = 1; seed <= 500; ++seed) {
        TableGame game(seed);
        std::array<int, 2> expectedScores{};
        int steps = 0;
        while (!game.MatchFinished() && steps++ < 6000) {
            Check(game.Rules().ValidateConservation(), "Conservation failed in table game");
            if (game.Busy()) { game.Update(0.1f); continue; }
            auto phase = game.Rules().GetPhase();
            if (phase == Phase::ROUND_END) {
                for (std::size_t i = 0; i < expectedScores.size(); ++i) expectedScores[i] += game.Rules().Result()->points[i];
                Check(game.Scores() == expectedScores, "Incorrect running match total");
                game.NextRound(); continue;
            }
            if (game.Rules().CurrentPlayer() == Player::PLAYER && phase == Phase::AWAIT_HAND_PLAY) {
                PlayFirst(game); ++totalTurns; continue;
            }
            if (game.Rules().CurrentPlayer() == Player::PLAYER && phase == Phase::AWAIT_DRAW_CHOICE) {
                // Stock flip may still need to start/finish before choices are enabled.
                auto before = game.Rules().GetPhase();
                game.PointerDown(Center(game.FieldRect(game.Rules().Choices().front())));
                if (game.Rules().GetPhase() != before) ++stockChoices;
                else game.Update(0.1f);
                continue;
            }
            if (game.Rules().CurrentPlayer() == Player::PLAYER && phase == Phase::AWAIT_DECISION) {
                game.Choose((decisions++ % 2) ? Decision::KOI_KOI : Decision::SHOUBU); continue;
            }
            game.Update(0.1f);
        }
        Check(game.MatchFinished() && game.Round() == 3 && game.Rules().GetPhase() == Phase::ROUND_END &&
              game.Rules().ValidateConservation(), "Three-round match did not finish");
        for (std::size_t i = 0; i < expectedScores.size(); ++i) expectedScores[i] += game.Rules().Result()->points[i];
        Check(game.Scores() == expectedScores, "Incorrect final match total");
        const auto scores = game.Scores(); game.Update(10); game.NextRound();
        Check(game.Scores() == scores && game.Round() == 3, "Match result counted twice");
    }
    Check(totalTurns > 100 && decisions > 0 && stockChoices > 0, "Match simulation did not cover inputs");
}
} // namespace
int main() {
    try { TestOpeningPresentation(); TestRoundIntroBulletTime(); TestProjection(); TestReturnAndFreeze(); TestFlips(); TestStockAvoidsDeferredCapture(); TestDropTolerance(); TestHandSlides(); TestMatches(); std::cout << "Table gameplay passed.\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
