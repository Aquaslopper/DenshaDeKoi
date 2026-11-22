#include "RadioAudioOutput.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

namespace {
constexpr std::size_t kWaveBufferCount = 8;
constexpr std::size_t kWaveBufferFrames = 2048; // ~371 ms total queued at 44.1 kHz
} // namespace

struct RadioAudioOutput::Impl {
    HWAVEOUT hWaveOut = nullptr;
    HANDLE event = nullptr;
    std::thread feeder;
    std::atomic<bool> stopRequested{false};
    std::atomic<float> gain{0.65f};
    bool active = false;
    int sampleRate = 0;
    int channels = 0;
    FillCallback callback = nullptr;
    void* user = nullptr;

    std::vector<WAVEHDR> headers;
    std::vector<std::vector<std::int16_t>> buffers;
    std::vector<bool> prepared;
    std::vector<bool> inFlight;

    void RunFeeder() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        const std::size_t ch = static_cast<std::size_t>(channels);
        const std::size_t totalSamples = kWaveBufferFrames * ch;

        while (!stopRequested.load(std::memory_order_acquire)) {
            bool submittedAny = false;
            for (std::size_t i = 0; i < kWaveBufferCount; ++i) {
                if (stopRequested.load(std::memory_order_acquire)) break;
                if (!inFlight[i] || (headers[i].dwFlags & WHDR_DONE) != 0) {
                    std::int16_t* dst = buffers[i].data();
                    callback(user, dst, kWaveBufferFrames);

                    const float g = std::clamp(gain.load(std::memory_order_relaxed), 0.0f, 1.0f);
                    if (g <= 0.0001f) {
                        std::memset(dst, 0, totalSamples * sizeof(std::int16_t));
                    } else if (g < 0.999f) {
                        for (std::size_t s = 0; s < totalSamples; ++s) {
                            const int scaled = static_cast<int>(static_cast<float>(dst[s]) * g);
                            dst[s] = static_cast<std::int16_t>(std::clamp(scaled, -32768, 32767));
                        }
                    }

                    headers[i].dwBufferLength = static_cast<DWORD>(totalSamples * sizeof(std::int16_t));
                    inFlight[i] = (waveOutWrite(hWaveOut, &headers[i], sizeof(WAVEHDR)) == MMSYSERR_NOERROR);
                    if (inFlight[i]) {
                        submittedAny = true;
                    }
                }
            }
            if (!submittedAny && !stopRequested.load(std::memory_order_acquire)) {
                WaitForSingleObject(event, 30);
            }
        }
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

    impl_->stopRequested.store(false, std::memory_order_relaxed);
    impl_->sampleRate = sampleRate;
    impl_->channels = channels;
    impl_->callback = callback;
    impl_->user = user;

