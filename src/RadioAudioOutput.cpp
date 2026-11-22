#include "RadioAudioOutput.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
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
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmsystem.h>

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif
#ifndef AUDCLNT_E_DEVICE_INVALIDATED
#define AUDCLNT_E_DEVICE_INVALIDATED static_cast<HRESULT>(0x88890004L)
#endif

namespace {
constexpr REFERENCE_TIME kWasapiBufferDuration = 15000000; // 1.5 seconds in 100-ns units
constexpr UINT32 kWasapiWriteChunkFrames = 2048;
constexpr std::size_t kWaveBufferCount = 16;
constexpr std::size_t kWaveBufferFrames = 4096; // ~1.48 seconds queued at 44.1 kHz

const CLSID kClsidMMDeviceEnumerator = {
    0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}
};
const IID kIidIMMDeviceEnumerator = {
    0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}
};
const IID kIidIAudioClient = {
    0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}
};
const IID kIidIAudioRenderClient = {
    0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2}
};

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
} // namespace

struct RadioAudioOutput::Impl {
    std::thread feeder;
    std::atomic<bool> stopRequested{false};
    std::atomic<float> gain{0.65f};
    std::mutex initMutex;
    std::condition_variable initCv;
    bool initDone = false;
    bool initOk = false;

    bool active = false;
    int sampleRate = 0;
    int channels = 0;
    FillCallback callback = nullptr;
    void* user = nullptr;

    void SignalInit(bool ok) {
        {
            std::lock_guard<std::mutex> lock(initMutex);
            initOk = ok;
            initDone = true;
        }
        initCv.notify_one();
    }

