#include "TableGame.hpp"
#include "ui/Hud.hpp"
#include "TableScene.hpp"
#include "SimpleRadio.hpp"
#include "SfxPlayer.hpp"
#include "ui/PauseMenu.hpp"
#include "ui/MatchSettingsMenu.hpp"
#include "ui/StartMenu.hpp"

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
        Ui::MatchSettingsMenu matchSettingsMenu(appDir);
        TableGame game = TableGame::WithRounds(matchSettingsMenu.SelectedRounds());
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
                inStartMenu = true;
                galleryOpen = false;
                startMenu.AdvanceVariant();
                game = TableGame::WithRounds(matchSettingsMenu.SelectedRounds());
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
                    if (startMenu.OptionsMenu().Update(options, input, display, desktop)) setOverlay(TableOverlay::NONE);
                    if (old.masterVolume != options.masterVolume && IsAudioDeviceReady())
                        SetMasterVolume(options.masterVolume / 100.0f);
                    if (old.sfxVolume != options.sfxVolume) sfx.SetVolume(options.sfxVolume / 100.0f);
                    if (old.screenMode != options.screenMode || old.resolution != options.resolution)
                        ApplyDisplay(options, desktop);
                    if (old.aaSamples != options.aaSamples) options.aaSamples = scene.SetAntiAliasing(options.aaSamples);
                } else if (overlay == TableOverlay::RADIO) {
                    if (pressed) {
                        if (In(canvas, TableHud::ResumeBottomButton)) setOverlay(TableOverlay::NONE);
                        else if (In(canvas, TableHud::NewMatchButton)) {
                            game = TableGame::WithRounds(matchSettingsMenu.SelectedRounds());
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
                        game = TableGame::WithRounds(matchSettingsMenu.SelectedRounds());
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
                    if (pause.Update(options, input, display, desktop)) setOverlay(TableOverlay::NONE);
                    if (old.masterVolume != options.masterVolume && IsAudioDeviceReady())
                        SetMasterVolume(options.masterVolume / 100.0f);
                    if (old.sfxVolume != options.sfxVolume) sfx.SetVolume(options.sfxVolume / 100.0f);
                    if (old.screenMode != options.screenMode || old.resolution != options.resolution)
                        ApplyDisplay(options, desktop);
                    if (old.aaSamples != options.aaSamples) options.aaSamples = scene.SetAntiAliasing(options.aaSamples);
                } else {
                    if (pressed) {
                        if (In(canvas, TableHud::ResumeBottomButton)) setOverlay(TableOverlay::NONE);
                        else if (In(canvas, TableHud::RadioPowerButton)) radio.ToggleEnabled();
                        else if (In(canvas, TableHud::RadioMuteButton)) radio.ToggleMute();
                        else if (In(canvas, TableHud::VolumeHitBox)) draggingVolume = true;
                        else if (In(canvas, TableHud::NewMatchButton)) {
                            game = TableGame::WithRounds(matchSettingsMenu.SelectedRounds());
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
                        if (game.MatchFinished()) game = TableGame::WithRounds(matchSettingsMenu.SelectedRounds());
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
