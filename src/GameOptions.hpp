#pragma once

#include <filesystem>

struct Resolution {
    int width, height;
    bool operator==(const Resolution&) const = default;
};

enum class ScreenMode { Windowed, Borderless, Fullscreen };

struct GameOptions {
    int masterVolume = 80;
    int sfxVolume = 70;
    bool cardHints = true;
    Resolution resolution{1360, 768}; // Preferred non-borderless size.
    ScreenMode screenMode = ScreenMode::Windowed;
    int aaSamples = 2;
    bool operator==(const GameOptions&) const = default;
};

// Walk the presets that fit this monitor, including its native size. The
// current window may also have an arbitrary size after dragging its border.
[[nodiscard]] Resolution StepResolution(Resolution current, Resolution desktop, int direction);
[[nodiscard]] std::filesystem::path OptionsFile();
[[nodiscard]] GameOptions LoadOptions(const std::filesystem::path& file);
void SaveOptions(const std::filesystem::path& file, const GameOptions& options);
