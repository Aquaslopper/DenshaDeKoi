#include "RadioPlayer.hpp"

#include <raylib.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {
constexpr int kRaylibStreamFrames = 8192;
} // namespace

struct RadioPlayer::AudioImpl {
    AudioStream stream{};
    std::vector<std::int16_t> scratch;
};

RadioPlayer::RadioPlayer() : audio_(new AudioImpl()) {
    audio_->scratch.resize(static_cast<std::size_t>(kRaylibStreamFrames) * 2);
}

RadioPlayer::~RadioPlayer() {
    Stop();
    if (ownsAudioDevice_ && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    delete audio_;
    audio_ = nullptr;
}

void RadioPlayer::ResetAudioStream() {
    if (streamActive_) {
        if (IsAudioStreamValid(audio_->stream)) {
            StopAudioStream(audio_->stream);
            UnloadAudioStream(audio_->stream);
        }
        audio_->stream = AudioStream{};
        streamActive_ = false;
        rebuffering_ = false;
        streamSampleRate_ = 0;
        streamChannels_ = 0;
    }
}

void RadioPlayer::StartCurrentSelection() {
    ResetAudioStream();
    const auto& stations = DefaultRadioStations();
    if (stationIndex_ >= 0 && static_cast<std::size_t>(stationIndex_) < stations.size()) {
        const auto& st = stations[static_cast<std::size_t>(stationIndex_)];
        std::vector<std::string> urls;
        if (!st.plsUrl.empty()) urls.push_back(st.plsUrl);
        if (!st.fallbackUrl.empty()) urls.push_back(st.fallbackUrl);
        enabled_ = true;
        worker_.Start(st.name, std::move(urls));
        return;
    }
    if (!customUrl_.empty() && ParseUrl(customUrl_).valid) {
        enabled_ = true;
        worker_.Start("Custom Stream", {customUrl_});
        return;
    }
    enabled_ = false;
}

void RadioPlayer::SelectStation(int index) {
    const auto& stations = DefaultRadioStations();
    if (index < 0 || static_cast<std::size_t>(index) >= stations.size()) return;
    stationIndex_ = index;
    StartCurrentSelection();
}

void RadioPlayer::PlayCustomUrl(std::string_view url) {
    const ParsedUrl parsed = ParseUrl(url);
    if (!parsed.valid) return;
    std::string cleaned(url);
    while (!cleaned.empty() && (cleaned.front() == ' ' || cleaned.front() == '\t' ||
                                cleaned.front() == '\r' || cleaned.front() == '\n')) {
        cleaned.erase(cleaned.begin());
    }
    while (!cleaned.empty() && (cleaned.back() == ' ' || cleaned.back() == '\t' ||
                                cleaned.back() == '\r' || cleaned.back() == '\n')) {
        cleaned.pop_back();
    }
    customUrl_ = cleaned;
    stationIndex_ = -1;
    StartCurrentSelection();
}

void RadioPlayer::ToggleEnabled() {
    if (enabled_) Stop();
    else StartCurrentSelection();
}

void RadioPlayer::Stop() {
    enabled_ = false;
    worker_.Stop();
    ResetAudioStream();
}

void RadioPlayer::SetVolume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
    if (streamActive_ && IsAudioStreamValid(audio_->stream)) {
        SetAudioStreamVolume(audio_->stream, muted_ ? 0.0f : volume_);
    }
}

void RadioPlayer::ToggleMute() {
    muted_ = !muted_;
    if (streamActive_ && IsAudioStreamValid(audio_->stream)) {
        SetAudioStreamVolume(audio_->stream, muted_ ? 0.0f : volume_);
    }
}

void RadioPlayer::Update() {
    if (!enabled_) return;

    int sr = 0;
    int ch = 0;
    std::size_t avail = 0;
    worker_.Ring().FormatAndAvailable(sr, ch, avail);
    if (sr <= 0 || (ch != 1 && ch != 2)) return;

    if (!audioDeviceReady_) {
        if (!IsAudioDeviceReady()) {
            InitAudioDevice();
            ownsAudioDevice_ = IsAudioDeviceReady();
        }
        audioDeviceReady_ = IsAudioDeviceReady();
        if (!audioDeviceReady_) return;
    }

    const std::size_t startThreshold = std::max<std::size_t>(
        static_cast<std::size_t>(kRaylibStreamFrames) * 3,
        static_cast<std::size_t>(sr) * 2);

    if (!streamActive_ || streamSampleRate_ != sr || streamChannels_ != ch) {
        if (avail < startThreshold) return;
        ResetAudioStream();
        SetAudioStreamBufferSizeDefault(kRaylibStreamFrames);
        audio_->stream = LoadAudioStream(static_cast<unsigned int>(sr), 16, static_cast<unsigned int>(ch));
        if (!IsAudioStreamValid(audio_->stream)) return;
        streamActive_ = true;
        rebuffering_ = false;
        streamSampleRate_ = sr;
        streamChannels_ = ch;
        SetAudioStreamVolume(audio_->stream, muted_ ? 0.0f : volume_);
        PlayAudioStream(audio_->stream);
    }

    if (rebuffering_) {
        const std::size_t resumeThreshold = std::max<std::size_t>(
            static_cast<std::size_t>(kRaylibStreamFrames) * 2,
            static_cast<std::size_t>(sr) * 3 / 2);
        if (avail < resumeThreshold) return;
        while (IsAudioStreamProcessed(audio_->stream)) {
            int popSr = 0;
            int popCh = 0;
            if (worker_.Ring().Pop(audio_->scratch.data(), kRaylibStreamFrames, popSr, popCh) != kRaylibStreamFrames) {
                return;
            }
            if (popSr != streamSampleRate_ || popCh != streamChannels_) {
                ResetAudioStream();
                return;
            }
            UpdateAudioStream(audio_->stream, audio_->scratch.data(), kRaylibStreamFrames);
        }
        ResumeAudioStream(audio_->stream);
        rebuffering_ = false;
        return;
    }

    while (IsAudioStreamProcessed(audio_->stream)) {
        int popSr = 0;
        int popCh = 0;
        if (worker_.Ring().Pop(audio_->scratch.data(), kRaylibStreamFrames, popSr, popCh) != kRaylibStreamFrames) {
            PauseAudioStream(audio_->stream);
            rebuffering_ = true;
            break;
        }
        if (popSr != streamSampleRate_ || popCh != streamChannels_) {
            ResetAudioStream();
            break;
        }
        UpdateAudioStream(audio_->stream, audio_->scratch.data(), kRaylibStreamFrames);
    }
}

RadioSnapshot RadioPlayer::Snapshot() const {
    const auto st = worker_.GetStatus();
    const auto& stations = DefaultRadioStations();
    std::string name = st.stationName;
    if (name.empty() && stationIndex_ >= 0 && static_cast<std::size_t>(stationIndex_) < stations.size()) {
        name = stations[static_cast<std::size_t>(stationIndex_)].name;
    }
    return {
        enabled_ ? st.state : RadioState::OFF,
        enabled_,
        stationIndex_,
        name,
        st.nowPlaying,
        enabled_ ? st.statusText : "Radio off",
        customUrl_,
        volume_,
        muted_,
    };
}
