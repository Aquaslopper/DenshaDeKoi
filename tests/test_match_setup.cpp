#include "TableGame.hpp"
#include "DeckRegister.hpp"
#include "ui/MatchSetupMenu.hpp"

#include <algorithm>
#include <string>

namespace {
using TableLayout::Point2;
using TableLayout::Rect;

void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

Point2 Center(Rect rect) { return {rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f}; }

Ui::MatchSetupInput Mouse(Point2 point, bool pressed = false) {
    return {point, true, pressed, 0, 0, false, false};
}
Ui::MatchSetupInput Keys(int vertical = 0, int horizontal = 0, bool enter = false, bool back = false) {
    return {{-1, -1}, false, false, vertical, horizontal, enter, back};
}

void TestDefaultsAndKeyboard() {
    Ui::MatchSetupMenu menu("assets");
    menu.Open({});
    Check(menu.IsOpen(), "The setup popup must open on PLAY");
    Check(menu.Setup().rounds == 3 && menu.Setup().background == 0,
          "Defaults must be three rounds on the first backdrop");

    // Rounds row is focused first: arrows step through 3 / 4 / 5 and wrap.
    menu.Update(Keys(0, 1));
    Check(menu.Setup().rounds == 4, "Right arrow must step the round count up");
    menu.Update(Keys(0, 1));
    Check(menu.Setup().rounds == 5, "Right arrow must reach five rounds");
    menu.Update(Keys(0, 1));
    Check(menu.Setup().rounds == 3, "Round choices must wrap around");
    menu.Update(Keys(0, -1));
    Check(menu.Setup().rounds == 5, "Left arrow must wrap the other way");
    menu.Update(Keys(0, 1));
    menu.Update(Keys(0, 1));

    // Down to the backdrop row: arrows cycle the five placeholders.
    menu.Update(Keys(1, 0));
    menu.Update(Keys(0, 1));
    Check(menu.Setup().background == 1, "Right arrow must select the next backdrop");
    menu.Update(Keys(0, -1));
    menu.Update(Keys(0, -1));
    Check(menu.Setup().background == Ui::MatchBackgrounds.size() - 1,
          "Backdrop selection must wrap in both directions");

    // Enter only commits on the start row, which sits below the backdrop row.
    Check(menu.Update(Keys(1, 0)) == Ui::MatchSetupResult::NONE, "Moving down must not commit");
    Check(menu.Update(Keys(0, 0, true)) == Ui::MatchSetupResult::START,
          "Enter on START MATCH must start the match");

    // Esc always backs out without starting.
    menu.Open(menu.Setup());
    Check(menu.Update(Keys(0, 0, false, true)) == Ui::MatchSetupResult::BACK,
          "Esc must close the popup");
    menu.Close();
    Check(!menu.IsOpen(), "Close must hide the popup");
}

void TestPointer() {
    Ui::MatchSetupMenu menu("assets");
    menu.Open({});

    menu.Update(Mouse(Center(Ui::MatchSetupMenu::RoundKey(2)), true));
    Check(menu.Setup().rounds == Ui::MatchRoundChoices[2], "Clicking a round key must select it");

    menu.Update(Mouse(Center(Ui::MatchSetupMenu::RoundKey(0)), true));
    Check(menu.Setup().rounds == Ui::MatchRoundChoices[0], "Clicking must switch back");

    menu.Update(Mouse(Center(Ui::MatchSetupMenu::BackgroundArrow(1)), true));
    Check(menu.Setup().background == 1, "The right arrow must step the backdrop");
    menu.Update(Mouse(Center(Ui::MatchSetupMenu::BackgroundValue()), true));
    Check(menu.Setup().background == 2, "The value cell must also step the backdrop");
    menu.Update(Mouse(Center(Ui::MatchSetupMenu::BackgroundArrow(-1)), true));
    Check(menu.Setup().background == 1, "The left arrow must step back");

    // The numbered drop-in slots select a backdrop directly.
    menu.Update(Mouse(Center(Ui::MatchSetupMenu::BackgroundSlot(4)), true));
    Check(menu.Setup().background == 4, "Clicking a numbered slot must select that backdrop");

    // Clicks elsewhere never start a match, and the preview does not overlap the keys.
    Check(menu.Update(Mouse(Center(Ui::MatchSetupMenu::PreviewBox), true)) == Ui::MatchSetupResult::NONE,
          "The preview is inert");
    const auto overlaps = [](Rect a, Rect b) {
        return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
    };
    Check(!overlaps(Ui::MatchSetupMenu::PreviewBox, Ui::MatchSetupMenu::RoundKey(0)) &&
          !overlaps(Ui::MatchSetupMenu::PreviewBox, Ui::MatchSetupMenu::BackgroundArrow(1)) &&
          !overlaps(Ui::MatchSetupMenu::StartButton, Ui::MatchSetupMenu::BackButton) &&
          !overlaps(Ui::MatchSetupMenu::InfoBox, Ui::MatchSetupMenu::BackgroundArrow(1)) &&
          !overlaps(Ui::MatchSetupMenu::BackgroundSlot(4), Ui::MatchSetupMenu::InfoBox) &&
          !overlaps(Ui::MatchSetupMenu::BackgroundSlot(4), Ui::MatchSetupMenu::StartButton),
          "Popup hit boxes must not overlap");

    Check(menu.Update(Mouse(Center(Ui::MatchSetupMenu::StartButton), true)) == Ui::MatchSetupResult::START,
          "Clicking START MATCH must start the match");
    menu.Open({});
    Check(menu.Update(Mouse(Center(Ui::MatchSetupMenu::BackButton), true)) == Ui::MatchSetupResult::BACK,
          "Clicking BACK must return to the title");
}

void TestSetupIsClamped() {
    Ui::MatchSetupMenu menu("assets");
    menu.Open({9, 42}); // Out-of-range values from a future settings file.
    Check(menu.Setup().rounds == Ui::MatchRoundChoices.front(), "Unknown round counts fall back to the default");
    Check(menu.Setup().background >= 0 &&
          menu.Setup().background < static_cast<int>(Ui::MatchBackgrounds.size()),
          "Backdrop index must wrap into range");
}

void TestMatchRounds() {
    // The contract default stays three rounds.
    TableGame defaultMatch;
    Check(defaultMatch.MatchRounds() == 3 && defaultMatch.Round() == 1, "Default match must keep three rounds");
    TableGame seeded(7u);
    Check(seeded.MatchRounds() == 3, "Seeded matches must keep the contract round count");

    for (int rounds : Ui::MatchRoundChoices) {
        TableGame game = TableGame::WithRounds(rounds);
        Check(game.MatchRounds() == rounds, "Chosen round count must reach the match");
        Check(game.AwaitingDealerChoice(), "A new match must still start with the dealer pick");
        game.SelectDealerCard(0);
        game.Update(3.5f); // Dealer reveal plus round intro.
        int steps = 0;
        while (!game.MatchFinished() && steps++ < 6000) {
            if (game.Busy()) { game.Update(0.1f); continue; }
            const Phase phase = game.Rules().GetPhase();
            if (phase == Phase::ROUND_END) { game.NextRound(); continue; }
            if (phase == Phase::AWAIT_HAND_PLAY && game.Rules().CurrentPlayer() == Player::PLAYER) {
                const auto& hand = game.Rules().Hand(Player::PLAYER);
                Check(!hand.empty(), "Player hand must not be empty");
                const int id = hand.front();
                std::optional<int> target;
                for (int field : game.Rules().Field())
                    if (DeckRegister::At(field).month == DeckRegister::At(id).month) { target = field; break; }
                game.PointerDown(Center(TableLayout::PlayerHand[0]));
                const Point2 destination = target ? Center(game.FieldRect(*target)) : Point2{400, 500};
                game.PointerMove(destination);
                game.PointerUp(destination);
                continue;
            }
            if (phase == Phase::AWAIT_DRAW_CHOICE && game.Rules().CurrentPlayer() == Player::PLAYER) {
                game.PointerDown(Center(game.FieldRect(game.Rules().Choices().front())));
                continue;
            }
            if (phase == Phase::AWAIT_DECISION && game.Rules().CurrentPlayer() == Player::PLAYER) {
                game.Choose(Decision::SHOUBU);
                continue;
            }
            game.Update(0.1f);
        }
        Check(game.MatchFinished() && game.Round() == rounds,
              "The match must finish exactly after the chosen number of rounds");
    }
}
} // namespace

void TestMatchSetup() {
    TestDefaultsAndKeyboard();
    TestPointer();
    TestSetupIsClamped();
    TestMatchRounds();
}
