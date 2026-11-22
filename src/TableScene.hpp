#pragma once

#include "TableGame.hpp" // Parse card enums before Raylib defines its BLUE color macro.
#include <raylib.h>

// Perspective table renderer. This class owns all of its GPU resources;
// construct and destroy it while a Raylib window/context is alive.
class TableScene {
public:
    TableScene(const char* backgroundPath, const char* atlasPath);
    ~TableScene();
    TableScene(const TableScene&) = delete;
    TableScene& operator=(const TableScene&) = delete;
    TableScene(TableScene&&) = delete;
    TableScene& operator=(TableScene&&) = delete;

    // Eases the 3D camera between the overview and player-turn framing.
    // `mouse` is the pointer normalised to [-1, 1] around the window centre.
    void Update(const TableGame& game, float dt, Vector2 mouse);
    void Draw(const TableGame& game) const;
    // Screen point (canvas pixels) -> table-plan point for the live camera.
    [[nodiscard]] TableLayout::Point2 PlanPoint(TableLayout::Point2 canvas) const;

private:
    Texture2D background_{};
    Texture2D atlas_{};
    RenderTexture2D canvas_{};
    Camera3D camera_{};
    Vector2 pointer_{}; // Held while dragging so pick and draw stay in sync.
};
