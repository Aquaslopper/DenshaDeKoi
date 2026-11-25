#pragma once

#include "ui/UiViewport.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <random>

namespace Ui {

// Title <-> match transition: fade to black, one of four supplied loading
// screens, three-second hold, then the same pair of fades to the other screen.
class LoadingScreen {
public:
    static constexpr float FadeSeconds = 0.05f;
    static constexpr float HoldSeconds = 3.0f;

    explicit LoadingScreen(const std::filesystem::path& appDirectory) {
        const auto directory = appDirectory / "options" / "loading";
        const std::array<const char*, 4> files{"load1.png", "load2.png", "load3.png", "laod4.png"};
        for (std::size_t i = 0; i < images_.size(); ++i)
            images_[i] = LoadTexture((directory / files[i]).string().c_str());
    }

    ~LoadingScreen() {
        for (Texture2D& image : images_) if (image.id != 0) UnloadTexture(image);
    }
    LoadingScreen(const LoadingScreen&) = delete;
    LoadingScreen& operator=(const LoadingScreen&) = delete;

    void Begin(bool toMatch) {
        active_ = true;
        swapped_ = false;
        elapsed_ = 0.0f;
        toMatch_ = toMatch;
        chosen_ = pick_(rng_);
    }

    // Returns true once, when the screen changes under a fully black frame.
    bool Update(float dt) {
        if (!active_) return false;
        elapsed_ += dt;
        if (!swapped_ && elapsed_ >= SwapAt) {
            swapped_ = true;
            return true;
        }
        if (elapsed_ >= Total) active_ = false;
        return false;
    }

    [[nodiscard]] bool Active() const { return active_; }
    [[nodiscard]] bool ToMatch() const { return toMatch_; }

    void Draw(const Viewport& viewport) const {
        if (!active_) return;

        if (!swapped_ && elapsed_ >= FadeSeconds) {
            const Texture2D image = images_[static_cast<std::size_t>(chosen_)];
            if (image.id != 0)
                DrawTexturePro(image, {0, 0, static_cast<float>(image.width), static_cast<float>(image.height)},
                               viewport.box, {0, 0}, 0, WHITE);
        }

        float alpha = 0.0f;
        if (elapsed_ < FadeSeconds) alpha = elapsed_ / FadeSeconds;
        else if (elapsed_ < LoadingStart) alpha = 1.0f - (elapsed_ - FadeSeconds) / FadeSeconds;
        else if (elapsed_ < LoadingEnd) alpha = 0.0f;
        else if (elapsed_ < SwapAt) alpha = (elapsed_ - LoadingEnd) / FadeSeconds;
        else alpha = 1.0f - (elapsed_ - SwapAt) / FadeSeconds;
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{0, 0, 0, static_cast<unsigned char>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)});
    }

private:
    static constexpr float LoadingStart = 2.0f * FadeSeconds;
    static constexpr float LoadingEnd = LoadingStart + HoldSeconds;
    static constexpr float SwapAt = LoadingEnd + FadeSeconds;
    static constexpr float Total = SwapAt + FadeSeconds;

    std::array<Texture2D, 4> images_{};
    std::mt19937 rng_{std::random_device{}()};
    std::uniform_int_distribution<int> pick_{0, 3};
    int chosen_ = 0;
    float elapsed_ = 0.0f;
    bool active_ = false;
    bool swapped_ = false;
    bool toMatch_ = true;
};

// TableScene draws this hook above the match screen; the title path draws it directly.
inline const LoadingScreen* activeLoadingScreen = nullptr;

} // namespace Ui
