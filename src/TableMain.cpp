#include "TableGame.hpp"
#include "TableHud.hpp"
#include "TableScene.hpp"
#include "SimpleRadio.hpp"
#include "SfxPlayer.hpp"
#include "ui/PauseMenu.hpp"
#include "ui/MatchSettingsMenu.hpp"
#include "ui/StartMenu.hpp"

#include <algorithm>
#include <array>
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
Ui::Viewport ScreenViewport() {
    const float scale = std::min(static_cast<float>(GetScreenWidth()) / TableLayout::ImageWidth,
                                 static_cast<float>(GetScreenHeight()) / TableLayout::ImageHeight);
    const float width = TableLayout::ImageWidth * scale;
    const float height = TableLayout::ImageHeight * scale;
    return {{(GetScreenWidth() - width) / 2.0f, (GetScreenHeight() - height) / 2.0f, width, height}, scale};
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
Resolution MonitorSize() {
    const int monitor = GetCurrentMonitor();
    const Resolution size{GetMonitorWidth(monitor), GetMonitorHeight(monitor)};
    return size.width > 0 && size.height > 0 ? size : Resolution{GetScreenWidth(), GetScreenHeight()};
}

void ApplyDisplay(GameOptions& options, Resolution desktop) {
    const ScreenMode requested = options.screenMode;
    // Leave the old mode first: raylib's borderless window owns its monitor
    // size, while exclusive fullscreen chooses a video mode from the window size.
    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
    if (IsWindowFullscreen() && requested != ScreenMode::Fullscreen) ToggleFullscreen();
    if (requested == ScreenMode::Borderless) ToggleBorderlessWindowed();
    else {
        auto& r = options.resolution;
        if (r.width > desktop.width || r.height > desktop.height) r = desktop;
        if (IsWindowMaximized()) RestoreWindow();
        SetWindowSize(r.width, r.height);
        if (requested == ScreenMode::Fullscreen && !IsWindowFullscreen()) ToggleFullscreen();
        if (requested == ScreenMode::Windowed) {
            const auto origin = GetMonitorPosition(GetCurrentMonitor());
            const auto pos = GetWindowPosition();
            SetWindowPosition(static_cast<int>(std::clamp(pos.x, origin.x, origin.x + desktop.width - r.width)),
                              static_cast<int>(std::clamp(pos.y, origin.y, origin.y + desktop.height - r.height)));
        }
    }
    options.screenMode = IsWindowFullscreen() ? ScreenMode::Fullscreen :
                         IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) ? ScreenMode::Borderless : ScreenMode::Windowed;
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
        TableScene scene(background.c_str(), atlas.c_str(), logo.c_str(),
                         headingFont.c_str(), bodyFont.c_str(), appDir / "options");
        TableGame game;
        SimpleRadio radio;
        SfxPlayer sfx(appDir / "sfx");
        TableOverlay overlay = TableOverlay::NONE;
        const auto optionsFile = OptionsFile();
        GameOptions options = LoadOptions(optionsFile);
        Resolution desktop = MonitorSize();
        ApplyDisplay(options, desktop);
        options.aaSamples = scene.SetAntiAliasing(options.aaSamples);
        if (IsAudioDeviceReady()) SetMasterVolume(options.masterVolume / 100.0f);
        sfx.SetVolume(options.sfxVolume / 100.0f);
        Ui::PauseMenu pause;
        Ui::StartMenu startMenu(appDir, optionsFile.parent_path() / "start-menu-variant.txt");
        Ui::MatchSettingsMenu matchSettingsMenu(appDir);
        bool inStartMenu = true;
        bool matchSettingsOpen = false;
        bool galleryOpen = false;
        bool draggingVolume = false; // Radio's own channel slider.
        bool inspectingCard = false;
        MagnifyEffect magnify{};
        const auto setOverlay = [&](TableOverlay next) {
            overlay = next;
            draggingVolume = false;
            inspectingCard = false;
            pause = {};
            if (next != TableOverlay::NONE) game.CancelDrag();
        };
        bool quitRequested = false;
        int targetRounds = options.matchRounds;
        int completedMatchRounds = 0;
        std::array<int, 2> cumulativeScores{};
        bool matchOver = false;
        const auto resetMatchProgress = [&]() {
            completedMatchRounds = 0;
            cumulativeScores = {};
            matchOver = false;
        };
        const auto totalMatchScores = [&]() {
            std::array<int, 2> total = cumulativeScores;
            const auto& s = game.Scores();
            total[0] += s[0];
            total[1] += s[1];
            return total;
        };

        while (!WindowShouldClose() && !quitRequested) {
            const float dt = std::min(GetFrameTime(), 0.1f);
            const Resolution display{GetScreenWidth(), GetScreenHeight()};
            if (display.width <= 0 || display.height <= 0) {
                radio.Update();
                BeginDrawing();
                EndDrawing(); // Keep polling events while minimized.
                continue;
            }
            if (overlay == TableOverlay::PAUSE && options.screenMode != ScreenMode::Fullscreen) desktop = MonitorSize();
            if (options.screenMode == ScreenMode::Windowed) options.resolution = display;
            if (!inStartMenu && IsKeyPressed(KEY_BACKSPACE)) {
                resetMatchProgress();
                inStartMenu = true;
                galleryOpen = false;
                startMenu.AdvanceVariant();
                game = TableGame();
                setOverlay(TableOverlay::NONE);
            }
            const bool pauseKey = !inStartMenu && (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P));
            if (pauseKey) setOverlay(overlay == TableOverlay::PAUSE ? TableOverlay::NONE : TableOverlay::PAUSE);
            if (IsKeyPressed(KEY_M)) radio.ToggleMute();

            const auto canvas = CanvasPoint(GetMousePosition());
            const bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            if (inStartMenu) {
                const bool escape = IsKeyPressed(KEY_ESCAPE);
                // Esc belongs to the 3D viewer while it is open; a second Esc
                // then leaves the gallery as before.
                if (escape && (overlay != TableOverlay::NONE ||
                               (galleryOpen && !startMenu.GalleryViewerOpen()))) {
                    setOverlay(TableOverlay::NONE);
                    galleryOpen = false;
                }
                if (galleryOpen) {
                    const auto movement = GetMouseDelta();
                    const int vertical =
                        static_cast<int>(IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) -
                        static_cast<int>(IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP));
                    const int horizontal =
                        static_cast<int>(IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) -
                        static_cast<int>(IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT));
                    const Ui::GalleryInput galleryInput{
                        canvas, movement.x != 0.0f || movement.y != 0.0f, pressed,
                        IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                        IsMouseButtonPressed(MOUSE_BUTTON_RIGHT), GetMouseWheelMove(),
                        vertical, horizontal, IsKeyPressed(KEY_ENTER), escape,
                        options.aaSamples};
                    if (startMenu.UpdateGallery(galleryInput)) galleryOpen = false;
                } else if (overlay == TableOverlay::PAUSE) {
                    const auto old = options;
                    const auto movement = GetMouseDelta();
                    const Ui::PauseInput input{canvas, movement.x != 0 || movement.y != 0, pressed,
                        IsMouseButtonDown(MOUSE_BUTTON_LEFT), IsKeyPressed(KEY_DOWN) - IsKeyPressed(KEY_UP),
                        IsKeyPressed(KEY_RIGHT) - IsKeyPressed(KEY_LEFT),
                        IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)};
                    const auto pauseAction = startMenu.OptionsMenu().Update(options, input, display, desktop);
                    if (old.masterVolume != options.masterVolume && IsAudioDeviceReady())
                        SetMasterVolume(options.masterVolume / 100.0f);
                    if (old.sfxVolume != options.sfxVolume) sfx.SetVolume(options.sfxVolume / 100.0f);
                    if (old.screenMode != options.screenMode || old.resolution != options.resolution)
                        ApplyDisplay(options, desktop);
                    if (old.aaSamples != options.aaSamples) options.aaSamples = scene.SetAntiAliasing(options.aaSamples);
                    if (pauseAction != Ui::PauseAction::None) setOverlay(TableOverlay::NONE);
                } else if (overlay == TableOverlay::RADIO) {
                    if (pressed) {
                        if (In(canvas, TableHud::ResumeBottomButton)) setOverlay(TableOverlay::NONE);
                        else if (In(canvas, TableHud::NewMatchButton)) {
                            targetRounds = options.matchRounds;
                            resetMatchProgress();
                            game = TableGame();
                            inStartMenu = false;
                            setOverlay(TableOverlay::NONE);
                        } else if (In(canvas, TableHud::RadioPowerButton)) radio.ToggleEnabled();
                        else if (In(canvas, TableHud::RadioMuteButton)) radio.ToggleMute();
                        else if (In(canvas, TableHud::VolumeHitBox)) draggingVolume = true;
                        else {
                            for (std::size_t i = 0; i < TableHud::StationSlotCount; ++i)
                                if (In(canvas, TableHud::StationButton(i))) { radio.SelectStation(static_cast<int>(i)); break; }
                        }
                    }
                    if (draggingVolume && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
                        radio.SetVolume((canvas.x - TableHud::VolumeBar.x) / TableHud::VolumeBar.width);
                    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) draggingVolume = false;
                } else if (matchSettingsOpen) {
                    const auto movement = GetMouseDelta();
                    const int vertical =
                        static_cast<int>(IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) -
                        static_cast<int>(IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP));
                    const int horizontal =
                        static_cast<int>(IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) -
                        static_cast<int>(IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT));
                    const auto matchAction = matchSettingsMenu.Update(
                        canvas, movement.x != 0.0f || movement.y != 0.0f, pressed,
                        vertical, horizontal, IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE), escape);
                    if (matchAction == Ui::MatchSettingsAction::BACK) {
                        matchSettingsOpen = false;
                    } else if (matchAction == Ui::MatchSettingsAction::START) {
                        options.matchRounds = matchSettingsMenu.SelectedRounds();
                        targetRounds = options.matchRounds;
                        SaveOptions(optionsFile, options);
                        resetMatchProgress();
                        game = TableGame();
                        matchSettingsOpen = false;
                        inStartMenu = false;
                    }
                } else if (overlay == TableOverlay::NONE && !galleryOpen) {
                    const auto movement = GetMouseDelta();
                    const auto action = startMenu.Update(canvas, movement.x != 0 || movement.y != 0, pressed,
                        IsKeyPressed(KEY_DOWN) - IsKeyPressed(KEY_UP),
                        IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE));
                    switch (action) {
                        case Ui::StartMenuAction::PLAY:
                            matchSettingsMenu.ResetFocus();
                            matchSettingsMenu.SetSelectedRounds(options.matchRounds);
                            matchSettingsOpen = true;
                            break;
                        case Ui::StartMenuAction::OPTIONS:
                            setOverlay(TableOverlay::PAUSE);
                            break;
                        case Ui::StartMenuAction::RADIO:
                            setOverlay(TableOverlay::RADIO);
                            break;
                        case Ui::StartMenuAction::GALLERY:
                            galleryOpen = true;
                            break;
                        case Ui::StartMenuAction::EXIT:
                            quitRequested = true;
                            break;
                        case Ui::StartMenuAction::NONE:
                            break;
                    }
                }

                radio.Update();
                BeginDrawing();
                ClearBackground(BLACK);
                const Ui::Viewport viewport = ScreenViewport();
                startMenu.DrawBackground(viewport);
                if (matchSettingsOpen) matchSettingsMenu.Draw(viewport, canvas);
                if (galleryOpen) startMenu.DrawGallery(viewport);
                else if (overlay == TableOverlay::PAUSE)
                    startMenu.DrawOptions(viewport, options, display);
                else if (overlay == TableOverlay::RADIO)
                    startMenu.DrawRadio(viewport, radio.Status());
                EndDrawing();
                continue;
            }
            if (matchOver) {
                const Ui::Viewport viewport = ScreenViewport();
                const auto total = cumulativeScores;
                const int playerScore = total[PlayerIndex(Player::PLAYER)];
                const int cpuScore = total[PlayerIndex(Player::OPPONENT)];
                const char* headline = playerScore == cpuScore ? "MATCH DRAWN"
                                     : (playerScore > cpuScore ? "YOU WIN" : "CPU WINS");
                const Color headlineColor = playerScore == cpuScore ? Color{220, 220, 220, 255}
                                          : (playerScore > cpuScore ? Color{255, 213, 25, 255}
                                                                    : Color{255, 120, 120, 255});
                constexpr TableLayout::Rect newMatchBtn{430.0f, 470.0f, 220.0f, 62.0f};
                constexpr TableLayout::Rect titleBtn{710.0f, 470.0f, 220.0f, 62.0f};
                const bool clickNew = pressed && In(canvas, newMatchBtn);
                const bool clickTitle = pressed && In(canvas, titleBtn);
                if (clickNew || IsKeyPressed(KEY_ENTER)) {
                    targetRounds = options.matchRounds;
                    resetMatchProgress();
                    game = TableGame();
                    setOverlay(TableOverlay::NONE);
                } else if (clickTitle || IsKeyPressed(KEY_ESCAPE)) {
                    SaveOptions(optionsFile, options);
                    resetMatchProgress();
                    inStartMenu = true;
                    galleryOpen = false;
                    startMenu.AdvanceVariant();
                    game = TableGame();
                    setOverlay(TableOverlay::NONE);
                }
                for (GameSfx cue : game.TakeSfx()) PlayGameSfx(sfx, cue);
                radio.Update();
                BeginDrawing();
                ClearBackground(Color{8, 12, 22, 255});
                startMenu.DrawBackground(viewport);
                DrawRectangle(static_cast<int>(viewport.box.x), static_cast<int>(viewport.box.y),
                              static_cast<int>(viewport.box.width), static_cast<int>(viewport.box.height),
                              Color{5, 8, 17, 170});
                const Rectangle panel = viewport.Rec({330, 150, 700, 430});
                DrawRectangleRounded(panel, 0.04f, 8, Color{16, 27, 44, 245});
                DrawRectangleRoundedLinesEx(panel, 0.04f, 8, std::max(1.0f, 2.0f * viewport.scale),
                                            Color{196, 207, 227, 180});
                auto drawCentered = [&](const char* text, float y, float size, Color color) {
                    const int fontSize = std::max(1, static_cast<int>(size * viewport.scale));
                    const int w = MeasureText(text, fontSize);
                    DrawText(text, static_cast<int>(viewport.box.x + viewport.box.width / 2 - w / 2),
                             static_cast<int>(viewport.Y(y)), fontSize, color);
                };
                drawCentered("MATCH OVER", 185, 28, Color{196, 207, 227, 255});
                drawCentered(headline, 235, 56, headlineColor);
                const std::string scoreLine = "YOU  " + std::to_string(playerScore) + "  -  " +
                                              std::to_string(cpuScore) + "  CPU";
                drawCentered(scoreLine.c_str(), 330, 40, WHITE);
                const std::string roundsLine = std::to_string(targetRounds) +
                                               (targetRounds == 1 ? " ROUND MATCH" : " ROUNDS MATCH");
                drawCentered(roundsLine.c_str(), 385, 22, Color{170, 185, 205, 255});
                auto drawBtn = [&](TableLayout::Rect r, const char* label, bool hot, Color accent) {
                    const Rectangle rec = viewport.Rec(r);
                    DrawRectangleRounded(rec, 0.12f, 6, hot ? Color{accent.r, accent.g, accent.b, 40}
                                                            : Color{27, 36, 53, 255});
                    DrawRectangleRoundedLinesEx(rec, 0.12f, 6, std::max(1.0f, 2.0f * viewport.scale),
                                                hot ? accent : Color{196, 207, 227, 200});
                    const int fs = std::max(1, static_cast<int>(28 * viewport.scale));
                    const int tw = MeasureText(label, fs);
                    DrawText(label, static_cast<int>(rec.x + (rec.width - tw) / 2),
                             static_cast<int>(rec.y + (rec.height - fs) / 2), fs, hot ? accent : WHITE);
                };
                const bool hotNew = In(canvas, newMatchBtn);
                const bool hotTitle = In(canvas, titleBtn);
                drawBtn(newMatchBtn, "NEW MATCH", hotNew, Color{255, 213, 25, 255});
                drawBtn(titleBtn, "MAIN MENU", hotTitle, Color{255, 171, 27, 255});
                EndDrawing();
                continue;
            }
            if (overlay != TableOverlay::NONE) {
                if (pressed && In(canvas, TableHud::RadioMenuButton)) {
                    setOverlay(overlay == TableOverlay::RADIO ? TableOverlay::NONE : TableOverlay::RADIO);
                } else if (pressed && In(canvas, TableHud::PauseButton)) {
                    setOverlay(overlay == TableOverlay::PAUSE ? TableOverlay::NONE : TableOverlay::PAUSE);
                } else if (overlay == TableOverlay::PAUSE) {
                    const auto old = options;
                    const auto movement = GetMouseDelta();
                    const Ui::PauseInput input{canvas, movement.x != 0 || movement.y != 0, pressed,
                        IsMouseButtonDown(MOUSE_BUTTON_LEFT), IsKeyPressed(KEY_DOWN) - IsKeyPressed(KEY_UP),
                        IsKeyPressed(KEY_RIGHT) - IsKeyPressed(KEY_LEFT), IsKeyPressed(KEY_ENTER)};
                    const auto pauseAction = pause.Update(options, input, display, desktop);
                    if (old.masterVolume != options.masterVolume && IsAudioDeviceReady())
                        SetMasterVolume(options.masterVolume / 100.0f);
                    if (old.sfxVolume != options.sfxVolume) sfx.SetVolume(options.sfxVolume / 100.0f);
                    if (old.screenMode != options.screenMode || old.resolution != options.resolution)
                        ApplyDisplay(options, desktop);
                    if (old.aaSamples != options.aaSamples) options.aaSamples = scene.SetAntiAliasing(options.aaSamples);
                    if (pauseAction == Ui::PauseAction::Resume) {
                        setOverlay(TableOverlay::NONE);
                    } else if (pauseAction == Ui::PauseAction::Title) {
                        SaveOptions(optionsFile, options);
                        resetMatchProgress();
                        inStartMenu = true;
                        galleryOpen = false;
                        startMenu.AdvanceVariant();
                        game = TableGame();
                        setOverlay(TableOverlay::NONE);
                    }
                } else {
                    if (pressed) {
                        if (In(canvas, TableHud::ResumeBottomButton)) setOverlay(TableOverlay::NONE);
                        else if (In(canvas, TableHud::RadioPowerButton)) radio.ToggleEnabled();
                        else if (In(canvas, TableHud::RadioMuteButton)) radio.ToggleMute();
                        else if (In(canvas, TableHud::VolumeHitBox)) draggingVolume = true;
                        else if (In(canvas, TableHud::NewMatchButton)) {
                            targetRounds = options.matchRounds;
                            resetMatchProgress();
                            game = TableGame();
                            setOverlay(TableOverlay::NONE);
                        } else {
                            for (std::size_t i = 0; i < TableHud::StationSlotCount; ++i)
                                if (In(canvas, TableHud::StationButton(i))) { radio.SelectStation(static_cast<int>(i)); break; }
                        }
                    }
                    if (draggingVolume && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
                        radio.SetVolume((canvas.x - TableHud::VolumeBar.x) / TableHud::VolumeBar.width);
                    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) draggingVolume = false;
                }
            } else if (!pauseKey) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    if (game.Dragging()) game.CancelDrag();
                    else inspectingCard = game.HasCardAt(PlanPoint(canvas));
                }
                if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) inspectingCard = false;
                if (pressed) {
                    if (game.AwaitingDealerChoice() && In(canvas, TableHud::DealerCardLeft)) {
                        game.SelectDealerCard(0);
                    } else if (game.AwaitingDealerChoice() && In(canvas, TableHud::DealerCardRight)) {
                        game.SelectDealerCard(1);
                    } else if (In(canvas, TableHud::RadioMenuButton)) {
                        setOverlay(TableOverlay::RADIO);
                    } else if (In(canvas, TableHud::PauseButton)) {
                        setOverlay(TableOverlay::PAUSE);
                    } else if (game.Rules().GetPhase() == Phase::ROUND_END && In(canvas, TableHud::RoundEndButton)) {
                        const int roundsDone = completedMatchRounds + game.Round();
                        if (roundsDone >= targetRounds) {
                            cumulativeScores = totalMatchScores();
                            matchOver = true;
                        } else if (game.MatchFinished()) {
                            cumulativeScores[0] += game.Scores()[0];
                            cumulativeScores[1] += game.Scores()[1];
                            completedMatchRounds += game.Round();
                            game = TableGame();
                        } else {
                            game.NextRound();
                        }
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
                if (overlay == TableOverlay::NONE) game.Update(dt);
            }

            const float desiredMagnification =
                overlay == TableOverlay::NONE && inspectingCard ? 0.85f : 0.0f;
            const float magnifyBlend = 1.0f - std::exp(-12.0f * dt);
            magnify.amount += (desiredMagnification - magnify.amount) * magnifyBlend;
            magnify.focus.x += (canvas.x - magnify.focus.x) * magnifyBlend;
            magnify.focus.y += (canvas.y - magnify.focus.y) * magnifyBlend;

            for (GameSfx cue : game.TakeSfx()) PlayGameSfx(sfx, cue);
            radio.Update();
            scene.Draw(game, {overlay, radio.Status(), options, pause.Focus(), canvas}, magnify);
        }
        SaveOptions(optionsFile, options);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        result = 1;
    }
    CloseAudioDevice();
    CloseWindow();
    return result;
}
