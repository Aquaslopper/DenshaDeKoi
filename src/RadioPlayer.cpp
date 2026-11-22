#include "RadioPlayer.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace {
constexpr int kRaylibStreamFrames = 2048;
constexpr std::array<float, 6> kAutoBufferLadderSec = {2.0f, 4.0f, 7.0f, 10.0f, 15.0f, 20.0f};

std::atomic<RadioPlayer*> gActiveRadioPlayer{nullptr};

void RadioAudioCallback(void* bufferData, unsigned int frames) {
    if (!bufferData || frames == 0) return;
    if (auto* player = gActiveRadioPlayer.load(std::memory_order_acquire)) {
        player->FillAudioFrames(static_cast<std::int16_t*>(bufferData), static_cast<std::size_t>(frames));
    }
}
} // namespace

struct RadioPlayer::AudioImpl {
    AudioStream stream{};
};

RadioPlayer::RadioPlayer() : audio_(new AudioImpl()) {
    gActiveRadioPlayer.store(this, std::memory_order_release);
}

RadioPlayer::~RadioPlayer() {
    Stop();
    RadioPlayer* expected = this;
    gActiveRadioPlayer.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
    if (ownsAudioDevice_ && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    delete audio_;
    audio_ = nullptr;
}

float RadioPlayer::ActiveTargetSeconds() const {
    switch (bufferMode_) {
    case BufferMode::AUTO: return autoTargetSec_;
    case BufferMode::SEC_2: return 2.0f;
    case BufferMode::SEC_5: return 5.0f;
    case BufferMode::SEC_10: return 10.0f;
    case BufferMode::SEC_15: return 15.0f;
    case BufferMode::SEC_20: return 20.0f;
    }
    return autoTargetSec_;
}

void RadioPlayer::SyncTargetFrames(int sampleRate) {
    const int sr = sampleRate > 0 ? sampleRate : 44100;
    const float targetSec = ActiveTargetSeconds();
    worker_.SetTargetBufferSeconds(targetSec);
    const std::size_t frames = static_cast<std::size_t>(targetSec * static_cast<float>(sr));
    targetBufferFrames_.store(std::max<std::size_t>(kRaylibStreamFrames * 4, frames), std::memory_order_release);
}

void RadioPlayer::FillAudioFrames(std::int16_t* out, std::size_t frames) {
    const int ch = callbackChannels_.load(std::memory_order_acquire);
    const int sr = callbackSampleRate_.load(std::memory_order_acquire);
    const std::size_t byteCount = frames * static_cast<std::size_t>(std::max(1, ch)) * sizeof(std::int16_t);

    if (!callbackActive_.load(std::memory_order_acquire) || ch <= 0 || sr <= 0) {
        std::memset(out, 0, byteCount);
        return;
    }

    const std::size_t targetFrames = targetBufferFrames_.load(std::memory_order_acquire);
    if (rebuffering_.load(std::memory_order_acquire)) {
        if (worker_.Ring().AvailableFrames() < targetFrames) {
            std::memset(out, 0, byteCount);
            return;
        }
        rebuffering_.store(false, std::memory_order_release);
    }

    std::size_t remaining = 0;
    if (!worker_.Ring().PopExact(out, frames, sr, ch, remaining)) {
        std::memset(out, 0, byteCount);
        rebuffering_.store(true, std::memory_order_release);
        fadeInPending_.store(true, std::memory_order_release);
        underrunCount_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    if (fadeInPending_.exchange(false, std::memory_order_acq_rel)) {
        const std::size_t rampFrames = std::min<std::size_t>(frames, static_cast<std::size_t>(std::max(1, sr / 100)));
        for (std::size_t f = 0; f < rampFrames; ++f) {
            const float gain = static_cast<float>(f) / static_cast<float>(rampFrames);
            for (int c = 0; c < ch; ++c) {
                const std::size_t idx = f * static_cast<std::size_t>(ch) + static_cast<std::size_t>(c);
                out[idx] = static_cast<std::int16_t>(static_cast<float>(out[idx]) * gain);
            }
        }
    }
}

void RadioPlayer::ResetAudioStream() {
    callbackActive_.store(false, std::memory_order_release);
    if (streamActive_) {
        if (IsAudioStreamValid(audio_->stream)) {
            StopAudioStream(audio_->stream);
            SetAudioStreamCallback(audio_->stream, nullptr);
            UnloadAudioStream(audio_->stream);
        }
        audio_->stream = AudioStream{};
        streamActive_ = false;
        streamSampleRate_ = 0;
        streamChannels_ = 0;
        callbackSampleRate_.store(0, std::memory_order_release);
        callbackChannels_.store(0, std::memory_order_release);
    }
    rebuffering_.store(false, std::memory_order_release);
    bufferedSec_ = 0.0f;
}

void RadioPlayer::StartCurrentSelection() {
    ResetAudioStream();
    SyncTargetFrames(0);
    lastSeenUnderruns_ = underrunCount_.load(std::memory_order_relaxed);
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
    ResetAudioStream();
    worker_.Stop();
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

void RadioPlayer::SetBufferMode(BufferMode mode) {
    bufferMode_ = mode;
    if (mode == BufferMode::AUTO) {
        autoTargetSec_ = kAutoBufferLadderSec.front();
        lastSeenUnderruns_ = underrunCount_.load(std::memory_order_relaxed);
    }
    SyncTargetFrames(streamSampleRate_);
}

void RadioPlayer::CycleBufferMode() {
    switch (bufferMode_) {
    case BufferMode::AUTO: SetBufferMode(BufferMode::SEC_2); break;
    case BufferMode::SEC_2: SetBufferMode(BufferMode::SEC_5); break;
    case BufferMode::SEC_5: SetBufferMode(BufferMode::SEC_10); break;
    case BufferMode::SEC_10: SetBufferMode(BufferMode::SEC_15); break;
    case BufferMode::SEC_15: SetBufferMode(BufferMode::SEC_20); break;
    case BufferMode::SEC_20: SetBufferMode(BufferMode::AUTO); break;
    }
}

void RadioPlayer::Update() {
    if (!enabled_) return;

    int sr = 0;
    int ch = 0;
    std::size_t avail = 0;
    worker_.Ring().FormatAndAvailable(sr, ch, avail);
    if (sr <= 0 || (ch != 1 && ch != 2)) {
        bufferedSec_ = 0.0f;
        return;
    }
    bufferedSec_ = static_cast<float>(avail) / static_cast<float>(sr);

    // Automatically scale up buffer size on network underruns when in AUTO mode.
    const std::uint64_t currentUnderruns = underrunCount_.load(std::memory_order_relaxed);
    if (currentUnderruns > lastSeenUnderruns_) {
        lastSeenUnderruns_ = currentUnderruns;
        if (bufferMode_ == BufferMode::AUTO) {
            for (float step : kAutoBufferLadderSec) {
                if (step > autoTargetSec_ + 0.1f) {
                    autoTargetSec_ = step;
                    break;
                }
            }
        }
    }
    SyncTargetFrames(sr);

    if (!audioDeviceReady_) {
        if (!IsAudioDeviceReady()) {
            InitAudioDevice();
            ownsAudioDevice_ = IsAudioDeviceReady();
        }
        audioDeviceReady_ = IsAudioDeviceReady();
        if (!audioDeviceReady_) return;
    }

    const std::size_t startThreshold = targetBufferFrames_.load(std::memory_order_acquire);
    if (!streamActive_ || streamSampleRate_ != sr || streamChannels_ != ch) {
        if (avail < startThreshold) return;
        ResetAudioStream();
        SetAudioStreamBufferSizeDefault(kRaylibStreamFrames);
        audio_->stream = LoadAudioStream(static_cast<unsigned int>(sr), 16, static_cast<unsigned int>(ch));
        if (!IsAudioStreamValid(audio_->stream)) return;
        streamActive_ = true;
        streamSampleRate_ = sr;
        streamChannels_ = ch;
        callbackSampleRate_.store(sr, std::memory_order_release);
        callbackChannels_.store(ch, std::memory_order_release);
        rebuffering_.store(false, std::memory_order_release);
        fadeInPending_.store(true, std::memory_order_release);
        SetAudioStreamVolume(audio_->stream, muted_ ? 0.0f : volume_);
        SetAudioStreamCallback(audio_->stream, &RadioAudioCallback);
        callbackActive_.store(true, std::memory_order_release);
        PlayAudioStream(audio_->stream);
    }
}

RadioSnapshot RadioPlayer::Snapshot() const {
    const auto st = worker_.GetStatus();
    const auto& stations = DefaultRadioStations();
    std::string name = st.stationName;
    if (name.empty() && stationIndex_ >= 0 && static_cast<std::size_t>(stationIndex_) < stations.size()) {
        name = stations[static_cast<std::size_t>(stationIndex_)].name;
    }
    const bool isRebuf = enabled_ && streamActive_ && rebuffering_.load(std::memory_order_acquire);
    RadioState effectiveState = enabled_ ? st.state : RadioState::OFF;
    std::string effectiveStatus = enabled_ ? st.statusText : "Radio off";
    if (isRebuf) {
        effectiveState = RadioState::BUFFERING;
        effectiveStatus = "Rebuffering...";
    } else if (enabled_ && !streamActive_ && st.state == RadioState::PLAYING) {
        effectiveState = RadioState::BUFFERING;
        effectiveStatus = "Buffering...";
    }
    return {
        effectiveState,
        enabled_,
        stationIndex_,
        name,
        st.nowPlaying,
        effectiveStatus,
        customUrl_,
        volume_,
        muted_,
        bufferMode_,
        ActiveTargetSeconds(),
        bufferedSec_,
        isRebuf,
        underrunCount_.load(std::memory_order_relaxed),
    };
}
