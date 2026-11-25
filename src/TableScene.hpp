#pragma once

#include "TableGame.hpp" // Parse card enums before Raylib defines its BLUE color macro.
#include <raylib.h>

#include <filesystem>

struct MagnifyEffect {
    TableLayout::Point2 focus{TableLayout::ImageWidth * 0.5f, TableLayout::ImageHeight * 0.5f};
    float amount = 0.0f; // 0 = unchanged; 0.2 = 20 percent magnification.
};

// Perspective table renderer. This class owns all of its GPU resources;
// construct and destroy it while a Raylib window/context is alive.
class TableScene {
public:
    TableScene(const char* backgroundPath, const char* atlasPath, const char* logoPath,
               const char* headingFontPath, const char* bodyFontPath,
               const std::filesystem::path& optionsDirectory);
    ~TableScene();
    TableScene(const TableScene&) = delete;
    TableScene& operator=(const TableScene&) = delete;
    TableScene(TableScene&&) = delete;
    TableScene& operator=(TableScene&&) = delete;

    // Returns the applied sample density; keeps the old target on allocation failure.
    int SetAntiAliasing(int samples);
    void Draw(const TableGame& game, const MagnifyEffect& magnify = {}) const;

private:
    Texture2D background_{};
    Texture2D atlas_{};
    Texture2D cardBack_{};
    Texture2D logo_{};
    Texture2D optionsPanel_{}, optionsStripe_{};
    Font headingFont_{};
    Font bodyFont_{};
    bool ownsHeadingFont_ = false;
    bool ownsBodyFont_ = false;
    // Fixed 1360x768 world canvas. It only provides aspect-preserving
    // letterboxing when the window is resized; it is never cropped or zoomed.
    RenderTexture2D canvas_{};
    RenderTexture2D sampledWorld_{}; // Optional supersampled world, resolved before drawing any UI.
    int aaSamples_ = 1;
    Shader magnifyShader_{};
    int magnifyFocusLocation_ = -1;
    int magnifyZoomLocation_ = -1;
};
