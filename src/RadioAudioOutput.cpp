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
#ifndef DIRECTSOUND_VERSION
#define DIRECTSOUND_VERSION 0x0800
#endif
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

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

struct TimerPeriodGuard {
    bool active = false;
    TimerPeriodGuard() { active = (timeBeginPeriod(1) == TIMERR_NOERROR); }
    ~TimerPeriodGuard() {
        if (active) timeEndPeriod(1);
    }
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

    bool RunDirectSound(bool& outSignaledInit) {
        HMODULE dsoundLib = LoadLibraryW(L"dsound.dll");
        if (!dsoundLib) return false;

        using DirectSoundCreate8Fn = HRESULT(WINAPI*)(LPCGUID, LPDIRECTSOUND8*, LPUNKNOWN);
        auto pDirectSoundCreate8 = reinterpret_cast<DirectSoundCreate8Fn>(
            reinterpret_cast<void*>(GetProcAddress(dsoundLib, "DirectSoundCreate8")));
        if (!pDirectSoundCreate8) {
            FreeLibrary(dsoundLib);
            return false;
        }

        LPDIRECTSOUND8 ds8 = nullptr;
        if (FAILED(pDirectSoundCreate8(nullptr, &ds8, nullptr)) || !ds8) {
            FreeLibrary(dsoundLib);
            return false;
        }

        HWND hwnd = GetForegroundWindow();
        if (!hwnd) hwnd = GetDesktopWindow();
        if (FAILED(ds8->SetCooperativeLevel(hwnd, DSSCL_PRIORITY))) {
            if (FAILED(ds8->SetCooperativeLevel(GetDesktopWindow(), DSSCL_NORMAL))) {
                ds8->Release();
                FreeLibrary(dsoundLib);
                return false;
            }
        }

        WAVEFORMATEX wfx{};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = static_cast<WORD>(channels);
        wfx.nSamplesPerSec = static_cast<DWORD>(sampleRate);
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = static_cast<WORD>(channels * sizeof(std::int16_t));
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

        const DWORD blockAlign = wfx.nBlockAlign;
        const DWORD bufferBytes = static_cast<DWORD>(sampleRate) * 2U * blockAlign;      // Full 2.0s ring
        const DWORD prefillBytes = static_cast<DWORD>((sampleRate * 3) / 4) * blockAlign; // 0.75s initial prefill
        const DWORD targetLeadBytes = static_cast<DWORD>((sampleRate * 3) / 2) * blockAlign; // 1.5s lead
        const DWORD minWriteBytes = blockAlign * 256U;
        const DWORD maxChunkBytes = blockAlign * 4096U;
        const DWORD safetyMarginBytes = blockAlign * 128U;

        DSBUFFERDESC dsbd{};
        dsbd.dwSize = sizeof(DSBUFFERDESC);
        dsbd.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_LOCSOFTWARE;
        dsbd.dwBufferBytes = bufferBytes;
        dsbd.lpwfxFormat = &wfx;

        LPDIRECTSOUNDBUFFER dsBuf = nullptr;
        if (FAILED(ds8->CreateSoundBuffer(&dsbd, &dsBuf, nullptr)) || !dsBuf) {
            ds8->Release();
            FreeLibrary(dsoundLib);
            return false;
        }

        const auto fillLockedRegion = [&](void* ptr, DWORD bytes) {
            if (!ptr || bytes < blockAlign) return;
            const std::size_t frames = static_cast<std::size_t>(bytes / blockAlign);
            auto* dst = static_cast<std::int16_t*>(ptr);
            callback(user, dst, frames);
            ApplyGain(dst, frames * static_cast<std::size_t>(channels), gain.load(std::memory_order_relaxed));
        };

        const auto writeRingChunk = [&](DWORD offset, DWORD bytes) -> bool {
            void* p1 = nullptr;
            void* p2 = nullptr;
            DWORD b1 = 0;
            DWORD b2 = 0;
            HRESULT hr = dsBuf->Lock(offset, bytes, &p1, &b1, &p2, &b2, 0);
            if (hr == DSERR_BUFFERLOST) {
                dsBuf->Restore();
                hr = dsBuf->Lock(offset, bytes, &p1, &b1, &p2, &b2, 0);
            }
            if (FAILED(hr)) return false;
            fillLockedRegion(p1, b1);
            fillLockedRegion(p2, b2);
            dsBuf->Unlock(p1, b1, p2, b2);
            return true;
        };

        // Zero the full 2.0s buffer first, then prefill the opening 1.0s cleanly.
        {
            void* p1 = nullptr;
            void* p2 = nullptr;
            DWORD b1 = 0;
            DWORD b2 = 0;
            if (SUCCEEDED(dsBuf->Lock(0, bufferBytes, &p1, &b1, &p2, &b2, 0))) {
                if (p1 && b1 > 0) std::memset(p1, 0, b1);
                if (p2 && b2 > 0) std::memset(p2, 0, b2);
                dsBuf->Unlock(p1, b1, p2, b2);
            }
        }
        if (!writeRingChunk(0, prefillBytes)) {
            dsBuf->Release();
            ds8->Release();
            FreeLibrary(dsoundLib);
            return false;
        }
        dsBuf->SetCurrentPosition(0);
        if (FAILED(dsBuf->Play(0, 0, DSBPLAY_LOOPING))) {
            dsBuf->Release();
            ds8->Release();
            FreeLibrary(dsoundLib);
            return false;
        }

        DWORD writeOffset = prefillBytes % bufferBytes;
        if (!outSignaledInit) {
            outSignaledInit = true;
            SignalInit(true);
        }

        TimerPeriodGuard timerGuard;
        while (!stopRequested.load(std::memory_order_acquire)) {
            DWORD playCursor = 0;
            DWORD writeCursor = 0;
            if (SUCCEEDED(dsBuf->GetCurrentPosition(&playCursor, &writeCursor))) {
                DWORD ahead = (writeOffset >= playCursor)
                    ? (writeOffset - playCursor)
                    : (bufferBytes - playCursor + writeOffset);
                const DWORD mixerLag = (writeCursor >= playCursor)
                    ? (writeCursor - playCursor)
                    : (bufferBytes - playCursor + writeCursor);

                // Recover cleanly if a massive OS stall caused playCursor to lap writeOffset.
                if (ahead <= mixerLag || ahead > bufferBytes - safetyMarginBytes) {
                    writeOffset = (writeCursor + safetyMarginBytes) % bufferBytes;
                    writeOffset -= (writeOffset % blockAlign);
                    ahead = (writeOffset >= playCursor)
                        ? (writeOffset - playCursor)
                        : (bufferBytes - playCursor + writeOffset);
                }

                if (ahead < targetLeadBytes) {
                    DWORD needed = targetLeadBytes - ahead;
                    const DWORD maxSafe = (bufferBytes > ahead + safetyMarginBytes)
                        ? (bufferBytes - ahead - safetyMarginBytes)
                        : 0;
                    needed = std::min(needed, maxSafe);
                    needed -= (needed % blockAlign);
                    while (needed >= minWriteBytes && !stopRequested.load(std::memory_order_acquire)) {
                        const DWORD chunk = std::min(needed, maxChunkBytes);
                        if (!writeRingChunk(writeOffset, chunk)) break;
                        writeOffset = (writeOffset + chunk) % bufferBytes;
                        needed -= chunk;
                    }
                }
            }
            Sleep(15);
        }

        dsBuf->Stop();
        dsBuf->Release();
        ds8->Release();
        FreeLibrary(dsoundLib);
        return true;
    }

