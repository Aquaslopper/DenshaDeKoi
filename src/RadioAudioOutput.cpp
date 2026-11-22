#include "RadioAudioOutput.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>

// Match raylib/src/raudio.c's exact miniaudio feature flags so ma_context and
// ma_device layouts match the compiled miniaudio symbols linked from raylib.
#define MA_NO_JACK
#define MA_NO_WAV
#define MA_NO_FLAC
#define MA_NO_MP3
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#include "external/miniaudio.h"

namespace {
constexpr ma_uint32 kMiniaudioPeriodMs = 40;
constexpr ma_uint32 kMiniaudioPeriods = 4; // ~160ms hardware playback ring

void ApplyGain(std::int16_t* samples, std::size_t totalSamples, float gain) {
    const float g = std::clamp(gain, 0.0f, 1.0f);
    if (g <= 0.0001f) {
        std::memset(samples, 0, totalSamples * sizeof(std::int16_t));
    } else if (g < 0.999f) {
        for (std::size_t s = 0; s < totalSamples; ++s) {
            const int scaled = static_cast<int>(static_cast<float>(samples[s]) * g);
            samples[s] = static_cast<std::int16_t>(std::clamp(scaled, -32768, 32767));
        }
    }
}

const char* MapBackendLabel(ma_backend backend) {
    switch (backend) {
    case ma_backend_wasapi: return "WASAPI";
    case ma_backend_dsound: return "DS";
    case ma_backend_winmm: return "WO";
    case ma_backend_coreaudio: return "CA";
    case ma_backend_pulseaudio: return "PA";
    case ma_backend_alsa: return "ALSA";
    case ma_backend_sndio: return "SNDIO";
    case ma_backend_oss: return "OSS";
    case ma_backend_aaudio: return "AA";
    case ma_backend_opensl: return "OSL";
    case ma_backend_null: return "NULL";
    default: return "--";
    }
}
} // namespace

struct RadioAudioOutput::Impl {
    ma_context context{};
    ma_device device{};
    bool contextInit = false;
    bool deviceInit = false;
    bool active = false;

    int sampleRate = 0;
    int channels = 0;
    FillCallback callback = nullptr;
    void* user = nullptr;
    std::atomic<float> gain{0.65f};
    std::atomic<const char*> backendName{"--"};

    static void DataCallback(ma_device* pDevice, void* pOutput, const void* /*pInput*/, ma_uint32 frameCount) {
        if (!pDevice || !pOutput || frameCount == 0) return;
        auto* self = static_cast<Impl*>(pDevice->pUserData);
        if (!self || !self->callback) {
            return;
        }
        auto* dst = static_cast<std::int16_t*>(pOutput);
        self->callback(self->user, dst, static_cast<std::size_t>(frameCount));
        ApplyGain(dst, static_cast<std::size_t>(frameCount) * static_cast<std::size_t>(self->channels),
                  self->gain.load(std::memory_order_relaxed));
    }
};

RadioAudioOutput::~RadioAudioOutput() {
    Stop();
    delete impl_;
    impl_ = nullptr;
}

bool RadioAudioOutput::Start(int sampleRate, int channels, FillCallback callback, void* user) {
    Stop();
    if (sampleRate <= 0 || (channels != 1 && channels != 2) || !callback) return false;
    if (!impl_) impl_ = new Impl();

    impl_->sampleRate = sampleRate;
    impl_->channels = channels;
    impl_->callback = callback;
    impl_->user = user;
    impl_->backendName.store("--", std::memory_order_relaxed);

    if (ma_context_init(nullptr, 0, nullptr, &impl_->context) != MA_SUCCESS) {
        return false;
    }
    impl_->contextInit = true;

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_s16;
    cfg.playback.channels = static_cast<ma_uint32>(channels);
    cfg.sampleRate = static_cast<ma_uint32>(sampleRate);
    cfg.periodSizeInMilliseconds = kMiniaudioPeriodMs;
    cfg.periods = kMiniaudioPeriods;
    cfg.performanceProfile = ma_performance_profile_conservative;
    cfg.dataCallback = &Impl::DataCallback;
    cfg.pUserData = impl_;

    if (ma_device_init(&impl_->context, &cfg, &impl_->device) != MA_SUCCESS) {
        ma_context_uninit(&impl_->context);
        impl_->contextInit = false;
        return false;
    }
    impl_->deviceInit = true;

    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        ma_device_uninit(&impl_->device);
        impl_->deviceInit = false;
        ma_context_uninit(&impl_->context);
        impl_->contextInit = false;
        return false;
    }

    impl_->backendName.store(MapBackendLabel(impl_->context.backend), std::memory_order_release);
    impl_->active = true;
    return true;
}

void RadioAudioOutput::Stop() {
    if (!impl_) return;
    if (impl_->deviceInit) {
        ma_device_uninit(&impl_->device);
        impl_->deviceInit = false;
    }
    if (impl_->contextInit) {
        ma_context_uninit(&impl_->context);
        impl_->contextInit = false;
    }
    impl_->active = false;
    impl_->sampleRate = 0;
    impl_->channels = 0;
    impl_->callback = nullptr;
    impl_->user = nullptr;
    impl_->backendName.store("--", std::memory_order_relaxed);
}

void RadioAudioOutput::SetGain(float gain) {
    if (!impl_) impl_ = new Impl();
    impl_->gain.store(std::clamp(gain, 0.0f, 1.0f), std::memory_order_relaxed);
}

bool RadioAudioOutput::IsActive() const {
    return impl_ && impl_->active;
}

int RadioAudioOutput::SampleRate() const {
    return impl_ ? impl_->sampleRate : 0;
}

int RadioAudioOutput::Channels() const {
    return impl_ ? impl_->channels : 0;
}

const char* RadioAudioOutput::BackendName() const {
    return impl_ ? impl_->backendName.load(std::memory_order_acquire) : "--";
}
