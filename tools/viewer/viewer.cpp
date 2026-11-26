// Headless scene viewer, adapted from SPACESHIP/tools/viewer. Uses the game's
// actual menu and gallery renderers with raylib's PLATFORM_MEMORY backend.
#include "DeckRegister.hpp"
#include "TableScene.hpp"
#include "ui/StartMenu.hpp"
#include "ui/MatchSettingsMenu.hpp"
#include <raylib.h>
#include <rlgl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

namespace fs = std::filesystem;
int main(int argc, char** argv) {
    if (argc != 3 || !std::getenv("VIEWER_OUT")) { std::cerr << "Usage: run.sh SCRIPT\n"; return 2; }
    std::ifstream script(argv[1]);
    if (!script) { std::cerr << "Cannot open script\n"; return 2; }
    const fs::path root = fs::absolute(argv[2]);
    // The desktop program copies these assets alongside its executable.
    const fs::path runtime = fs::absolute(root / "tools/viewer/build/runtime");
    fs::create_directories(runtime);
    for (auto [name, target] : std::vector<std::pair<std::string, fs::path>>{
             {"options", root / "assets/options"}, {"fonts", root / "assets/fonts"},
             {"koidenscha.png", root / "assets/koidenscha.png"}}) {
        auto link = runtime / name;
        if (!fs::exists(link)) fs::create_symlink(target, link);
    }
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1360, 768, "Densha de Koi viewer");
    {
        // Variant file starts at 3 so the first menu is always variant 0.
        { std::ofstream f(runtime / "variant.txt"); f << 3; }
        Ui::StartMenu menu(runtime, runtime / "variant.txt");
        Ui::MatchSettingsMenu settings(runtime);
        TableGame game(1);
        TableScene table((root / "assets/the-city.jpg").c_str(), (root / "cards_atlas.png").c_str(),
                         (root / "assets/koidenscha.png").c_str(), (root / "assets/fonts/Oxanium-Bold.ttf").c_str(),
                         (root / "assets/fonts/Rajdhani-Bold.ttf").c_str(), root / "assets/options");
        Ui::Viewport viewport{{0, 0, 1360, 768}, 1};
        GameOptions options;
        RadioStatus radio;
        std::string scene = "start", line, command;
        int lineNumber = 0;
        auto draw = [&] {
            if (scene == "match") { table.Draw(game); return; }
            BeginDrawing(); ClearBackground(BLACK);
            menu.DrawBackground(viewport);
            if (scene == "settings") settings.Draw(viewport, {-100, -100});
            else if (scene == "options") menu.DrawOptions(viewport, options, {1360, 768});
            else if (scene == "radio") menu.DrawRadio(viewport, radio);
            else if (scene == "gallery") menu.DrawGallery(viewport);
            EndDrawing();
        };
        while (std::getline(script, line)) {
            ++lineNumber;
            std::istringstream in(line); in >> command;
            if (!in || command[0] == '#') continue;
            if (command == "scene") {
                in >> scene;
                if (scene != "start" && scene != "settings" && scene != "options" && scene != "radio" && scene != "gallery" && scene != "match") { std::cerr << "Invalid scene at line " << lineNumber << '\n'; return 3; }
                if (scene == "gallery") (void)menu.UpdateGallery({});
            } else if (command == "variant") {
                int n = -1; in >> n;
                if (n < 1 || n > 4) { std::cerr << "Invalid variant at line " << lineNumber << '\n'; return 3; }
                while (menu.Variant() != n - 1) menu.AdvanceVariant();
            } else if (command == "month") {
                int n = 0; in >> n;
                if (n < 1 || n > 12) { std::cerr << "Invalid month at line " << lineNumber << '\n'; return 3; }
                // Click the corresponding gallery month strip; settle its fade.
                Ui::GalleryInput input{}; input.vertical = 0;
                // Number of steps is tracked here so month commands work repeatedly.
                static int current = 1;
                input.vertical = n - current; current = n;
                (void)menu.UpdateGallery(input);
                for (int i=0;i<15;i++) { draw(); (void)menu.UpdateGallery({}); }
            } else if (command == "play") {
                // Advance the real controller until the player may drag a card.
                for (int i = 0; i < 3000 && (game.Busy() || game.Rules().GetPhase() != Phase::AWAIT_HAND_PLAY || game.Rules().CurrentPlayer() != Player::PLAYER); ++i) game.Update(0.016f);
                if (game.Busy() || game.Rules().GetPhase() != Phase::AWAIT_HAND_PLAY || game.Rules().CurrentPlayer() != Player::PLAYER) { std::cerr << "No playable turn\n"; return 3; }
                const auto hand = game.Rules().Hand(Player::PLAYER);
                bool played = false;
                for (size_t i = 0; i < hand.size() && !played; ++i) {
                    for (int field : game.Rules().Field()) {
                        if (DeckRegister::At(field).month != DeckRegister::At(hand[i]).month) continue;
                        auto h = TableLayout::PlayerHand[i]; auto f = game.FieldRect(field);
                        TableLayout::Point2 from{h.x+h.width/2, h.y+h.height/2};
                        TableLayout::Point2 to{f.x+f.width/2, f.y+f.height/2};
                        game.PointerDown(from); game.PointerMove(to); game.PointerUp(to);
                        played = game.Busy();
                        if (played) break;
                    }
                }
                if (!played) { std::cerr << "No matching card was played\n"; return 3; }
                game.Update(0.12f); // Capture the played card on the table during its landing.
            } else if (command == "shot") {
                std::string name; in >> name;
                if (name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != std::string::npos) { std::cerr << "Invalid shot name at line " << lineNumber << '\n'; return 3; }
                draw();
                std::vector<unsigned char> pixels(1360 * 768 * 4);
                rlCopyFramebuffer(0, 0, 1360, 768, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, pixels.data());
                for (size_t i = 0; i < pixels.size(); i += 4) { std::swap(pixels[i], pixels[i+2]); pixels[i+3] = 255; }
                Image image{pixels.data(), 1360, 768, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                const auto path = fs::path(std::getenv("VIEWER_OUT")) / (name + ".png");
                if (!ExportImage(image, path.string().c_str())) { std::cerr << "Cannot write " << path << '\n'; return 3; }
                std::cout << path << '\n';
            } else { std::cerr << "Unknown command at line " << lineNumber << '\n'; return 3; }
        }
    }
    CloseWindow();
}
