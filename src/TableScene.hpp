#pragma once

#include "CardPresentation.hpp"

// Owns GPU resources; construct and destroy while the graphics context is alive.
class TableScene {
public:
    TableScene(const char* backgroundPath, const char* atlasPath);
    ~TableScene();
    TableScene(const TableScene&) = delete;
    TableScene& operator=(const TableScene&) = delete;
    TableScene(TableScene&&) = delete;
    TableScene& operator=(TableScene&&) = delete;

    void Draw(const TableGame& game, const Camera3D& camera,
              const CardPresentation& presentation, Vector2 cursor) const;

private:
    Texture2D background_{};
    Texture2D atlas_{};
    RenderTexture2D canvas_{};
};
