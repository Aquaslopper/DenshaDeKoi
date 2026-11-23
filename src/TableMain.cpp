#include "TableGame.hpp"
#include "TableHud.hpp"
#include "TableScene.hpp"
#include "SimpleRadio.hpp"
#include "SfxPlayer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

namespace {
TableLayout::Point2 CanvasPoint(Vector2 mouse) {
    const float scale = std::min(static_cast<float>(GetScreenWidth()) / TableLayout::ImageWidth,
                                 static_cast<float>(GetScreenHeight()) / TableLayout::ImageHeight);
    return {(mouse.x - (GetScreenWidth() - TableLayout::ImageWidth * scale) / 2.0f) / scale,
            (mouse.y - (GetScreenHeight() - TableLayout::ImageHeight * scale) / 2.0f) / scale};
}
bool In(TableLayout::Point2 p, TableLayout::Rect r) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}
TableLayout::Point2 PlanPoint(TableLayout::Point2 canvas) {
    auto p = TableLayout::Unproject(canvas);
    p.x = std::clamp(p.x, -200.0f, TableLayout::PlanWidth + 200.0f);
    p.y = std::clamp(p.y, -200.0f, TableLayout::PlanHeight + 200.0f);
    return p;
}
void PlayGameSfx(SfxPlayer& player, GameSfx cue) {
    switch (cue) {
        case GameSfx::CARD_DEAL: player.Play(SfxId::CardDeal); break;
        case GameSfx::CARD_FLIP: player.Play(SfxId::CardFlip); break;
        case GameSfx::CARD_PLACE: player.Play(SfxId::CardPlace); break;
        case GameSfx::CARD_MATCH: player.Play(SfxId::CardMatch); break;
        case GameSfx::CARD_CAPTURE: player.Play(SfxId::CardCapture); break;
        case GameSfx::KOI_KOI: player.Play(SfxId::KoiKoi); break;
        case GameSfx::SHOUBU: player.Play(SfxId::Shobu); break;
        case GameSfx::ROUND_WIN: player.Play(SfxId::RoundWin); break;
        case GameSfx::ROUND_LOSE: player.Play(SfxId::RoundLose); break;
    }
}
} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(TableLayout::ImageWidth, TableLayout::ImageHeight, "Densha de Koi");
    if (!IsWindowReady()) {
        std::fprintf(stderr, "Could not create the window.\n");
        return 1;
    }
    SetExitKey(KEY_NULL);
    SetWindowMinSize(680, 384);
    SetTargetFPS(60);
    InitAudioDevice(); // the radio streams into a Raylib AudioStream
    if (!IsAudioDeviceReady()) std::fprintf(stderr, "[radio] no audio device; radio will stay silent\n");

    int result = 0;
    try {
        const std::filesystem::path appDir(GetApplicationDirectory());
        const std::string background = (appDir / "the-city.jpg").string();
        const std::string atlas = (appDir / "cards_atlas.png").string();
        const std::string logo = (appDir / "koidenscha.png").string();
        const std::string headingFont = (appDir / "fonts" / "Oxanium-Bold.ttf").string();
        const std::string bodyFont = (appDir / "fonts" / "Rajdhani-Bold.ttf").string();
        const TableScene scene(background.c_str(), atlas.c_str(), logo.c_str(),
                               headingFont.c_str(), bodyFont.c_str());
        TableGame game;
        SimpleRadio radio;
        SfxPlayer sfx(appDir / "sfx");
        TableOverlay overlay = TableOverlay::NONE;
        float masterVolume = 1.0f;
        float sfxVolume = 1.0f;
        bool draggingVolume = false;
        bool draggingMaster = false;
        bool draggingSfx = false;
        bool inspectingCard = false;
        MagnifyEffect magnify{};

        while (!WindowShouldClose()) {
            const float dt = std::min(GetFrameTime(), 0.1f);

            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
                overlay = overlay == TableOverlay::PAUSE ? TableOverlay::NONE : TableOverlay::PAUSE;
                draggingVolume = draggingMaster = draggingSfx = false;
                if (overlay != TableOverlay::NONE) {
                    game.CancelDrag();
                    inspectingCard = false;
                }
            }
            if (IsKeyPressed(KEY_M)) {
                radio.ToggleMute();
            }

            const auto canvas = CanvasPoint(GetMousePosition());
            if (overlay != TableOverlay::NONE) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (In(canvas, TableHud::RadioMenuButton)) {
                        overlay = overlay == TableOverlay::RADIO ? TableOverlay::NONE : TableOverlay::RADIO;
                    } else if (In(canvas, TableHud::PauseButton)) {
                        overlay = overlay == TableOverlay::PAUSE ? TableOverlay::NONE : TableOverlay::PAUSE;
                    } else if (overlay == TableOverlay::RADIO) {
                        if (In(canvas, TableHud::ResumeBottomButton)) overlay = TableOverlay::NONE;
                        else if (In(canvas, TableHud::RadioPowerButton)) radio.ToggleEnabled();
                        else if (In(canvas, TableHud::RadioMuteButton)) radio.ToggleMute();
                        else if (In(canvas, TableHud::VolumeHitBox)) draggingVolume = true;
                        else if (In(canvas, TableHud::NewMatchButton)) {
                            game = TableGame();
                            overlay = TableOverlay::NONE;
                        } else {
                            for (std::size_t i = 0; i < TableHud::StationSlotCount; ++i)
                                if (In(canvas, TableHud::StationButton(i))) { radio.SelectStation(static_cast<int>(i)); break; }
                        }
                    } else {
                        if (In(canvas, TableHud::OptionsResumeButton)) overlay = TableOverlay::NONE;
                        else if (In(canvas, TableHud::OptionsNewMatchButton)) {
                            game = TableGame();
                            overlay = TableOverlay::NONE;
                        } else if (In(canvas, TableHud::MasterVolumeHitBox)) draggingMaster = true;
                        else if (In(canvas, TableHud::SfxVolumeHitBox)) draggingSfx = true;
                    }
                }
                if (draggingVolume && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
                    radio.SetVolume((canvas.x - TableHud::VolumeBar.x) / TableHud::VolumeBar.width);
                if (draggingMaster && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    masterVolume = std::clamp((canvas.x - TableHud::MasterVolumeBar.x) / TableHud::MasterVolumeBar.width, 0.0f, 1.0f);
                    SetMasterVolume(masterVolume);
                }
                if (draggingSfx && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    sfxVolume = std::clamp((canvas.x - TableHud::SfxVolumeBar.x) / TableHud::SfxVolumeBar.width, 0.0f, 1.0f);
                    sfx.SetVolume(sfxVolume);
                }
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                    draggingVolume = draggingMaster = draggingSfx = false;
            } else {
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    if (game.Dragging()) game.CancelDrag();
                    else inspectingCard = game.HasCardAt(PlanPoint(canvas));
                }
                if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) inspectingCard = false;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (game.AwaitingDealerChoice() && In(canvas, TableHud::DealerCardLeft)) {
                        game.SelectDealerCard(0);
                    } else if (game.AwaitingDealerChoice() && In(canvas, TableHud::DealerCardRight)) {
                        game.SelectDealerCard(1);
                    } else if (In(canvas, TableHud::RadioMenuButton)) {
                        overlay = TableOverlay::RADIO;
                        game.CancelDrag();
                    } else if (In(canvas, TableHud::PauseButton)) {
                        overlay = TableOverlay::PAUSE;
                        game.CancelDrag();
                    } else if (game.Rules().GetPhase() == Phase::ROUND_END && In(canvas, TableHud::RoundEndButton)) {
                        if (game.MatchFinished()) game = TableGame();
                        else game.NextRound();
                    } else if (game.Rules().GetPhase() == Phase::AWAIT_DECISION && !game.Busy() &&
                               game.Rules().CurrentPlayer() == Player::PLAYER && In(canvas, TableHud::KoiKoiButton)) {
                        game.Choose(Decision::KOI_KOI);
                    } else if (game.Rules().GetPhase() == Phase::AWAIT_DECISION && !game.Busy() &&
                               game.Rules().CurrentPlayer() == Player::PLAYER && In(canvas, TableHud::ShobuButton)) {
                        game.Choose(Decision::SHOUBU);
                    } else {
                        game.PointerDown(PlanPoint(canvas));
                    }
                }
                if (game.Dragging()) game.PointerMove(PlanPoint(canvas));
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) game.PointerUp(PlanPoint(canvas));
                game.Update(dt);
            }

            const float desiredMagnification =
                overlay == TableOverlay::NONE && inspectingCard ? 0.85f : 0.0f;
            const float magnifyBlend = 1.0f - std::exp(-12.0f * dt);
            magnify.amount += (desiredMagnification - magnify.amount) * magnifyBlend;
            magnify.focus.x += (canvas.x - magnify.focus.x) * magnifyBlend;
            magnify.focus.y += (canvas.y - magnify.focus.y) * magnifyBlend;

            for (GameSfx cue : game.TakeSfx()) PlayGameSfx(sfx, cue);
            radio.Update();
            scene.Draw(game, {overlay, radio.Status(), masterVolume, sfxVolume}, magnify);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        result = 1;
    }
    CloseAudioDevice();
    CloseWindow();
    return result;
}
