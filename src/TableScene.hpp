#pragma once

#include "TableGame.hpp" // Parse card enums before Raylib defines its BLUE color macro.
#include "TableHud.hpp"
#include <raylib.h>

// Everything the pause menu needs beyond the baked table background:
// the baked DENSHA DE KOI wordmark and pixel fonts loaded at native sizes.
struct TableSceneUiAssets {
    Font title64{};  // Silkscreen Bold native 64 px (PAUSE)
    Font title32{};  // Silkscreen Bold native 32 px (MENU)
    Font title24{};  // Silkscreen Bold native 24 px (PAUSED / BGM / 70% / OFF / MUTE)
    Font title16{};  // Silkscreen Bold native 16 px (STATION / chip)
    Font vt42{};     // VT323 native 42 px (station readout name)
    Font vt28{};     // VT323 native 28 px (station button titles)
    Font vt25{};     // VT323 native 25 px ([Radio status])
    Font vt24{};     // VT323 native 24 px (footer hints)
    Font vt21{};     // VT323 native 21 px (station button subtitles / chip tag)
    Texture2D logo{};
    bool logoValid = false;
};

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

    void Draw(const TableGame& game, const TableHudState& hud = {}) const;

private:
    Texture2D background_{};
    Texture2D atlas_{};
    Texture2D cardBack_{};
    // Baked wordmark + pixel fonts for the pause menu, loaded once at startup
    // (see assets/densha-logo.png and assets/fonts/).
    TableSceneUiAssets assets_{};
    // Fixed 1360x768 world canvas. It only provides aspect-preserving
    // letterboxing when the window is resized; it is never cropped or zoomed.
    RenderTexture2D canvas_{};
};
