#pragma once

#include <cstddef>
#include <cstdint>

// Platform audio output abstraction: a miniaudio playback device (the copy
// bundled with Raylib) with a fixed 40 ms period and 4 periods, feeding the
// caller's fill callback from the audio thread. miniaudio picks the backend
// (WASAPI/DirectSound/WinMM on Windows, ALSA/PulseAudio elsewhere).
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
