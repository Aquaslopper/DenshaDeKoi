#pragma once

#include "GameOptions.hpp"
#include "RadioEngine.hpp"
#include "TableHud.hpp"
#include "ui/CardGallery.hpp"
#include "ui/PauseMenu.hpp"
#include "ui/PauseRadioMenu.hpp"
#include "ui/UiViewport.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace Ui {

enum class StartMenuAction { NONE, PLAY, OPTIONS, RADIO, GALLERY, EXIT };

// Main-menu artwork is supplied as four complete 16:9 screens. The hit areas
// and overlay rendering use the same 1360x768 letterboxed UI space as the game.
class StartMenu {
public:
    StartMenu(const std::filesystem::path& appDirectory, std::filesystem::path variantFile)
        : variantFile_(std::move(variantFile)), gallery_(appDirectory) {
        const auto menuDirectory = appDirectory / "options" / "start-menu";
        for (std::size_t i = 0; i < backgrounds_.size(); ++i) {
            backgrounds_[i] = LoadTexture((menuDirectory / ("menu" + std::to_string(i + 1) + ".png")).string().c_str());
        }
        // The transparent composition is also a useful fallback if a composite
        // menu image is missing from a packaged build.
        buttonComposition_ = LoadTexture((menuDirectory / "menu-buttons.png").string().c_str());
        optionsPanel_ = LoadTexture((appDirectory / "options" / "menu.png").string().c_str());
        optionsStripe_ = LoadTexture((appDirectory / "options" / "setting-stripe.png").string().c_str());
        logo_ = LoadTexture((appDirectory / "koidenscha.png").string().c_str());
        headingFont_ = LoadFontEx((appDirectory / "fonts" / "Oxanium-Bold.ttf").string().c_str(), 64, nullptr, 0);
        bodyFont_ = LoadFontEx((appDirectory / "fonts" / "Rajdhani-Bold.ttf").string().c_str(), 48, nullptr, 0);

        int savedVariant = 3;
        std::ifstream input(variantFile_);
        if (input >> savedVariant && savedVariant >= 0 && savedVariant < 4) variant_ = savedVariant;
        AdvanceVariant(); // Every launch opens on the next scene, including across restarts.
    }

    ~StartMenu() {
        for (Texture2D& texture : backgrounds_) if (texture.id != 0) UnloadTexture(texture);
        if (buttonComposition_.id != 0) UnloadTexture(buttonComposition_);
        if (optionsPanel_.id != 0) UnloadTexture(optionsPanel_);
        if (optionsStripe_.id != 0) UnloadTexture(optionsStripe_);
        if (logo_.id != 0) UnloadTexture(logo_);
        if (headingFont_.texture.id != 0) UnloadFont(headingFont_);
        if (bodyFont_.texture.id != 0) UnloadFont(bodyFont_);
    }

    StartMenu(const StartMenu&) = delete;
    StartMenu& operator=(const StartMenu&) = delete;

    void AdvanceVariant() {
        variant_ = (variant_ + 1) % static_cast<int>(backgrounds_.size());
        std::error_code error;
        std::filesystem::create_directories(variantFile_.parent_path(), error);
        std::ofstream output(variantFile_, std::ios::trunc);
        if (output) output << variant_ << '\n';
    }

    [[nodiscard]] int Variant() const { return variant_; }
    [[nodiscard]] OptionRow Focus() const { return optionMenu_.Focus(); }
    [[nodiscard]] PauseMenu& OptionsMenu() { return optionMenu_; }

    [[nodiscard]] PauseRadioMenuAssets RadioAssets() const {
        return {logo_, headingFont_, bodyFont_};
    }

    [[nodiscard]] StartMenuAction Update(TableLayout::Point2 pointer, bool moved, bool pressed,
                                         int vertical, bool activate) {
        if (moved) {
            for (int i = 0; i < static_cast<int>(Buttons.size()); ++i) {
                if (Contains(Buttons[static_cast<std::size_t>(i)], pointer)) {
                    selected_ = i;
                    break;
                }
            }
        }
        if (vertical != 0) selected_ = (selected_ + vertical + static_cast<int>(Buttons.size())) % static_cast<int>(Buttons.size());
        if (pressed) {
            for (int i = 0; i < static_cast<int>(Buttons.size()); ++i) {
                if (Contains(Buttons[static_cast<std::size_t>(i)], pointer)) {
                    selected_ = i;
                    return ActionFor(i);
                }
            }
        }
        return activate ? ActionFor(selected_) : StartMenuAction::NONE;
    }

