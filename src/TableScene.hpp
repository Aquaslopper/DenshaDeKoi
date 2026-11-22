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

    // Eases the digital zoom toward the player's turn. Call once per frame
    // before Draw().
    void Update(const TableGame& game, float dt);
    void Draw(const TableGame& game) const;
    // Visible sub-rectangle of the world canvas, in canvas pixels.
    [[nodiscard]] Rectangle SourceRect() const;

private:
    Texture2D background_{};
    Texture2D atlas_{};
    RenderTexture2D canvas_{};
    float zoom_ = 0.0f;      // 0 = whole table, 1 = fully cropped
    float zoomPhase_ = 0.0f; // eased 0..1 progress behind zoom_
};
