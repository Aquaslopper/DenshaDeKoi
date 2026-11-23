#pragma once

#include <array>
#include <filesystem>

#include "raylib.h"

enum class SfxId {
    CardDeal,
    CardFlip,
    CardPlace,
    CardMatch,
    CardCapture,
    KoiKoi,
    Shobu,
    RoundWin,
    RoundLose,
    Count
};

// Optional sound-effect bank. Missing files are deliberately silent, so artists
// can add and replace effects in assets/sfx without touching the code.
class SfxPlayer {
public:
    explicit SfxPlayer(const std::filesystem::path& directory);
    ~SfxPlayer();
    SfxPlayer(const SfxPlayer&) = delete;
    SfxPlayer& operator=(const SfxPlayer&) = delete;

    void Play(SfxId id);
    void SetVolume(float volume);

private:
    struct Entry {
        Sound sound{};
        bool loaded = false;
    };
    std::array<Entry, static_cast<std::size_t>(SfxId::Count)> sounds_{};
    float volume_ = 1.0f;
};
