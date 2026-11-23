#include "TableGame.hpp"
#include "TableHud.hpp"
#include "TableScene.hpp"
#include "SimpleRadio.hpp"
#include "SfxPlayer.hpp"
#include "ui/PauseMenu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>

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
        case GameSfx::KOI_KOI: player.Play(SfxId::KoiKoi); break;
        case GameSfx::SHOUBU: player.Play(SfxId::Shobu); break;
        case GameSfx::ROUND_WIN: player.Play(SfxId::RoundWin); break;
        case GameSfx::ROUND_LOSE: player.Play(SfxId::RoundLose); break;
    }
}
struct GalleryCard {
    std::string id, kind, train, description;
    int month = 1;
};
std::string JsonString(const std::string& object, const std::string& key) {
    const std::string marker = std::string(1, '"') + key + '"';
    auto pos = object.find(marker);
    if (pos == std::string::npos) return {};
    pos = object.find(':', pos + marker.size());
    if (pos == std::string::npos || (pos = object.find('"', pos)) == std::string::npos) return {};
    std::string value;
    for (++pos; pos < object.size(); ++pos) {
        const char c = object[pos];
        if (c == '"') break;
        if (c == '\\' && pos + 1 < object.size()) {
            const char next = object[++pos];
            value += next == 'n' ? ' ' : next;
        } else value += c;
    }
    return value;
}
std::vector<GalleryCard> ReadGalleryCards(const std::filesystem::path& directory) {
    std::vector<GalleryCard> cards;
    std::ifstream csv(directory / "cards.csv");
    std::string line;
    std::getline(csv, line);
    while (std::getline(csv, line)) {
        std::stringstream row(line);
        std::string file, month, type, kind;
        std::getline(row, file, ','); std::getline(row, month, ',');
        std::getline(row, type, ','); std::getline(row, kind, ',');
        const auto slash = file.find_last_of('/');
        const auto dot = file.find_last_of('.');
        if (slash == std::string::npos || dot == std::string::npos) continue;
        cards.push_back({file.substr(slash + 1, dot - slash - 1), kind, {}, {}, std::stoi(month)});
    }
    std::ifstream jsonFile(directory / "gallery_data.json");
    const std::string json((std::istreambuf_iterator<char>(jsonFile)), std::istreambuf_iterator<char>());
    for (auto& card : cards) {
        const std::string marker = "\"id\": \"" + card.id + "\"";
        auto begin = json.find(marker);
        if (begin == std::string::npos) continue;
        begin = json.rfind('{', begin);
        const auto end = json.find('}', begin);
        if (begin == std::string::npos || end == std::string::npos) continue;
        const auto object = json.substr(begin, end - begin + 1);
        card.train = JsonString(object, "train_model");
        card.description = JsonString(object, "description");
        if (card.description.empty()) card.description = JsonString(object, "motif_twist");
    }
    return cards;
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
        std::array<Texture2D, 4> titleArt{};
        for (int i = 0; i < 4; ++i) {
            const std::string path = (appDir / "title" / ("menu" + std::to_string(i + 1) + ".png")).string();
            titleArt[static_cast<std::size_t>(i)] = LoadTexture(path.c_str());
            if (titleArt[static_cast<std::size_t>(i)].id == 0)
                throw std::runtime_error("Could not load title menu art: " + path);
        }
        std::size_t titleVariant = 0;
        bool titleScreen = true;
        bool galleryPlaceholder = false;
        const auto galleryDirectory = appDir / "card-gallery";
        const auto galleryCards = ReadGalleryCards(galleryDirectory);
        std::array<Texture2D, 4> galleryTextures{};
        Texture2D galleryRaw{};
        int galleryMonth = 1, selectedGalleryCard = -1;
        bool showingRaw = false, draggingGallery = false;
        float galleryRotation = 0.0f, galleryZoom = 1.0f;
        const auto loadGalleryMonth = [&](int month) {
            for (auto& texture : galleryTextures) { if (texture.id) UnloadTexture(texture); texture = {}; }
            if (galleryRaw.id) { UnloadTexture(galleryRaw); galleryRaw = {}; }
            selectedGalleryCard = -1; showingRaw = false;
            galleryMonth = month;
            int slot = 0;
            for (const auto& card : galleryCards) if (card.month == month && slot < 4) {
                const auto path = galleryDirectory / "cards" / (card.id + ".jpg");
                galleryTextures[static_cast<std::size_t>(slot++)] = LoadTexture(path.string().c_str());
            }
        };
        loadGalleryMonth(galleryMonth);
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

        while (!WindowShouldClose()) {
            const float dt = std::min(GetFrameTime(), 0.1f);
            const Resolution display{GetScreenWidth(), GetScreenHeight()};
            if (display.width <= 0 || display.height <= 0) {
                radio.Update();
                BeginDrawing();
                EndDrawing(); // Keep polling events while minimized.
                continue;
            }
            if (titleScreen) {
                if (galleryPlaceholder) {
                    if (IsKeyPressed(KEY_ESCAPE) && selectedGalleryCard >= 0) selectedGalleryCard = -1;
                    else if (IsKeyPressed(KEY_ESCAPE)) {
                        galleryPlaceholder = false;
                        titleVariant = (titleVariant + 1) % titleArt.size();
                    }
                    if (selectedGalleryCard >= 0) {
                        if (IsKeyPressed(KEY_LEFT)) galleryRotation -= 8;
                        if (IsKeyPressed(KEY_RIGHT)) galleryRotation += 8;
                        galleryZoom = std::clamp(galleryZoom + GetMouseWheelMove() * 0.08f, 0.65f, 1.75f);
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) draggingGallery = true;
                        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) draggingGallery = false;
                        if (draggingGallery) galleryRotation += GetMouseDelta().x * 0.7f;
                    } else {
                        if (IsKeyPressed(KEY_LEFT)) loadGalleryMonth((galleryMonth + 10) % 12 + 1);
                        if (IsKeyPressed(KEY_RIGHT)) loadGalleryMonth(galleryMonth % 12 + 1);
                    }
                }
                BeginDrawing();
                ClearBackground(BLACK);
                const auto& art = titleArt[titleVariant];
                DrawTexturePro(art, {0, 0, static_cast<float>(art.width), static_cast<float>(art.height)},
                    {0, 0, static_cast<float>(display.width), static_cast<float>(display.height)},
                    {0, 0}, 0, WHITE);
                if (galleryPlaceholder) {
                    const float sx = display.width / 1360.0f, sy = display.height / 768.0f;
                    DrawRectangle(0, 0, display.width, display.height, Color{2, 7, 19, 208});
                    const Rectangle panel{20*sx, 18*sy, 1320*sx, 732*sy};
                    DrawRectangleRounded(panel, 0.025f, 8, Color{10, 19, 34, 247});
                    DrawRectangleLinesEx(panel, 2*sx, Color{68, 206, 232, 255});
                    DrawText("DENSHA DE GO!  /  CARD ARCHIVE", 48*sx, 34*sy, 24*sx, Color{115, 224, 239, 255});
                    DrawText(TextFormat("MONTH %02d / 12", galleryMonth), 48*sx, 72*sy, 30*sx, WHITE);
                    DrawText("ESC  BACK", 1160*sx, 40*sy, 17*sx, Color{178, 201, 215, 255});
                    DrawLineEx({45*sx, 117*sy}, {1315*sx, 117*sy}, 2*sx, Color{55, 112, 134, 255});
                    if (selectedGalleryCard < 0) {
                        DrawText("SELECT A CARD", 48*sx, 132*sy, 17*sx, Color{147, 177, 192, 255});
                        DrawText("<", 26*sx, 401*sy, 34*sx, SKYBLUE);
                        DrawText(">", 1300*sx, 401*sy, 34*sx, SKYBLUE);
                        int slot = 0;
                        for (std::size_t i = 0; i < galleryCards.size(); ++i) {
                            const auto& card = galleryCards[i];
                            if (card.month != galleryMonth) continue;
                            const float x = (72 + slot * 310) * sx;
                            const Rectangle box{x, 174*sy, 252*sx, 388*sy};
                            DrawRectangleRounded(box, 0.025f, 5, Color{17, 34, 51, 255});
                            DrawRectangleLinesEx(box, 1.5f*sx, Color{70, 150, 175, 255});
                            if (galleryTextures[static_cast<std::size_t>(slot)].id) {
                                const auto& texture = galleryTextures[static_cast<std::size_t>(slot)];
                                DrawTexturePro(texture, {0,0,float(texture.width),float(texture.height)},
                                    {x+10*sx, 184*sy, 232*sx, 336*sy}, {0,0}, 0, WHITE);
                            }
                            DrawText(card.id.c_str(), x+10*sx, 531*sy, 18*sx, Color{103, 226, 239, 255});
                            DrawText(card.kind.c_str(), x+63*sx, 531*sy, 17*sx, WHITE);
                            ++slot;
                        }
                        DrawText("ARROWS  CHANGE MONTH     CLICK  INSPECT", 48*sx, 604*sy, 17*sx, Color{164, 185, 198, 255});
                    } else {
                        int slot = 0;
                        const GalleryCard* card = nullptr;
                        for (const auto& item : galleryCards) if (item.month == galleryMonth) {
                            if (slot++ == selectedGalleryCard) { card = &item; break; }
                        }
                        if (card && galleryTextures[static_cast<std::size_t>(selectedGalleryCard)].id) {
                            const auto& texture = galleryTextures[static_cast<std::size_t>(selectedGalleryCard)];
                            const Texture2D shown = showingRaw && galleryRaw.id ? galleryRaw : texture;
                            const float zoom = galleryZoom;
                            DrawRectangleRounded({53*sx, 139*sy, 488*sx, 570*sy}, 0.02f, 5, Color{7, 13, 24, 255});
                            DrawTexturePro(shown, {0,0,float(shown.width),float(shown.height)},
                                {297*sx, 424*sy, 248*zoom*sx, 382*zoom*sy}, {124*zoom*sx,191*zoom*sy}, galleryRotation, WHITE);
                            DrawText(card->id.c_str(), 573*sx, 158*sy, 34*sx, Color{103, 226, 239, 255});
                            DrawText(card->kind.c_str(), 573*sx, 203*sy, 25*sx, WHITE);
                            DrawText(card->train.empty() ? "Train details unavailable" : card->train.c_str(), 573*sx, 250*sy, 20*sx, Color{255, 205, 119, 255});
                            DrawText("ARTWORK NOTES", 573*sx, 300*sy, 16*sx, Color{103, 226, 239, 255});
                            std::string remaining = card->description.empty() ? "No description available." : card->description;
                            int descriptionY = 328;
                            while (!remaining.empty() && descriptionY < 520) {
                                std::size_t count = std::min<std::size_t>(72, remaining.size());
                                if (count < remaining.size()) {
                                    const auto space = remaining.rfind(' ', count);
                                    if (space != std::string::npos && space > 20) count = space;
                                }
                                DrawText(remaining.substr(0, count).c_str(), 573*sx, descriptionY*sy, 18*sx, Color{218, 226, 232, 255});
                                remaining.erase(0, count);
                                while (!remaining.empty() && remaining.front() == ' ') remaining.erase(0, 1);
                                descriptionY += 25;
                            }
                            DrawText("HOLD + DRAG TO ROTATE   WHEEL TO ZOOM", 573*sx, 540*sy, 16*sx, Color{147, 177, 192, 255});
                            DrawRectangleRounded({573*sx, 592*sy, 184*sx, 48*sy}, 0.1f, 5, showingRaw ? Color{137, 54, 69, 255} : Color{26, 94, 121, 255});
                            DrawText(showingRaw ? "SHOW CARD ART" : "SHOW RAW", 594*sx, 607*sy, 18*sx, WHITE);
                        }
                    }
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        const Vector2 p = GetMousePosition();
                        const float x = p.x / sx, y = p.y / sy;
                        if (selectedGalleryCard >= 0 && x > 560 && y > 580 && y < 650) {
                            showingRaw = !showingRaw;
                            if (galleryRaw.id) { UnloadTexture(galleryRaw); galleryRaw = {}; }
                            if (showingRaw) {
                                int index = 0;
                                for (const auto& item : galleryCards) if (item.month == galleryMonth) {
                                    if (index++ == selectedGalleryCard) {
                                        const auto rawPath = galleryDirectory / "raw" / (item.id + ".jpg");
                                        galleryRaw = LoadTexture(rawPath.string().c_str());
                                        break;
                                    }
                                }
                            }
                        }
                        else if (selectedGalleryCard < 0 && y > 160 && y < 575) {
                            int slot = static_cast<int>((x - 72) / 310);
                            if (slot >= 0 && slot < 4 && x >= 72 + slot*310 && x < 324 + slot*310) {
                                selectedGalleryCard = slot; showingRaw = false; galleryRotation = 0; galleryZoom = 1;
                            } else if (x < 65) loadGalleryMonth((galleryMonth + 10) % 12 + 1);
                            else if (x > 1280) loadGalleryMonth(galleryMonth % 12 + 1);
                        }
                    }
                } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    const Vector2 p = GetMousePosition();
                    const float nx = p.x / display.width, ny = p.y / display.height;
                    if (ny > 0.64f && ny < 0.88f) {
                        if (nx < 0.29f) { titleScreen = false; overlay = TableOverlay::NONE; }
                        else if (nx < 0.43f) { titleScreen = false; overlay = TableOverlay::PAUSE; }
                        else if (nx < 0.58f) { titleScreen = false; overlay = TableOverlay::RADIO; }
                        else if (nx < 0.72f) galleryPlaceholder = true;
                        else if (nx < 0.88f) SetWindowShouldClose(true);
                    }
                }
                EndDrawing();
                if (!titleScreen) titleVariant = (titleVariant + 1) % titleArt.size();
                continue;
            }
            if (overlay == TableOverlay::PAUSE && options.screenMode != ScreenMode::Fullscreen) desktop = MonitorSize();
            if (options.screenMode == ScreenMode::Windowed) options.resolution = display;
            const bool pauseKey = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P);
            if (pauseKey) setOverlay(overlay == TableOverlay::PAUSE ? TableOverlay::NONE : TableOverlay::PAUSE);
            if (IsKeyPressed(KEY_M)) {
                radio.ToggleMute();
            }

            const auto canvas = CanvasPoint(GetMousePosition());
            const bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
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
        for (const auto& art : titleArt) if (art.id != 0) UnloadTexture(art);
        for (const auto& texture : galleryTextures) if (texture.id != 0) UnloadTexture(texture);
        if (galleryRaw.id != 0) UnloadTexture(galleryRaw);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        result = 1;
    }
    CloseAudioDevice();
    CloseWindow();
    return result;
}
