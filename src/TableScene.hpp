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

    void Draw(const TableGame& game) const;

private:
    Texture2D background_{};
    Texture2D atlas_{};
    Texture2D cardBack_{};
    // Fixed 1360x768 world canvas. It only provides aspect-preserving
    // letterboxing when the window is resized; it is never cropped or zoomed.
    RenderTexture2D canvas_{};
};
