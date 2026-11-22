#pragma once

#include "RadioAudioOutput.hpp"
#include "RadioPlaylist.hpp"
#include "RadioStreamWorker.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

enum class BufferMode {
    AUTO,
    SEC_2,
    SEC_5,
    SEC_10,
    SEC_15,
    SEC_20,
};

struct RadioSnapshot {
    RadioState state = RadioState::OFF;
    bool enabled = false;
    int stationIndex = 0; // -1 when playing a custom URL
    std::string stationName;
    std::string nowPlaying;
    std::string statusText;
    std::string customUrl;
    float volume = 0.65f;
    bool muted = false;
    BufferMode bufferMode = BufferMode::AUTO;
    float targetBufferSec = 4.0f;
    float bufferedSec = 0.0f;
    bool rebuffering = false;
    std::uint64_t underruns = 0;
    std::string backendName = "--";
    std::uint64_t resyncs = 0;
    std::uint64_t reconnects = 0;
};

class RadioPlayer {
public:
    RadioPlayer();
    ~RadioPlayer();
    RadioPlayer(const RadioPlayer&) = delete;
    RadioPlayer& operator=(const RadioPlayer&) = delete;

    void SelectStation(int index);
    void PlayCustomUrl(std::string_view url);
    void ToggleEnabled();
    void Stop();
    void SetVolume(float volume);
    void ToggleMute();
    void CycleBufferMode();
    void SetBufferMode(BufferMode mode);
    void Update();

    [[nodiscard]] RadioSnapshot Snapshot() const;

    // Called from the audio output thread.
    void FillAudioFrames(std::int16_t* out, std::size_t frames);

private:
    void StartCurrentSelection();
    void ResetAudioStream();
    [[nodiscard]] float ActiveTargetSeconds() const;
    void SyncTargetFrames(int sampleRate);

    RadioStreamWorker worker_;
    RadioAudioOutput output_;
    bool enabled_ = false;
    int stationIndex_ = 0;
    std::string customUrl_;
    float volume_ = 0.65f;
    bool muted_ = false;
    BufferMode bufferMode_ = BufferMode::AUTO;
    float autoTargetSec_ = 4.0f;
    float bufferedSec_ = 0.0f;

    std::atomic<bool> callbackActive_{false};
    std::atomic<bool> rebuffering_{false};
    std::atomic<bool> fadeInPending_{false};
    std::atomic<int> callbackSampleRate_{0};
    std::atomic<int> callbackChannels_{0};
    std::atomic<std::size_t> targetBufferFrames_{176400};
    std::atomic<std::uint64_t> underrunCount_{0};
    std::uint64_t lastSeenUnderruns_ = 0;
};