    void DrawBackground(const Viewport& viewport) const {
        const Texture2D background = backgrounds_[static_cast<std::size_t>(variant_)];
        const Rectangle destination = viewport.box;
        if (background.id != 0) {
            DrawTexturePro(background, {0, 0, static_cast<float>(background.width), static_cast<float>(background.height)},
                           destination, {0, 0}, 0, WHITE);
        } else if (buttonComposition_.id != 0) {
            ClearBackground(BLACK);
            DrawTexturePro(buttonComposition_, {0, 0, static_cast<float>(buttonComposition_.width),
                                                static_cast<float>(buttonComposition_.height)},
                           destination, {0, 0}, 0, WHITE);
        } else {
            ClearBackground(Color{10, 22, 54, 255});
            DrawText("DENSHA DE KOI", viewport.X(470), viewport.Y(190), viewport.Font(64), WHITE);
        }

        // Keep the supplied artwork clean: indicate focus with a feathered,
        // blue-white aura instead of drawing a hard box around the button.
        const Rectangle focus = viewport.Rec(Buttons[static_cast<std::size_t>(selected_)]);
        constexpr std::array<float, 4> spreads{14.0f, 9.0f, 4.0f, 0.0f};
        constexpr std::array<float, 4> opacities{7.0f, 9.0f, 13.0f, 18.0f};
        for (std::size_t layer = 0; layer < spreads.size(); ++layer) {
            const float spread = spreads[layer] * viewport.scale;
            const Rectangle halo{focus.x - spread, focus.y - spread,
                                 focus.width + 2.0f * spread, focus.height + 2.0f * spread};
            const float thickness = std::max(2.0f * viewport.scale, spread * 1.35f);
            DrawRectangleRoundedLinesEx(halo, 0.18f, 16, thickness,
                Color{151, 211, 255, static_cast<unsigned char>(opacities[layer])});
        }
        DrawRectangleRounded(focus, 0.16f, 16, Color{151, 211, 255, 10});
    }

    void DrawOptions(const Viewport& viewport, const GameOptions& options, Resolution display) const {
        DrawPauseMenu(viewport, options, display, optionMenu_.Focus(),
                      {optionsPanel_, optionsStripe_, headingFont_, bodyFont_});
    }

    void DrawRadio(const Viewport& viewport, const RadioStatus& radio) const {
        DrawPauseRadioMenu(viewport, radio, RadioAssets());
    }

    void DrawGallery(const Viewport& viewport) const {
        gallery_.Draw(viewport, headingFont_, bodyFont_);
    }

    [[nodiscard]] bool UpdateGallery(const GalleryInput& input) {
        return gallery_.Update(input);
    }

    [[nodiscard]] bool GalleryViewerOpen() const { return gallery_.ViewerOpen(); }

    inline static constexpr std::array<TableLayout::Rect, 5> Buttons{{
        {213, 512, 176, 148}, {409, 512, 177, 148}, {609, 512, 178, 148},
        {810, 512, 179, 148}, {1011, 512, 179, 148}
    }};
    inline static constexpr TableLayout::Rect GalleryBack{570, 500, 220, 64};

private:
    static bool Contains(TableLayout::Rect rect, TableLayout::Point2 p) {
        return p.x >= rect.x && p.x < rect.x + rect.width && p.y >= rect.y && p.y < rect.y + rect.height;
    }

    static StartMenuAction ActionFor(int selected) {
        switch (selected) {
            case 0: return StartMenuAction::PLAY;
            case 1: return StartMenuAction::OPTIONS;
            case 2: return StartMenuAction::RADIO;
            case 3: return StartMenuAction::GALLERY;
            case 4: return StartMenuAction::EXIT;
            default: return StartMenuAction::NONE;
        }
    }

    std::filesystem::path variantFile_;
    std::array<Texture2D, 4> backgrounds_{};
    Texture2D buttonComposition_{};
    Texture2D optionsPanel_{};
    Texture2D optionsStripe_{};
    Texture2D logo_{};
    Font headingFont_{};
    Font bodyFont_{};
    PauseMenu optionMenu_{};
    int variant_ = 3;
    int selected_ = 0;
    CardGallery gallery_;
};

} // namespace Ui
