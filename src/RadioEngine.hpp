#pragma once

#include "RadioPlaylist.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

enum class RadioState { OFF, CONNECTING, BUFFERING, PLAYING, FAILED };

struct RadioStatus {
    RadioState state = RadioState::OFF;
    bool enabled = false;
    int stationIndex = 0;
    std::string stationName;
    std::string statusText;
    float volume = 0.7f;
    bool muted = false;
    float bufferedSec = 0.0f;
};

// Lock-protected PCM FIFO between the stream thread and the output. Small and
// deliberately boring: one mutex, one circular buffer, no lock-free cleverness.
class PcmQueue {
public:
    explicit PcmQueue(std::size_t capacityFrames = 44100 * 6);

    void Clear();
    void SetFormat(int sampleRate, int channels);
    [[nodiscard]] bool Format(int& sampleRate, int& channels) const;

    // Both return the number of frames actually moved (never more than asked).
    std::size_t Push(const std::int16_t* interleaved, std::size_t frames);
    std::size_t Pop(std::int16_t* out, std::size_t maxFrames);

    [[nodiscard]] std::size_t AvailableFrames() const;
    [[nodiscard]] std::size_t CapacityFrames() const { return capacityFrames_; }

private:
    mutable std::mutex mutex_;
    std::vector<std::int16_t> samples_;
    std::size_t capacityFrames_ = 0;
    std::size_t read_ = 0;
    std::size_t size_ = 0;
    int sampleRate_ = 0;
    int channels_ = 0;
};

// Incremental MP3 decoder built on dr_mp3's low-level push API, so a live stream
// can be fed in arbitrary network-sized chunks.
class Mp3Decoder {
public:
    using PcmCallback =
        std::function<void(const std::int16_t*, std::size_t frames, int sampleRate, int channels)>;

    Mp3Decoder();
    ~Mp3Decoder();
    Mp3Decoder(const Mp3Decoder&) = delete;
    Mp3Decoder& operator=(const Mp3Decoder&) = delete;

    void Reset();
    // `flush` decodes the remaining tail; live streams never need it, tests do.
    void Feed(const std::uint8_t* data, std::size_t size, const PcmCallback& onPcm, bool flush = false);
    [[nodiscard]] std::size_t DecodedFrames() const { return decodedFrames_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::vector<std::uint8_t> inBuf_;
    std::size_t decodedFrames_ = 0;
};

// Decides when the queued PCM may reach the sound card. The device pulls blocks
// faster than a real-time stream fills them, so without a cushion every block is
// half silence. This waits for `StartSeconds` before the first block, and for
// `ResumeSeconds` after the stream ran dry, then lets playback run at 1x.
//
// Fill() runs on the audio thread, Poll() on the main thread.
class PcmPlaybackGate {
public:
    static constexpr float StartSeconds = 2.0f;
    static constexpr float ResumeSeconds = 0.8f;

    // Copies up to `frames` frames into out and returns how many are real audio;
    // the caller silences the remainder. Returns 0 while the cushion builds.
    std::size_t Fill(PcmQueue& queue, std::int16_t* out, std::size_t frames, int channels);

    // Re-arms the cushion after a dropout and starts playback once enough
    // audio is queued. Cheap: call it once per frame.
    void Poll(PcmQueue& queue);

    [[nodiscard]] bool Playing() const { return playing_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::size_t Dropouts() const { return dropouts_; }
    void Reset() { playing_.store(false); starving_.store(false); everStarted_ = false; dropouts_ = 0; }

private:
    std::atomic<bool> playing_{false};
    std::atomic<bool> starving_{false};
    bool everStarted_ = false;     // main thread only
    std::size_t dropouts_ = 0;     // main thread only
};

// Background thread that resolves a station's .pls, stream reads the bytes and
// decodes them into the queue. It never touches the audio device.
class RadioEngine {
public:
    RadioEngine() = default;
    ~RadioEngine();
    RadioEngine(const RadioEngine&) = delete;
    RadioEngine& operator=(const RadioEngine&) = delete;

    void Start(std::string stationName, std::vector<std::string> candidateUrls);
    void Stop();

    [[nodiscard]] PcmQueue& Queue() { return queue_; }
    [[nodiscard]] const PcmQueue& Queue() const { return queue_; }
    [[nodiscard]] RadioStatus Status() const;

private:
    void Run(std::vector<std::string> candidateUrls);
    void SetState(RadioState state, std::string text);
    void PushBlocking(const std::int16_t* pcm, std::size_t frames, int channels);

    PcmQueue queue_;
    mutable std::mutex statusMutex_;
    RadioStatus status_;
    std::atomic<bool> stopRequested_{false};
    std::thread worker_;
};
