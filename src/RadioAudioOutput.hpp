#pragma once

#include <cstddef>
#include <cstdint>

// Platform audio output abstraction:
// - On Windows, prefers a chunked DirectSound circular buffer (16 x 2048 frames),
//   with event-driven Pro Audio WASAPI and WinMM waveOut fallbacks.
// - On non-Windows, uses Raylib's AudioStream callback.
class RadioAudioOutput {
public:
    using FillCallback = void (*)(void* user, std::int16_t* out, std::size_t frames);

    RadioAudioOutput() = default;
    ~RadioAudioOutput();
    RadioAudioOutput(const RadioAudioOutput&) = delete;
    RadioAudioOutput& operator=(const RadioAudioOutput&) = delete;

    bool Start(int sampleRate, int channels, FillCallback callback, void* user);
    void Stop();
    void SetGain(float gain);

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] int SampleRate() const;
    [[nodiscard]] int Channels() const;
    [[nodiscard]] const char* BackendName() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};