    impl_->event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!impl_->event) {
        Stop();
        return false;
    }

    WAVEFORMATEX wfx{};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = static_cast<WORD>(channels);
    wfx.nSamplesPerSec = static_cast<DWORD>(sampleRate);
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = static_cast<WORD>(channels * sizeof(std::int16_t));
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (waveOutOpen(&impl_->hWaveOut, WAVE_MAPPER, &wfx,
                    reinterpret_cast<DWORD_PTR>(impl_->event), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        Stop();
        return false;
    }

    impl_->headers.assign(kWaveBufferCount, WAVEHDR{});
    impl_->buffers.assign(kWaveBufferCount, std::vector<std::int16_t>(kWaveBufferFrames * static_cast<std::size_t>(channels), 0));
    impl_->prepared.assign(kWaveBufferCount, false);
    impl_->inFlight.assign(kWaveBufferCount, false);

    for (std::size_t i = 0; i < kWaveBufferCount; ++i) {
        impl_->headers[i].lpData = reinterpret_cast<LPSTR>(impl_->buffers[i].data());
        impl_->headers[i].dwBufferLength = static_cast<DWORD>(impl_->buffers[i].size() * sizeof(std::int16_t));
        if (waveOutPrepareHeader(impl_->hWaveOut, &impl_->headers[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            Stop();
            return false;
        }
        impl_->prepared[i] = true;
    }

    impl_->active = true;
    impl_->feeder = std::thread(&Impl::RunFeeder, impl_);
    return true;
}

void RadioAudioOutput::Stop() {
    if (!impl_) return;
    impl_->stopRequested.store(true, std::memory_order_release);
    if (impl_->event) SetEvent(impl_->event);
    if (impl_->feeder.joinable()) impl_->feeder.join();
    if (impl_->hWaveOut) {
        waveOutReset(impl_->hWaveOut);
        for (std::size_t i = 0; i < impl_->headers.size(); ++i) {
            if (i < impl_->prepared.size() && impl_->prepared[i]) {
                waveOutUnprepareHeader(impl_->hWaveOut, &impl_->headers[i], sizeof(WAVEHDR));
            }
        }
        waveOutClose(impl_->hWaveOut);
        impl_->hWaveOut = nullptr;
    }
    if (impl_->event) {
        CloseHandle(impl_->event);
        impl_->event = nullptr;
    }
    impl_->headers.clear();
    impl_->buffers.clear();
    impl_->prepared.clear();
    impl_->inFlight.clear();
    impl_->active = false;
    impl_->sampleRate = 0;
    impl_->channels = 0;
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

#else

#include <raylib.h>

namespace {
constexpr int kRaylibStreamFrames = 2048;

struct PosixCallbackTarget {
    RadioAudioOutput::FillCallback cb = nullptr;
    void* user = nullptr;
};
std::atomic<PosixCallbackTarget*> gActiveTarget{nullptr};

void PosixRaylibCallback(void* bufferData, unsigned int frames) {
    if (!bufferData || frames == 0) return;
    if (auto* target = gActiveTarget.load(std::memory_order_acquire)) {
        if (target->cb) {
            target->cb(target->user, static_cast<std::int16_t*>(bufferData), static_cast<std::size_t>(frames));
        }
    }
}
} // namespace

struct RadioAudioOutput::Impl {
    AudioStream stream{};
    PosixCallbackTarget target{};
    bool ownsAudioDevice = false;
    bool audioDeviceReady = false;
    bool active = false;
    int sampleRate = 0;
    int channels = 0;
    float gain = 0.65f;
};

RadioAudioOutput::~RadioAudioOutput() {
    Stop();
    if (impl_ && impl_->ownsAudioDevice && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    delete impl_;
    impl_ = nullptr;
}

bool RadioAudioOutput::Start(int sampleRate, int channels, FillCallback callback, void* user) {
    Stop();
    if (sampleRate <= 0 || (channels != 1 && channels != 2) || !callback) return false;
    if (!impl_) impl_ = new Impl();

    if (!impl_->audioDeviceReady) {
        if (!IsAudioDeviceReady()) {
            InitAudioDevice();
            impl_->ownsAudioDevice = IsAudioDeviceReady();
        }
        impl_->audioDeviceReady = IsAudioDeviceReady();
        if (!impl_->audioDeviceReady) return false;
    }

    SetAudioStreamBufferSizeDefault(kRaylibStreamFrames);
    impl_->stream = LoadAudioStream(static_cast<unsigned int>(sampleRate), 16, static_cast<unsigned int>(channels));
    if (!IsAudioStreamValid(impl_->stream)) return false;

    impl_->sampleRate = sampleRate;
    impl_->channels = channels;
    impl_->target.cb = callback;
    impl_->target.user = user;
    gActiveTarget.store(&impl_->target, std::memory_order_release);
    impl_->active = true;
    SetAudioStreamVolume(impl_->stream, impl_->gain);
    SetAudioStreamCallback(impl_->stream, &PosixRaylibCallback);
    PlayAudioStream(impl_->stream);
    return true;
}

void RadioAudioOutput::Stop() {
    if (!impl_ || !impl_->active) return;
    PosixCallbackTarget* expected = &impl_->target;
    gActiveTarget.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
    if (IsAudioStreamValid(impl_->stream)) {
        StopAudioStream(impl_->stream);
        SetAudioStreamCallback(impl_->stream, nullptr);
        UnloadAudioStream(impl_->stream);
    }
    impl_->stream = AudioStream{};
    impl_->active = false;
    impl_->sampleRate = 0;
    impl_->channels = 0;
}

void RadioAudioOutput::SetGain(float gain) {
    if (!impl_) impl_ = new Impl();
    impl_->gain = std::clamp(gain, 0.0f, 1.0f);
    if (impl_->active && IsAudioStreamValid(impl_->stream)) {
        SetAudioStreamVolume(impl_->stream, impl_->gain);
    }
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

#endif