    bool RunWasapiEvent(bool& outSignaledInit) {
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

        IMMDeviceEnumerator* enumerator = nullptr;
        if (FAILED(CoCreateInstance(kClsidMMDeviceEnumerator, nullptr, CLSCTX_ALL, kIidIMMDeviceEnumerator,
                                    reinterpret_cast<void**>(&enumerator))) ||
            !enumerator) {
            cleanupCom();
            return false;
        }

        IMMDevice* device = nullptr;
        if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device)) || !device) {
            enumerator->Release();
            cleanupCom();
            return false;
        }

        IAudioClient* audioClient = nullptr;
        HRESULT hr = device->Activate(kIidIAudioClient, CLSCTX_ALL, nullptr,
                                      reinterpret_cast<void**>(&audioClient));
        device->Release();
        if (FAILED(hr) || !audioClient) {
            enumerator->Release();
            cleanupCom();
            return false;
        }

        const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                            AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 0, 0, &wfx, nullptr);
        UINT32 bufferFrames = 0;
        IAudioRenderClient* renderClient = nullptr;
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (FAILED(hr) || !event ||
            FAILED(audioClient->SetEventHandle(event)) ||
            FAILED(audioClient->GetBufferSize(&bufferFrames)) || bufferFrames == 0 ||
            FAILED(audioClient->GetService(kIidIAudioRenderClient, reinterpret_cast<void**>(&renderClient))) ||
            !renderClient) {
            if (event) CloseHandle(event);
            if (renderClient) renderClient->Release();
            audioClient->Release();
            enumerator->Release();
            cleanupCom();
            return false;
        }

        BYTE* pInit = nullptr;
        if (SUCCEEDED(renderClient->GetBuffer(bufferFrames, &pInit))) {
            auto* dst = reinterpret_cast<std::int16_t*>(pInit);
            callback(user, dst, static_cast<std::size_t>(bufferFrames));
            ApplyGain(dst, static_cast<std::size_t>(bufferFrames) * static_cast<std::size_t>(channels),
                      gain.load(std::memory_order_relaxed));
            renderClient->ReleaseBuffer(bufferFrames, 0);
        }

        if (FAILED(audioClient->Start())) {
            CloseHandle(event);
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
            WaitForSingleObject(event, 50);
            if (stopRequested.load(std::memory_order_acquire)) break;
            UINT32 padding = 0;
            hr = audioClient->GetCurrentPadding(&padding);
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED || FAILED(hr)) break;
            const UINT32 avail = padding < bufferFrames ? (bufferFrames - padding) : 0;
            if (avail > 0) {
                BYTE* pData = nullptr;
                hr = renderClient->GetBuffer(avail, &pData);
                if (FAILED(hr)) break;
                auto* dst = reinterpret_cast<std::int16_t*>(pData);
                callback(user, dst, static_cast<std::size_t>(avail));
                ApplyGain(dst, static_cast<std::size_t>(avail) * static_cast<std::size_t>(channels),
                          gain.load(std::memory_order_relaxed));
                renderClient->ReleaseBuffer(avail, 0);
            }
        }

        audioClient->Stop();
        CloseHandle(event);
        renderClient->Release();
        audioClient->Release();
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

        TimerPeriodGuard timerGuard;
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
        if (RunDirectSound(signaledInit) || stopRequested.load(std::memory_order_acquire)) {
            if (!signaledInit) SignalInit(false);
            return;
        }
        if (RunWasapiEvent(signaledInit) || stopRequested.load(std::memory_order_acquire)) {
            if (!signaledInit) SignalInit(false);
            return;
        }
        if (!RunWaveOut(signaledInit) && !signaledInit) {
            SignalInit(false);
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
