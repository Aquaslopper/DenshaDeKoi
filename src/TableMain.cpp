#include "RadioPlayer.hpp"
#include "TableGame.hpp"
#include "TableHud.hpp"
#include "TableScene.hpp"

#include <algorithm>
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

    int result = 0;
    try {
        const std::filesystem::path appDir(GetApplicationDirectory());
        const std::string background = (appDir / "the-city.jpg").string();
        const std::string atlas = (appDir / "cards_atlas.png").string();
        const TableScene scene(background.c_str(), atlas.c_str());
        TableGame game;
        RadioPlayer radio;
        bool paused = false;
        bool draggingVolume = false;
        bool draggingPrebuffer = false;
        std::string hudFeedback;
        float feedbackTimer = 0.0f;

        while (!WindowShouldClose()) {
            const float dt = std::min(GetFrameTime(), 0.1f);
            if (feedbackTimer > 0.0f) {
                feedbackTimer -= dt;
                if (feedbackTimer <= 0.0f) {
                    feedbackTimer = 0.0f;
                    hudFeedback.clear();
                }
            }

            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
                paused = !paused;
                draggingVolume = false;
                draggingPrebuffer = false;
                if (paused) game.CancelDrag();
            }
            if (IsKeyPressed(KEY_M)) {
                radio.ToggleMute();
            }

            const auto canvas = CanvasPoint(GetMousePosition());
            if (paused) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (In(canvas, TableHud::PauseButton) ||
                        In(canvas, TableHud::ResumeTopButton) ||
                        In(canvas, TableHud::ResumeBottomButton)) {
                        paused = false;
                        draggingVolume = false;
                        draggingPrebuffer = false;
                    } else if (In(canvas, TableHud::RadioPowerButton)) {
                        radio.ToggleEnabled();
                    } else if (In(canvas, TableHud::RadioMuteButton)) {
                        radio.ToggleMute();
                    } else if (In(canvas, TableHud::RadioBufferButton)) {
                        radio.CycleBufferMode();
                    } else if (In(canvas, TableHud::VolumeHitBox)) {
                        draggingVolume = true;
                        radio.SetVolume((canvas.x - TableHud::VolumeBar.x) / TableHud::VolumeBar.width);
                    } else if (In(canvas, TableHud::PrebufferHitBox)) {
                        draggingPrebuffer = true;
                        radio.SetPrebufferRatio(
                            (canvas.x - TableHud::PrebufferBar.x) / TableHud::PrebufferBar.width);
                    } else if (In(canvas, TableHud::PasteUrlButton)) {
                        const char* clip = GetClipboardText();
                        if (clip && ParseUrl(clip).valid) {
                            radio.PlayCustomUrl(clip);
                            hudFeedback.clear();
                        } else {
                            hudFeedback = "Copy a valid http(s) .pls or MP3 stream URL first";
                            feedbackTimer = 3.5f;
                        }
                    } else if (In(canvas, TableHud::NewMatchButton)) {
                        game = TableGame();
                        paused = false;
                        draggingVolume = false;
                        draggingPrebuffer = false;
                    } else {
                        for (std::size_t i = 0; i < TableHud::StationSlotCount; ++i) {
                            if (In(canvas, TableHud::StationButton(i))) {
                                radio.SelectStation(static_cast<int>(i));
                                hudFeedback.clear();
                                break;
                            }
                        }
                    }
                }
                if (draggingVolume && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    radio.SetVolume((canvas.x - TableHud::VolumeBar.x) / TableHud::VolumeBar.width);
                }
                if (draggingPrebuffer && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    radio.SetPrebufferRatio(
                        (canvas.x - TableHud::PrebufferBar.x) / TableHud::PrebufferBar.width);
                }
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    if (draggingPrebuffer) {
                        radio.ApplyPrebufferThreshold();
                    }
                    draggingVolume = false;
                    draggingPrebuffer = false;
                }
            } else {
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) game.CancelDrag();
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (In(canvas, TableHud::PauseButton)) {
                        paused = true;
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

            radio.Update();
            scene.Draw(game, {paused, radio.Snapshot(), hudFeedback});
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        result = 1;
    }
    CloseWindow();
    return result;
}
