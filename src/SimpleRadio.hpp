#pragma once

#include "RadioEngine.hpp"

#include <raylib.h>

#include <atomic>
#include <string>
#include <vector>

// Internet radio for the pause menu. The engine thread fetches and decodes a
// station into a PCM queue; Raylib's audio thread pulls the queue through
// PcmPlaybackGate, so the output does not depend on the frame rate and the
// playback waits for a cushion instead of padding every block with silence.
// Requires InitAudioDevice(); call Update() once per frame from the main thread.
class SimpleRadio {
public:
    SimpleRadio() = default;
    ~SimpleRadio();
    SimpleRadio(const SimpleRadio&) = delete;
    SimpleRadio& operator=(const SimpleRadio&) = delete;

    void SelectStation(int index);
    // Plays an explicit stream/playlist URL list; used by SelectStation and tests.
    void PlayUrl(std::string displayName, std::vector<std::string> urls);
    void ToggleEnabled();
    void SetVolume(float volume);
    void ToggleMute();

    void Update();
    [[nodiscard]] RadioStatus Status() const;

private:
    static void StreamCallback(void* buffer, unsigned int frames);
    void FillAudio(std::int16_t* out, unsigned int frames);

    void StartCurrentStation();
    void ReleaseStream();
    void ApplyVolume();
    [[nodiscard]] bool EnsureStream(int sampleRate, int channels);

    RadioEngine engine_;
    PcmPlaybackGate gate_;
    AudioStream stream_{};
    bool streamReady_ = false;
    int streamSampleRate_ = 0;
    std::atomic<int> streamChannels_{0};   // read by the audio thread

    bool enabled_ = false;
    int stationIndex_ = 0;
    float volume_ = 0.7f;
    bool muted_ = false;
    std::string deviceError_;
};
