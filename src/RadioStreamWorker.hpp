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

enum class RadioState {
    OFF,
    CONNECTING,
    BUFFERING,
    PLAYING,
    FAILED,
};

// Decodes HTTP/1.1 Transfer-Encoding: chunked framing when an audio server sends chunked bodies.
class HttpChunkedDecoder {
public:
    void Reset(bool enabled);
    void Feed(const std::uint8_t* data, std::size_t size,
              const std::function<void(const std::uint8_t*, std::size_t)>& onPayload);

private:
    enum class Phase { SIZE_LINE, DATA, DATA_CR, DATA_LF, TRAILER };
    bool enabled_ = false;
    Phase phase_ = Phase::SIZE_LINE;
    std::size_t chunkRemaining_ = 0;
    std::string sizeLine_;
};

// Demuxes SHOUTcast/Icecast inline metadata blocks (icy-metaint) from an audio byte stream.
class IcyDemuxer {
public:
    explicit IcyDemuxer(std::size_t metaInterval = 0)
        : metaInterval_(metaInterval), audioRemaining_(metaInterval) {}

    void Reset(std::size_t metaInterval);
    void Feed(const std::uint8_t* data, std::size_t size,
              const std::function<void(const std::uint8_t*, std::size_t)>& onAudio,
              const std::function<void(const std::string&)>& onMeta);

private:
    enum class Phase { AUDIO, META_LEN, META_BODY };
    std::size_t metaInterval_ = 0;
    Phase phase_ = Phase::AUDIO;
    std::size_t audioRemaining_ = 0;
    std::size_t metaRemaining_ = 0;
    std::string metaBuffer_;
};

// Streaming MP3 frame decoder using dr_mp3's low-level push API (handles mid-stream sync).
class Mp3StreamDecoder {
public:
    Mp3StreamDecoder();
    ~Mp3StreamDecoder();
    Mp3StreamDecoder(const Mp3StreamDecoder&) = delete;
    Mp3StreamDecoder& operator=(const Mp3StreamDecoder&) = delete;

    void Reset();
    void Feed(const std::uint8_t* data, std::size_t size, bool flush,
              const std::function<void(const std::int16_t*, std::size_t frames, int sampleRate, int channels)>& onPcm);
    [[nodiscard]] std::size_t DecodedFrames() const { return decodedFrames_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::vector<std::uint8_t> inBuf_;
    std::size_t decodedFrames_ = 0;
};

// Lock-free Single-Producer Single-Consumer PCM ring buffer (30s capacity at 44.1 kHz).
// Audio callback only reads/advances head_; worker thread only writes/advances tail_.
class PcmRingBuffer {
public:
    explicit PcmRingBuffer(std::size_t maxFrames = 1323000);

    void Clear(int sampleRate = 0, int channels = 0);
    std::size_t Push(const std::int16_t* interleaved, std::size_t frames, int sampleRate, int channels);
    std::size_t Pop(std::int16_t* out, std::size_t exactFrames, int& outSampleRate, int& outChannels);
    bool PopExact(std::int16_t* out, std::size_t exactFrames, int expectedSampleRate, int expectedChannels,
                  std::size_t& outRemaining);
    void FormatAndAvailable(int& outSampleRate, int& outChannels, std::size_t& outFrames) const;
    [[nodiscard]] std::size_t AvailableFrames() const;
    [[nodiscard]] std::size_t CapacityFrames() const { return maxFrames_; }

private:
    std::vector<std::int16_t> samples_;
    std::size_t maxFrames_ = 1323000;
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
    std::atomic<int> sampleRate_{0};
    std::atomic<int> channels_{0};
};

// Background stream fetcher/demuxer/decoder for HTTP/HTTPS .pls and MP3 radio streams.
class RadioStreamWorker {
public:
    RadioStreamWorker() = default;
    ~RadioStreamWorker();
    RadioStreamWorker(const RadioStreamWorker&) = delete;
    RadioStreamWorker& operator=(const RadioStreamWorker&) = delete;

    void Start(std::string stationName, std::vector<std::string> candidateUrls);
    void Stop();
    void SetTargetBufferSeconds(float seconds);

    PcmRingBuffer& Ring() { return ring_; }

    struct Status {
        RadioState state = RadioState::OFF;
        std::string stationName;
        std::string nowPlaying;
        std::string statusText;
    };
    Status GetStatus() const;

private:
    void Run(std::string stationName, std::vector<std::string> candidateUrls);
    void SetStatus(RadioState state, const std::string& statusText);
    void SetNowPlaying(const std::string& title);
    void SetStationName(const std::string& name);

    PcmRingBuffer ring_;
    std::atomic<float> targetBufferSeconds_{2.0f};
    mutable std::mutex statusMutex_;
    Status status_;
    std::atomic<bool> stopRequested_{false};
    std::thread worker_;
};
