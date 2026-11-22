#include "RadioPlayer.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace {
constexpr std::size_t kMinTargetFrames = 8192;

void FillRadioAudioTrampoline(void* user, std::int16_t* out, std::size_t frames) {
    if (!user || !out || frames == 0) return;
    static_cast<RadioPlayer*>(user)->FillAudioFrames(out, frames);
}
} // namespace

RadioPlayer::RadioPlayer() {
    output_.SetGain(muted_ ? 0.0f : volume_);
}

RadioPlayer::~RadioPlayer() {
    Stop();
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
    targetBufferFrames_.store(std::max(kMinTargetFrames, frames), std::memory_order_release);
}

void RadioPlayer::FillAudioFrames(std::int16_t* out, std::size_t frames) {
    const int ch = callbackChannels_.load(std::memory_order_acquire);
    const int sr = callbackSampleRate_.load(std::memory_order_acquire);
    const std::size_t channels = static_cast<std::size_t>(std::max(1, ch));
    const std::size_t byteCount = frames * channels * sizeof(std::int16_t);

    if (!callbackActive_.load(std::memory_order_acquire) || ch <= 0 || sr <= 0) {
        std::memset(out, 0, byteCount);
        return;
    }

    const std::size_t targetFrames = targetBufferFrames_.load(std::memory_order_acquire);
    const float ratio = prebufferRatio_.load(std::memory_order_acquire);
    const std::size_t resumeThreshold = RadioResumeThresholdFrames(targetFrames, sr, ratio);
    if (rebuffering_.load(std::memory_order_acquire)) {
        if (worker_.Ring().AvailableFrames() < resumeThreshold) {
            std::memset(out, 0, byteCount);
            return;
        }
        rebuffering_.store(false, std::memory_order_release);
    }

    // Fill as much of the period as is buffered and pad only the missing tail.
    // Demanding a whole period used to zero the entire period on any shortfall,
    // which turned a single lost 40 ms period into a multi-second dropout.
    const std::size_t got = worker_.Ring().PopAvailable(out, frames, sr, ch);
    if (got < frames) {
        std::memset(out + got * channels, 0, (frames - got) * channels * sizeof(std::int16_t));
        // An audible hole: count it and nudge the cushion, but keep playing.
        underrunCount_.fetch_add(1, std::memory_order_relaxed);
        fadeInPending_.store(true, std::memory_order_release);
    }
    if (got == 0) {
        // Genuinely starved: from here on we wait for the resume threshold again.
        rebuffering_.store(true, std::memory_order_release);
        return;
    }

    if (fadeInPending_.exchange(false, std::memory_order_acq_rel)) {
        const std::size_t rampFrames = std::min<std::size_t>(got, static_cast<std::size_t>(std::max(1, sr / 100)));
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
    output_.Stop();
    callbackSampleRate_.store(0, std::memory_order_release);
    callbackChannels_.store(0, std::memory_order_release);
    rebuffering_.store(false, std::memory_order_release);
    bufferedSec_ = 0.0f;
}

void RadioPlayer::StartCurrentSelection() {
    ResetAudioStream();
    SyncTargetFrames(0);
    lastSeenUnderruns_ = underrunCount_.load(std::memory_order_relaxed);
    lastUnderrunTime_ = std::chrono::steady_clock::now();
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
    output_.SetGain(muted_ ? 0.0f : volume_);
}

void RadioPlayer::ToggleMute() {
    muted_ = !muted_;
    output_.SetGain(muted_ ? 0.0f : volume_);
}

void RadioPlayer::SetBufferMode(BufferMode mode) {
    bufferMode_ = mode;
    if (mode == BufferMode::AUTO) {
        autoTargetSec_ = kRadioAutoBufferLadderSec.front();
        lastSeenUnderruns_ = underrunCount_.load(std::memory_order_relaxed);
        lastUnderrunTime_ = std::chrono::steady_clock::now();
    }
    SyncTargetFrames(output_.SampleRate());
}

void RadioPlayer::SetPrebufferRatio(float ratio) {
    prebufferRatio_.store(std::clamp(ratio, 0.0f, 1.0f), std::memory_order_release);
}

void RadioPlayer::ApplyPrebufferThreshold() {
    if (!enabled_ || !output_.IsActive()) return;
    const std::size_t targetFrames = targetBufferFrames_.load(std::memory_order_acquire);
    const float ratio = prebufferRatio_.load(std::memory_order_acquire);
    const std::size_t threshold = RadioStartThresholdFrames(targetFrames, output_.SampleRate(), ratio);
    if (worker_.Ring().AvailableFrames() < threshold) {
        fadeInPending_.store(true, std::memory_order_release);
        rebuffering_.store(true, std::memory_order_release);
    }
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

    // AUTO: grow the buffer on an underrun, but decay back down after a stable
    // run. Without the decay the target only ever grew (4 -> 6 -> 8 -> 10 s),
    // which made every following dropout cost even more seconds of silence.
    const auto now = std::chrono::steady_clock::now();
    const std::uint64_t currentUnderruns = underrunCount_.load(std::memory_order_relaxed);
    const bool hadUnderrun = currentUnderruns > lastSeenUnderruns_;
    if (hadUnderrun) {
        lastSeenUnderruns_ = currentUnderruns;
        lastUnderrunTime_ = now;
    }
    if (bufferMode_ == BufferMode::AUTO) {
        const double stableSeconds = std::chrono::duration<double>(now - lastUnderrunTime_).count();
        autoTargetSec_ = RadioNextAutoBufferTarget(autoTargetSec_, hadUnderrun, stableSeconds);
    }
    SyncTargetFrames(sr);

    const std::size_t startThreshold = RadioStartThresholdFrames(
        targetBufferFrames_.load(std::memory_order_acquire), sr,
        prebufferRatio_.load(std::memory_order_acquire));
    if (!output_.IsActive() || output_.SampleRate() != sr || output_.Channels() != ch) {
        if (avail < startThreshold) return;
        ResetAudioStream();
        callbackSampleRate_.store(sr, std::memory_order_release);
        callbackChannels_.store(ch, std::memory_order_release);
        rebuffering_.store(false, std::memory_order_release);
        fadeInPending_.store(true, std::memory_order_release);
        output_.SetGain(muted_ ? 0.0f : volume_);
        callbackActive_.store(true, std::memory_order_release);
        if (!output_.Start(sr, ch, &FillRadioAudioTrampoline, this)) {
            callbackActive_.store(false, std::memory_order_release);
        }
    }
}

RadioSnapshot RadioPlayer::Snapshot() const {
    const auto st = worker_.GetStatus();
    const auto& stations = DefaultRadioStations();
    std::string name = st.stationName;
    if (name.empty() && stationIndex_ >= 0 && static_cast<std::size_t>(stationIndex_) < stations.size()) {
        name = stations[static_cast<std::size_t>(stationIndex_)].name;
    }
    const bool streamActive = output_.IsActive();
    const bool isRebuf = enabled_ && streamActive && rebuffering_.load(std::memory_order_acquire);
    RadioState effectiveState = enabled_ ? st.state : RadioState::OFF;
    std::string effectiveStatus = enabled_ ? st.statusText : "Radio off";
    if (isRebuf) {
        effectiveState = RadioState::BUFFERING;
        effectiveStatus = "Rebuffering...";
    } else if (enabled_ && streamActive && st.state == RadioState::BUFFERING) {
        effectiveState = RadioState::PLAYING;
        effectiveStatus = "LIVE";
    } else if (enabled_ && !streamActive && st.state == RadioState::PLAYING) {
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
        prebufferRatio_.load(std::memory_order_acquire),
        isRebuf,
        underrunCount_.load(std::memory_order_relaxed),
        output_.BackendName(),
        st.resyncs,
        st.reconnects,
    };
}
