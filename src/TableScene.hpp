#pragma once

#include <raylib.h>

// Static perspective table preview. This class owns all of its GPU resources;
// construct and destroy it while a Raylib window/context is alive.
class TableScene {
public:
    TableScene(const char* backgroundPath, const char* atlasPath);
    ~TableScene();
    TableScene(const TableScene&) = delete;
    TableScene& operator=(const TableScene&) = delete;
    TableScene(TableScene&&) = delete;
    TableScene& operator=(TableScene&&) = delete;

    void Draw() const;

private:
    Texture2D background_{};
    Texture2D atlas_{};
    RenderTexture2D canvas_{};
};