    bool RunWasapi(bool& outSignaledInit) {
        const HRESULT comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool shouldUninit = (comHr == S_OK || comHr == S_FALSE);
        const auto cleanupCom = [&]() {
            if (shouldUninit) CoUninitialize();
        };
        if (FAILED(comHr) && comHr != RPC_E_CHANGED_MODE) {
            cleanupCom();
            return false;
        }

        WAVEFORMATEX wfx{};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = static_cast<WORD>(channels);
        wfx.nSamplesPerSec = static_cast<DWORD>(sampleRate);
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = static_cast<WORD>(channels * sizeof(std::int16_t));
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

        const auto openEndpoint = [&](IMMDeviceEnumerator* enumerator,
                                      IAudioClient*& outClient,
                                      IAudioRenderClient*& outRender,
                                      UINT32& outBufFrames) -> bool {
            IMMDevice* device = nullptr;
            if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device)) || !device) {
                return false;
            }
            IAudioClient* client = nullptr;
            HRESULT hr = device->Activate(kIidIAudioClient, CLSCTX_ALL, nullptr,
                                          reinterpret_cast<void**>(&client));
            device->Release();
            if (FAILED(hr) || !client) return false;

            const DWORD flags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
            hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, kWasapiBufferDuration, 0, &wfx, nullptr);
            if (FAILED(hr)) {
                client->Release();
                return false;
            }

            UINT32 bufFrames = 0;
            if (FAILED(client->GetBufferSize(&bufFrames)) || bufFrames == 0) {
                client->Release();
                return false;
            }

            IAudioRenderClient* render = nullptr;
            hr = client->GetService(kIidIAudioRenderClient, reinterpret_cast<void**>(&render));
            if (FAILED(hr) || !render) {
                client->Release();
                return false;
            }

            outClient = client;
            outRender = render;
            outBufFrames = bufFrames;
            return true;
        };

        IMMDeviceEnumerator* enumerator = nullptr;
        if (FAILED(CoCreateInstance(kClsidMMDeviceEnumerator, nullptr, CLSCTX_ALL, kIidIMMDeviceEnumerator,
                                    reinterpret_cast<void**>(&enumerator))) ||
            !enumerator) {
            cleanupCom();
            return false;
        }

        IAudioClient* audioClient = nullptr;
        IAudioRenderClient* renderClient = nullptr;
        UINT32 bufferFrames = 0;
        if (!openEndpoint(enumerator, audioClient, renderClient, bufferFrames)) {
            enumerator->Release();
            cleanupCom();
            return false;
        }

        const auto fillFrames = [&](UINT32 count) -> HRESULT {
            BYTE* pData = nullptr;
            const HRESULT hr = renderClient->GetBuffer(count, &pData);
            if (FAILED(hr)) return hr;
            auto* dst = reinterpret_cast<std::int16_t*>(pData);
            callback(user, dst, static_cast<std::size_t>(count));
            ApplyGain(dst, static_cast<std::size_t>(count) * static_cast<std::size_t>(channels),
                      gain.load(std::memory_order_relaxed));
            return renderClient->ReleaseBuffer(count, 0);
        };

        // Pre-fill up to 1.0s before starting the engine so startup is glitch-free
        // without over-draining smaller 2s ring targets.
        UINT32 prefillTarget = std::min<UINT32>(bufferFrames, static_cast<UINT32>(sampleRate));
        while (prefillTarget > 0 && !stopRequested.load(std::memory_order_acquire)) {
            const UINT32 chunk = std::min(prefillTarget, kWasapiWriteChunkFrames);
            if (FAILED(fillFrames(chunk))) {
                renderClient->Release();
                audioClient->Release();
                enumerator->Release();
                cleanupCom();
                return false;
            }
            prefillTarget -= chunk;
        }

        if (FAILED(audioClient->Start())) {
            renderClient->Release();
            audioClient->Release();
            enumerator->Release();
            cleanupCom();
            return false;
        }

        if (!outSignaledInit) {
            outSignaledInit = true;
            SignalInit(true);
        }

        while (!stopRequested.load(std::memory_order_acquire)) {
            UINT32 padding = 0;
            HRESULT hr = audioClient->GetCurrentPadding(&padding);
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
                audioClient->Stop();
                renderClient->Release();
                audioClient->Release();
                audioClient = nullptr;
                renderClient = nullptr;
                while (!stopRequested.load(std::memory_order_acquire) &&
                       !openEndpoint(enumerator, audioClient, renderClient, bufferFrames)) {
                    Sleep(100);
                }
                if (!audioClient || FAILED(audioClient->Start())) break;
                continue;
            }
            if (FAILED(hr)) break;

            const UINT32 avail = padding < bufferFrames ? (bufferFrames - padding) : 0;
            if (avail >= 512) {
                UINT32 remaining = avail;
                while (remaining >= 512 && !stopRequested.load(std::memory_order_acquire)) {
                    const UINT32 chunk = std::min(remaining, kWasapiWriteChunkFrames);
                    hr = fillFrames(chunk);
                    if (FAILED(hr)) break;
                    remaining -= chunk;
                }
                if (hr == AUDCLNT_E_DEVICE_INVALIDATED) continue;
                if (FAILED(hr)) break;
            }
            Sleep(20);
        }

        if (audioClient) audioClient->Stop();
        if (renderClient) renderClient->Release();
        if (audioClient) audioClient->Release();
        enumerator->Release();
        cleanupCom();
        return true;
    }

    bool RunWaveOut(bool& outSignaledInit) {
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event) return false;

        WAVEFORMATEX wfx{};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = static_cast<WORD>(channels);
        wfx.nSamplesPerSec = static_cast<DWORD>(sampleRate);
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = static_cast<WORD>(channels * sizeof(std::int16_t));
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

        HWAVEOUT hWaveOut = nullptr;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx,
                        reinterpret_cast<DWORD_PTR>(event), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
            CloseHandle(event);
            return false;
        }

        const std::size_t ch = static_cast<std::size_t>(channels);
        const std::size_t totalSamples = kWaveBufferFrames * ch;
        std::vector<WAVEHDR> headers(kWaveBufferCount, WAVEHDR{});
        std::vector<std::vector<std::int16_t>> buffers(
            kWaveBufferCount, std::vector<std::int16_t>(totalSamples, 0));
        std::vector<bool> prepared(kWaveBufferCount, false);
        std::vector<bool> inFlight(kWaveBufferCount, false);

        const auto cleanupWave = [&]() {
            waveOutReset(hWaveOut);
            for (std::size_t i = 0; i < kWaveBufferCount; ++i) {
                if (prepared[i]) waveOutUnprepareHeader(hWaveOut, &headers[i], sizeof(WAVEHDR));
            }
            waveOutClose(hWaveOut);
            CloseHandle(event);
        };

        waveOutPause(hWaveOut);
        for (std::size_t i = 0; i < kWaveBufferCount; ++i) {
            headers[i].lpData = reinterpret_cast<LPSTR>(buffers[i].data());
            headers[i].dwBufferLength = static_cast<DWORD>(totalSamples * sizeof(std::int16_t));
            if (waveOutPrepareHeader(hWaveOut, &headers[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
                cleanupWave();
                return false;
            }
            prepared[i] = true;
            std::int16_t* dst = buffers[i].data();
            callback(user, dst, kWaveBufferFrames);
            ApplyGain(dst, totalSamples, gain.load(std::memory_order_relaxed));
            inFlight[i] = (waveOutWrite(hWaveOut, &headers[i], sizeof(WAVEHDR)) == MMSYSERR_NOERROR);
        }
        waveOutRestart(hWaveOut);

        if (!outSignaledInit) {
            outSignaledInit = true;
            SignalInit(true);
        }

        while (!stopRequested.load(std::memory_order_acquire)) {
            bool submittedAny = false;
            for (std::size_t i = 0; i < kWaveBufferCount; ++i) {
                if (stopRequested.load(std::memory_order_acquire)) break;
                if (!inFlight[i] || (headers[i].dwFlags & WHDR_DONE) != 0) {
                    std::int16_t* dst = buffers[i].data();
                    callback(user, dst, kWaveBufferFrames);
                    ApplyGain(dst, totalSamples, gain.load(std::memory_order_relaxed));
                    headers[i].dwBufferLength = static_cast<DWORD>(totalSamples * sizeof(std::int16_t));
                    inFlight[i] = (waveOutWrite(hWaveOut, &headers[i], sizeof(WAVEHDR)) == MMSYSERR_NOERROR);
                    if (inFlight[i]) submittedAny = true;
                }
            }
            if (!submittedAny && !stopRequested.load(std::memory_order_acquire)) {
                WaitForSingleObject(event, 30);
            }
        }

        cleanupWave();
        return true;
    }

    void RunFeeder() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        bool signaledInit = false;
        if (!RunWasapi(signaledInit) && !stopRequested.load(std::memory_order_acquire)) {
            if (!RunWaveOut(signaledInit) && !signaledInit) {
                SignalInit(false);
            }
        } else if (!signaledInit) {
            SignalInit(false);
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
    {
        std::lock_guard<std::mutex> lock(impl_->initMutex);
        impl_->initDone = false;
        impl_->initOk = false;
    }
    impl_->sampleRate = sampleRate;
    impl_->channels = channels;
    impl_->callback = callback;
    impl_->user = user;

    impl_->feeder = std::thread(&Impl::RunFeeder, impl_);

    bool ok = false;
    {
        std::unique_lock<std::mutex> lock(impl_->initMutex);
        impl_->initCv.wait(lock, [&]() { return impl_->initDone; });
        ok = impl_->initOk;
    }
    if (!ok) {
        Stop();
        return false;
    }
    impl_->active = true;
    return true;
}

void RadioAudioOutput::Stop() {
    if (!impl_) return;
    impl_->stopRequested.store(true, std::memory_order_release);
    if (impl_->feeder.joinable()) impl_->feeder.join();
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
constexpr int kRaylibStreamFrames = 4096;

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
