#pragma once

#include "RadioAudioOutput.hpp"
#include "RadioPlaylist.hpp"
#include "RadioStreamWorker.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// --- Streaming buffer policy -------------------------------------------------
// These rules decide when streaming (re)starts and how the AUTO buffer grows or
// shrinks. They are free-standing so they can be unit tested without an audio
// device: the audio callback must never be able to stall for seconds because of
// a single missed period.
//
// Starting playback uses the prebuffer slider as a share of the configured
// buffer, which doubles as the playout cushion. Resuming after a dropout uses a
// much smaller threshold: the buffer only refills at the rate of the stream, so
// demanding the full cushion again would cost seconds of silence. Both ends are
// clamped so a shortfall never leaves playback waiting forever or on nothing.
inline constexpr float kRadioMinResumeSeconds = 0.20f;
inline constexpr float kRadioMaxResumeSeconds = 1.50f;

// AUTO mode walks this ladder upwards on an underrun and back down again once
// the stream has been stable for kRadioAutoDecaySeconds.
inline constexpr std::array<float, 2> kRadioAutoBufferLadderSec = {4.0f, 6.0f};
inline constexpr double kRadioAutoDecaySeconds = 20.0;

namespace radio_buffer_detail {
inline double ClampedSeconds(std::size_t targetFrames, int sampleRate, float ratio, double maxSeconds) {
    const int sr = sampleRate > 0 ? sampleRate : 44100;
    const double targetSec = std::max(static_cast<double>(targetFrames) / static_cast<double>(sr), 0.0);
    const double hi = std::min(maxSeconds, targetSec);
    const double lo = std::min(static_cast<double>(kRadioMinResumeSeconds), hi);
    const double wanted = targetSec * static_cast<double>(std::clamp(ratio, 0.0f, 1.0f));
    return std::clamp(wanted, lo, hi);
}
} // namespace radio_buffer_detail

// Frames required before playback starts: the full prebuffer share, i.e. the
// cushion that has to absorb network jitter.
inline std::size_t RadioStartThresholdFrames(std::size_t targetFrames, int sampleRate, float ratio) {
    const int sr = sampleRate > 0 ? sampleRate : 44100;
    const double targetSec = static_cast<double>(targetFrames) / static_cast<double>(sr);
    return static_cast<std::size_t>(
        radio_buffer_detail::ClampedSeconds(targetFrames, sr, ratio, targetSec) * static_cast<double>(sr));
}

// Frames required before playback resumes after a dropout: capped so a glitch
// cannot escalate into seconds of silence.
inline std::size_t RadioResumeThresholdFrames(std::size_t targetFrames, int sampleRate, float ratio) {
    const int sr = sampleRate > 0 ? sampleRate : 44100;
    return static_cast<std::size_t>(
        radio_buffer_detail::ClampedSeconds(targetFrames, sr, ratio, static_cast<double>(kRadioMaxResumeSeconds)) *
        static_cast<double>(sr));
}

// Next AUTO buffer target: step up on an underrun, decay after a stable run.
inline float RadioNextAutoBufferTarget(float currentSec, bool underrunSinceLastCheck, double stableSeconds) {
    const auto& ladder = kRadioAutoBufferLadderSec;
    if (underrunSinceLastCheck) {
        for (float step : ladder) {
            if (step > currentSec + 0.1f) return step;
        }
        return ladder.back();
    }
    if (stableSeconds >= kRadioAutoDecaySeconds && currentSec > ladder.front() + 0.1f) {
        for (auto it = ladder.rbegin(); it != ladder.rend(); ++it) {
            if (*it < currentSec - 0.1f) return *it;
        }
        return ladder.front();
    }
    return currentSec;
}

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
    float prebufferRatio = 0.75f;
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
    void SetPrebufferRatio(float ratio);
    void ApplyPrebufferThreshold();
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
    std::atomic<float> prebufferRatio_{0.75f};
    std::atomic<std::uint64_t> underrunCount_{0};
    std::uint64_t lastSeenUnderruns_ = 0;
    std::chrono::steady_clock::time_point lastUnderrunTime_{std::chrono::steady_clock::now()};
};
