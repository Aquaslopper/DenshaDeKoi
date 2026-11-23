#include "SfxPlayer.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

namespace {
constexpr std::array<const char*, static_cast<std::size_t>(SfxId::Count)> FileNames{
    "card_deal.wav",
    "card_flip.wav",
    "card_place.wav",
    "card_match.wav",
    "card_capture.wav",
    "koi_koi.wav",
    "shobu.wav",
    "round_win.wav",
    "round_lose.wav",
};
}

SfxPlayer::SfxPlayer(const std::filesystem::path& directory) {
    if (!IsAudioDeviceReady()) return;
    for (std::size_t i = 0; i < sounds_.size(); ++i) {
        const auto path = directory / FileNames[i];
        if (!std::filesystem::exists(path)) continue;
        sounds_[i].sound = LoadSound(path.string().c_str());
        sounds_[i].loaded = IsSoundValid(sounds_[i].sound);
        if (!sounds_[i].loaded)
            std::fprintf(stderr, "[sfx] could not load %s\n", path.string().c_str());
    }
}

SfxPlayer::~SfxPlayer() {
    for (auto& entry : sounds_)
        if (entry.loaded) UnloadSound(entry.sound);
}

void SfxPlayer::Play(SfxId id) {
    auto& entry = sounds_[static_cast<std::size_t>(id)];
    if (!entry.loaded) return;
    PlaySound(entry.sound);
}

void SfxPlayer::SetVolume(float volume) {
    volume = std::clamp(volume, 0.0f, 1.0f);
    for (const auto& entry : sounds_)
        if (entry.loaded) SetSoundVolume(entry.sound, volume);
}
