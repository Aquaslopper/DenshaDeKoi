#include "TableScene.hpp"

#include "TableLayout.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(TableLayout::ImageWidth, TableLayout::ImageHeight, "Densha de Koi - Table Preview");
    if (!IsWindowReady()) {
        std::fprintf(stderr, "Could not create the window.\n");
        return 1;
    }
    SetWindowMinSize(680, 384);
    SetTargetFPS(60);

    int result = 0;
    try {
        const std::filesystem::path appDir(GetApplicationDirectory());
        const std::string background = (appDir / "the-city.jpg").string();
        const std::string atlas = (appDir / "cards_atlas.png").string();
        const TableScene scene(background.c_str(), atlas.c_str());
        while (!WindowShouldClose()) scene.Draw();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        result = 1;
    }
    // All scene textures are destroyed before the graphics context is closed.
    CloseWindow();
    return result;
}
