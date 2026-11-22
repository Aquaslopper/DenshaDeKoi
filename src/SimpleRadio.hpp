#pragma once

#include "RadioEngine.hpp"

#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

// Internet radio for the pause menu: the engine thread fetches and decodes the
// station, and this class pours the PCM into a plain Raylib AudioStream.
// Requires InitAudioDevice(); call Update() once per frame from the main thread.
class SimpleRadio {
public:
    SimpleRadio() = default;
    ~SimpleRadio();
    SimpleRadio(const SimpleRadio&) = delete;
    SimpleRadio& operator=(const SimpleRadio&) = delete;

    void SelectStation(int index);
    void ToggleEnabled();
    void SetVolume(float volume);
    void ToggleMute();

    void Update();
    [[nodiscard]] RadioStatus Status() const;

    // Frames per AudioStream block. Two blocks are kept by Raylib, so this is
    // also the worst-case gap the main loop may take before audio stutters.
    static constexpr int BlockFrames = 4096;

private:
    void StartCurrentStation();
    void ReleaseStream();
    void ApplyVolume();
    [[nodiscard]] bool EnsureStream(int sampleRate, int channels);

    RadioEngine engine_;
    AudioStream stream_{};
    bool streamReady_ = false;
    int streamSampleRate_ = 0;
    int streamChannels_ = 0;

    bool enabled_ = false;
    int stationIndex_ = 0;
    float volume_ = 0.7f;
    bool muted_ = false;
    std::string deviceError_;
    std::vector<std::int16_t> block_;
};
