#include "SimpleRadio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace {
// Raylib's AudioCallback carries no user data, and the game owns exactly one
// radio, so the stream callback reaches its owner through this pointer. It is
// set before the stream is played and cleared before the stream is unloaded.
SimpleRadio* g_callbackOwner = nullptr;

void Silence(std::int16_t* out, std::size_t samples) {
    std::memset(out, 0, samples * sizeof(std::int16_t));
}

std::string FormatTenths(float seconds) {
    const int tenths = std::max(0, static_cast<int>(std::lround(seconds * 10.0f)));
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "s";
}
} // namespace

SimpleRadio::~SimpleRadio() {
    engine_.Stop();
    ReleaseStream();
}

void SimpleRadio::StreamCallback(void* buffer, unsigned int frames) {
    auto* out = static_cast<std::int16_t*>(buffer);
    SimpleRadio* radio = g_callbackOwner;
    if (!radio) {
        Silence(out, frames);
        return;
    }
    radio->FillAudio(out, frames);
}

void SimpleRadio::FillAudio(std::int16_t* out, unsigned int frames) {
    const int channels = streamChannels_.load(std::memory_order_relaxed);
    if (channels <= 0) {
        Silence(out, frames);
        return;
    }
    // Hand the gate room for exactly `frames` frames; it returns how many are
    // real audio, the remainder is silence (no data yet or a dropout).
    const std::size_t got = gate_.Fill(engine_.Queue(), out, frames, channels);
    if (got < frames) {
        Silence(out + got * static_cast<std::size_t>(channels),
                (frames - got) * static_cast<std::size_t>(channels));
    }
}

void SimpleRadio::ReleaseStream() {
    if (streamReady_) {
        // Detach the callback before freeing the buffer: UnloadAudioStream frees
        // it without taking Raylib's audio lock.
        StopAudioStream(stream_);
        SetAudioStreamCallback(stream_, nullptr);
        g_callbackOwner = nullptr;
        UnloadAudioStream(stream_);
        stream_ = AudioStream{};
        streamReady_ = false;
    }
    streamChannels_.store(0, std::memory_order_relaxed);
    streamSampleRate_ = 0;
}

bool SimpleRadio::EnsureStream(int sampleRate, int channels) {
    if (streamReady_ && streamSampleRate_ == sampleRate &&
        streamChannels_.load(std::memory_order_relaxed) == channels) {
        return true;
    }
    ReleaseStream();
    if (!IsAudioDeviceReady()) {
        deviceError_ = "No audio device";
        std::fprintf(stderr, "[radio] no audio device\n");
        return false;
    }
    stream_ = LoadAudioStream(static_cast<unsigned int>(sampleRate), 16,
                              static_cast<unsigned int>(channels));
    if (!IsAudioStreamValid(stream_)) {
        deviceError_ = "Audio stream failed";
        std::fprintf(stderr, "[radio] LoadAudioStream(%d, 16, %d) failed\n", sampleRate, channels);
        return false;
    }
    deviceError_.clear();
    streamSampleRate_ = sampleRate;
    streamChannels_.store(channels, std::memory_order_relaxed);
    streamReady_ = true;
    ApplyVolume();
    g_callbackOwner = this;
    SetAudioStreamCallback(stream_, &SimpleRadio::StreamCallback);
    PlayAudioStream(stream_);
    std::printf("[radio] output %d Hz %d ch\n", sampleRate, channels);
    std::fflush(stdout);
    return true;
}

void SimpleRadio::ApplyVolume() {
    if (streamReady_) SetAudioStreamVolume(stream_, muted_ ? 0.0f : volume_);
}

void SimpleRadio::Update() {
    if (!enabled_) return;

    int sampleRate = 0;
    int channels = 0;
    if (!engine_.Queue().Format(sampleRate, channels)) return; // nothing decoded yet
    if (!EnsureStream(sampleRate, channels)) return;

    // Starts playback once the cushion is buffered and re-arms it after a dropout.
    gate_.Poll(engine_.Queue());
}

RadioStatus SimpleRadio::Status() const {
    RadioStatus status = engine_.Status();
    status.enabled = enabled_;
    status.stationIndex = stationIndex_;
    status.volume = volume_;
    status.muted = muted_;

    int sampleRate = 0;
    int channels = 0;
    if (engine_.Queue().Format(sampleRate, channels) && sampleRate > 0) {
        status.bufferedSec = static_cast<float>(engine_.Queue().AvailableFrames()) /
                             static_cast<float>(sampleRate);
    }

    if (!enabled_) {
        status.state = RadioState::OFF;
        status.statusText = "Radio off";
    } else if (!deviceError_.empty()) {
        status.state = RadioState::FAILED;
        status.statusText = deviceError_;
    } else if (status.state != RadioState::FAILED) {
        if (gate_.Playing()) {
            status.state = RadioState::PLAYING;
            status.statusText = "Live";
        } else if (status.state != RadioState::CONNECTING) {
            status.state = RadioState::BUFFERING;
            status.statusText = "Buffering " + FormatTenths(status.bufferedSec);
        }
    }
    return status;
}

void SimpleRadio::StartCurrentStation() {
    ReleaseStream();
    deviceError_.clear();
    const auto& stations = DefaultRadioStations();
    if (stationIndex_ < 0 || static_cast<std::size_t>(stationIndex_) >= stations.size()) {
        enabled_ = false;
        return;
    }
    const auto& station = stations[static_cast<std::size_t>(stationIndex_)];
    std::vector<std::string> urls;
    if (!station.plsUrl.empty()) urls.push_back(station.plsUrl);
    if (!station.fallbackUrl.empty()) urls.push_back(station.fallbackUrl);
    PlayUrl(station.name, std::move(urls));
}

void SimpleRadio::PlayUrl(std::string displayName, std::vector<std::string> urls) {
    if (urls.empty()) return;
    ReleaseStream();
    gate_.Reset();
    deviceError_.clear();
    enabled_ = true;
    engine_.Start(std::move(displayName), std::move(urls));
}

void SimpleRadio::SelectStation(int index) {
    const auto& stations = DefaultRadioStations();
    if (index < 0 || static_cast<std::size_t>(index) >= stations.size()) return;
    stationIndex_ = index;
    StartCurrentStation(); // clicking a station switches the radio on
}

void SimpleRadio::ToggleEnabled() {
    if (enabled_) {
        enabled_ = false;
        engine_.Stop();
        ReleaseStream();
        gate_.Reset();
        return;
    }
    StartCurrentStation();
}

void SimpleRadio::SetVolume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
    ApplyVolume();
}

void SimpleRadio::ToggleMute() {
    muted_ = !muted_;
    ApplyVolume();
}
