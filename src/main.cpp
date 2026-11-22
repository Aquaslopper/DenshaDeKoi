#include "AtlasManager.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>

namespace {
constexpr int WindowWidth = 1280;
constexpr int WindowHeight = 720;
constexpr float HorizontalGap = 10.0f;
constexpr float VerticalGap = 10.0f;
constexpr float HorizontalMargin = 30.0f;
constexpr float TopMargin = 38.0f;
constexpr float BottomMargin = 58.0f;
constexpr Color StationGrey{0x1a, 0x1c, 0x23, 0xff};

void RunInspector(const AtlasManager& atlas) {
    const float availableWidth = WindowWidth - 2.0f * HorizontalMargin -
                                 (AtlasManager::MonthCount - 1) * HorizontalGap;
    const float availableHeight = WindowHeight - TopMargin - BottomMargin -
                                  (AtlasManager::CardsPerMonth - 1) * VerticalGap;
    const float scale = std::min(availableWidth / (AtlasManager::MonthCount * atlas.GetCardWidth()),
                                 availableHeight / (AtlasManager::CardsPerMonth * atlas.GetCardHeight()));
    const float width = atlas.GetCardWidth() * scale;
    const float height = atlas.GetCardHeight() * scale;
    const float gridWidth = AtlasManager::MonthCount * width +
                            (AtlasManager::MonthCount - 1) * HorizontalGap;
    const float gridHeight = AtlasManager::CardsPerMonth * height +
                             (AtlasManager::CardsPerMonth - 1) * VerticalGap;
    const float startX = (WindowWidth - gridWidth) / 2.0f;
    const float startY = TopMargin + (WindowHeight - TopMargin - BottomMargin - gridHeight) / 2.0f;

    while (!WindowShouldClose()) {
        int hoveredMonth = 0;
        int hoveredIndex = -1;
        const Vector2 mouse = GetMousePosition();

        BeginDrawing();
        ClearBackground(StationGrey);

        for (int row = 0; row < AtlasManager::CardsPerMonth; ++row) {
            for (int column = 0; column < AtlasManager::MonthCount; ++column) {
                const Vector2 position{startX + column * (width + HorizontalGap),
                                       startY + row * (height + VerticalGap)};
                const Rectangle bounds{position.x, position.y, width, height};
                atlas.DrawCard(column + 1, row, position, scale);
                if (CheckCollisionPointRec(mouse, bounds)) {
                    hoveredMonth = column + 1;
                    hoveredIndex = row;
                    DrawRectangleLinesEx(bounds, 2.0f, WHITE);
                }
            }
        }

        if (hoveredMonth != 0) {
            DrawText(TextFormat("Month: %d   Card Index: %d", hoveredMonth, hoveredIndex),
                     static_cast<int>(HorizontalMargin), WindowHeight - 37, 20, WHITE);
        } else {
            DrawText("Hover over a card to inspect it", static_cast<int>(HorizontalMargin),
                     WindowHeight - 37, 20, LIGHTGRAY);
        }
        EndDrawing();
    }
}
} // namespace

int main(int argc, char* argv[]) {
    InitWindow(WindowWidth, WindowHeight, "Densha de Koi - Atlas Inspector");
    if (!IsWindowReady()) {
        std::fprintf(stderr, "Could not create the window.\n");
        return 1;
    }
    SetTargetFPS(60);

    int result = 0;
    try {
        // Optional path makes it possible to inspect another uniformly packed atlas.
        const AtlasManager atlas(argc > 1 ? argv[1] : "cards_atlas.png");
        RunInspector(atlas);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        result = 1;
    }
    // The atlas is destroyed before the graphics context is closed.
    CloseWindow();
    return result;
}
