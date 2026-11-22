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
        game.Update(0.4f);
        Check(!game.Busy() && game.Rules().Hand(Player::PLAYER).size() == 8, "Return animation changed state");
        PlayFirst(game);
        game.Update(0.07f); // snap ends, 50 ms hit freeze begins
        Check(game.Busy() && game.Rules().Hand(Player::PLAYER).size() == 8, "Capture committed before hit freeze");
        Check(game.Cards().back().elevation > TableLayout::CardElevation + TableLayout::CardThickness * 0.9f,
              "Snapped card must rest on top of matched field card during hit freeze");
        game.Update(0.049f);
        Check(game.Rules().Hand(Player::PLAYER).size() == 8, "Hit freeze shorter than 50 ms");
        game.Update(0.002f);
        Check(game.Rules().Hand(Player::PLAYER).size() == 7 && game.Rules().ValidateConservation(), "Capture did not commit after freeze");
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
            float initialStockThickness = 0.0f;
            for (const auto& c : game.Cards()) if (c.id == -1 && c.rect.x == TableLayout::Stock.x) initialStockThickness = c.thickness;
            Check(initialStockThickness > TableLayout::CardThickness * 2.0f, "Stock pile must have 3D stack thickness");
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
            float nextStockThickness = 0.0f;
            for (const auto& c : game.Cards()) if (c.id == -1 && c.rect.x == TableLayout::Stock.x) nextStockThickness = c.thickness;
            Check(nextStockThickness < initialStockThickness, "Stock stack thickness must shrink as cards are drawn");
            stockChecked = true;
        }
    }
    Check(cpuChecked && stockChecked, "Could not exercise both flip types");
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
    try { TestProjection(); TestReturnAndFreeze(); TestFlips(); TestDropTolerance(); TestMatches(); std::cout << "Table gameplay passed.\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
