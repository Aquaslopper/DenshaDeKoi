#include "SimpleRadio.hpp"

#include <algorithm>
#include <cstdio>

SimpleRadio::~SimpleRadio() {
    engine_.Stop();
    ReleaseStream();
}

void SimpleRadio::ReleaseStream() {
    if (streamReady_) {
        UnloadAudioStream(stream_);
        stream_ = AudioStream{};
        streamReady_ = false;
    }
    streamSampleRate_ = 0;
    streamChannels_ = 0;
}

bool SimpleRadio::EnsureStream(int sampleRate, int channels) {
    if (streamReady_ && streamSampleRate_ == sampleRate && streamChannels_ == channels) return true;
    ReleaseStream();
    if (!IsAudioDeviceReady()) {
        deviceError_ = "No audio device";
        std::fprintf(stderr, "[radio] no audio device\n");
        return false;
    }
    SetAudioStreamBufferSizeDefault(BlockFrames);
    stream_ = LoadAudioStream(static_cast<unsigned int>(sampleRate), 16,
                              static_cast<unsigned int>(channels));
    if (!IsAudioStreamValid(stream_)) {
        deviceError_ = "Audio stream failed";
        std::fprintf(stderr, "[radio] LoadAudioStream(%d, 16, %d) failed\n", sampleRate, channels);
        return false;
    }
    deviceError_.clear();
    streamReady_ = true;
    streamSampleRate_ = sampleRate;
    streamChannels_ = channels;
    ApplyVolume();
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

    if (!IsAudioStreamProcessed(stream_)) return;

    const std::size_t samples = static_cast<std::size_t>(BlockFrames) * static_cast<std::size_t>(channels);
    block_.assign(samples, 0); // silence is the default for the unfilled tail
    // A short read simply leaves the rest of the block silent.
    engine_.Queue().Pop(block_.data(), static_cast<std::size_t>(BlockFrames));
    UpdateAudioStream(stream_, block_.data(), BlockFrames);
}

RadioStatus SimpleRadio::Status() const {
    RadioStatus status = engine_.Status();
    status.enabled = enabled_;
    status.stationIndex = stationIndex_;
    status.volume = volume_;
    status.muted = muted_;
    if (!enabled_) {
        status.state = RadioState::OFF;
        status.statusText = "Radio off";
    } else if (!deviceError_.empty()) {
        status.state = RadioState::FAILED;
        status.statusText = deviceError_;
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
    enabled_ = true;
    engine_.Start(station.name, std::move(urls));
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
