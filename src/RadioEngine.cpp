#include "RadioEngine.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4505)
#endif
#define DRMP3_API static
#define DRMP3_PRIVATE static
// dr_mp3 decodes to int16 unless this is set. Its low-level decoder writes into
// the caller's buffer as drmp3d_sample_t, so without it the f32 buffer below is
// filled with raw int16 bit patterns and every sample turns into garbage
// (which drmp3dec_f32_to_s16 then flattens into silence).
#define DR_MP3_FLOAT_OUTPUT
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "third_party/dr_mp3.h"

static_assert(sizeof(drmp3d_sample_t) == sizeof(float),
              "dr_mp3 must decode to f32: Mp3Decoder converts f32 -> s16");
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {
constexpr std::size_t kMaxPlaylistBytes = 65536;
constexpr std::size_t kReadChunkBytes = 8192;
constexpr int kReceiveTimeoutMs = 400;   // keeps Stop() responsive
constexpr int kConnectTimeoutSeconds = 5;
constexpr std::size_t kMp3LookaheadBytes = 4096;   // bytes needed before a frame is decodable
constexpr std::size_t kMp3TailPreserveBytes = 2304; // largest possible MP3 frame

using DataCallback = std::function<void(const std::uint8_t*, std::size_t)>;

// --- Windows: WinHTTP handles DNS, TLS, proxies and chunked bodies for us. ---
#if defined(_WIN32)
std::wstring ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), len);
    return out;
}

std::string ToUtf8(std::wstring_view wide) {
    if (wide.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                        nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<std::size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), len,
                        nullptr, nullptr);
    return out;
}

struct WinHttpHandle {
    HINTERNET handle = nullptr;
    ~WinHttpHandle() { if (handle) WinHttpCloseHandle(handle); }
};

// Opens a request and leaves it positioned at the response body.
bool OpenWinHttpRequest(const std::string& url, WinHttpHandle& session, WinHttpHandle& connect,
                        WinHttpHandle& request, std::string& error) {
    const ParsedUrl parsed = ParseUrl(url);
    if (!parsed.valid) { error = "Invalid URL"; return false; }

    session.handle = WinHttpOpen(L"DenshaDeKoi/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.handle) { error = "WinHTTP session failed"; return false; }
    WinHttpSetTimeouts(session.handle, 5000, 5000, 5000, kReceiveTimeoutMs);

    const std::wstring host = ToWide(parsed.host);
    connect.handle = WinHttpConnect(session.handle, host.c_str(),
                                   static_cast<INTERNET_PORT>(parsed.port), 0);
    if (!connect.handle) { error = "Connect failed"; return false; }

    const DWORD flags = parsed.scheme == "https" ? WINHTTP_FLAG_SECURE : 0;
    const std::wstring path = ToWide(parsed.path);
    request.handle = WinHttpOpenRequest(connect.handle, L"GET", path.c_str(), nullptr,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request.handle) { error = "Request failed"; return false; }

    // Stations redirect .pls entries and legacy mount points all the time, and
    // some CDNs answer 403 to WinHTTP's default user agent.
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request.handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy,
                     sizeof(redirectPolicy));
    WinHttpAddRequestHeaders(request.handle, L"User-Agent: DenshaDeKoi/1.0\r\n", (DWORD)-1,
                             WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    if (!WinHttpSendRequest(request.handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA,
                            0, 0, 0) ||
        !WinHttpReceiveResponse(request.handle, nullptr)) {
        error = "Server did not respond";
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                            WINHTTP_NO_HEADER_INDEX)) {
        if (status < 200 || status >= 300) {
            error = "HTTP " + std::to_string(status);
            return false;
        }
    }
    return true;
}

bool FetchSmallText(const std::string& url, std::string& body, const std::atomic<bool>& stop,
                    std::string& error) {
    WinHttpHandle session, connect, request;
    if (!OpenWinHttpRequest(url, session, connect, request, error)) return false;
    body.clear();
    char buffer[4096];
    DWORD read = 0;
    while (!stop.load() && body.size() < kMaxPlaylistBytes &&
           WinHttpReadData(request.handle, buffer, sizeof(buffer), &read) && read > 0) {
        body.append(buffer, std::min<std::size_t>(read, kMaxPlaylistBytes - body.size()));
    }
    if (body.empty()) { error = "Empty playlist"; return false; }
    return true;
}

bool StreamHttp(const std::string& url, const std::atomic<bool>& stop, const DataCallback& onData,
                std::string& error) {
    WinHttpHandle session, connect, request;
    if (!OpenWinHttpRequest(url, session, connect, request, error)) return false;
    std::uint8_t buffer[kReadChunkBytes];
    DWORD read = 0;
    while (!stop.load()) {
        if (WinHttpReadData(request.handle, buffer, sizeof(buffer), &read)) {
            if (read == 0) { error = "Stream ended"; return false; }
            onData(buffer, static_cast<std::size_t>(read));
            continue;
        }
        // A short receive timeout only means "no data yet": keep waiting so a
        // slow station is not treated as dead, and Stop() stays responsive.
        if (GetLastError() == ERROR_WINHTTP_TIMEOUT) continue;
        error = "Read failed";
        return false;
    }
    error = "Stopped";
    return false;
}

#else // ---------------------------------------------------------------------
// --- POSIX: plain HTTP/1.0 over a blocking socket (no TLS, no proxies). -----

struct SocketHandle {
    int fd = -1;
    ~SocketHandle() { if (fd >= 0) ::close(fd); }
};

// Non-blocking connect with a hard 5 s cap, so a dead host cannot hang the
// stream thread (and therefore Stop()) for minutes.
bool TryConnect(const ParsedUrl& url, SocketHandle& out, std::string& error) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    const std::string port = std::to_string(url.port);
    if (getaddrinfo(url.host.c_str(), port.c_str(), &hints, &result) != 0 || !result) {
        error = "DNS lookup failed";
        return false;
    }
    bool connected = false;
    for (addrinfo* it = result; it && !connected; it = it->ai_next) {
        const int fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) continue;
        const int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        int rc = connect(fd, it->ai_addr, static_cast<socklen_t>(it->ai_addrlen));
        if (rc != 0 && errno == EINPROGRESS) {
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(fd, &writeSet);
            timeval wait{kConnectTimeoutSeconds, 0};
            rc = (select(fd + 1, nullptr, &writeSet, nullptr, &wait) > 0) ? 0 : -1;
            if (rc == 0) {
                int socketError = 0;
                socklen_t length = sizeof(socketError);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &length);
                if (socketError != 0) rc = -1;
            }
        }
        if (rc == 0) {
            fcntl(fd, F_SETFL, flags);
            timeval timeout{kReceiveTimeoutMs / 1000, (kReceiveTimeoutMs % 1000) * 1000};
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
            out.fd = fd;
            connected = true;
        } else {
            ::close(fd);
        }
    }
    freeaddrinfo(result);
    if (!connected) error = "Connection failed";
    return connected;
}

// Sends the request and returns once the response head has been consumed.
bool OpenSocketStream(const std::string& url, SocketHandle& socket, std::string& error,
                      std::string& carry) {
    const ParsedUrl parsed = ParseUrl(url);
    if (!parsed.valid) { error = "Invalid URL"; return false; }
    if (parsed.scheme == "https") { error = "HTTPS needs Windows"; return false; }
    if (!TryConnect(parsed, socket, error)) return false;

    const std::string hostHeader =
        parsed.port == 80 ? parsed.host : parsed.host + ":" + std::to_string(parsed.port);
    const std::string request = "GET " + parsed.path + " HTTP/1.0\r\n" "Host: " + hostHeader +
        "\r\nUser-Agent: DenshaDeKoi/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n";
    std::size_t sent = 0;
    while (sent < request.size()) {
        const ssize_t n = send(socket.fd, request.data() + sent, request.size() - sent, 0);
        if (n <= 0) { error = "Send failed"; return false; }
        sent += static_cast<std::size_t>(n);
    }

    carry.clear();
    char buffer[4096];
    std::size_t headEnd = std::string::npos;
    while (carry.size() < 16384) {
        const ssize_t n = recv(socket.fd, buffer, sizeof(buffer), 0);
        if (n <= 0) { error = "Server did not respond"; return false; }
        carry.append(buffer, static_cast<std::size_t>(n));
        headEnd = carry.find("\r\n\r\n");
        if (headEnd != std::string::npos) { headEnd += 4; break; }
    }
    if (headEnd == std::string::npos) { error = "Malformed response"; return false; }

    int status = 0;
    // Icecast answers "HTTP/1.0 200 OK", SHOUTcast servers answer "ICY 200 OK".
    if (std::sscanf(carry.c_str(), "HTTP/%*s %d", &status) != 1 &&
        std::sscanf(carry.c_str(), "ICY %d", &status) != 1) {
        error = "Malformed status line";
        return false;
    }
    if (status < 200 || status >= 300) {
        error = "HTTP " + std::to_string(status);
        return false;
    }
    carry.erase(0, headEnd); // body bytes that arrived with the head
    return true;
}

bool StreamHttp(const std::string& url, const std::atomic<bool>& stop, const DataCallback& onData,
                std::string& error) {
    SocketHandle socket;
    std::string carry;
    if (!OpenSocketStream(url, socket, error, carry)) return false;
    if (!carry.empty()) onData(reinterpret_cast<const std::uint8_t*>(carry.data()), carry.size());

    std::uint8_t buffer[kReadChunkBytes];
    while (!stop.load()) {
        const ssize_t n = recv(socket.fd, buffer, sizeof(buffer), 0);
        if (n > 0) {
            onData(buffer, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) { error = "Stream ended"; return false; }
        if (errno == EAGAIN || errno == EWOULDBLOCK) continue; // timeout, not EOF
        error = "Read failed";
        return false;
    }
    error = "Stopped";
    return false;
}

bool FetchSmallText(const std::string& url, std::string& body, const std::atomic<bool>& stop,
                    std::string& error) {
    SocketHandle socket;
    std::string carry;
    if (!OpenSocketStream(url, socket, error, carry)) return false;
    body = carry;
    char buffer[4096];
    while (!stop.load() && body.size() < kMaxPlaylistBytes) {
        const ssize_t n = recv(socket.fd, buffer, sizeof(buffer), 0);
        if (n <= 0) break;
        body.append(buffer, std::min<std::size_t>(static_cast<std::size_t>(n),
                                                  kMaxPlaylistBytes - body.size()));
    }
    if (body.empty()) { error = "Empty playlist"; return false; }
    return true;
}
#endif

const char* StateLabel(RadioState state) {
    switch (state) {
    case RadioState::OFF: return "OFF";
    case RadioState::CONNECTING: return "CONNECTING";
    case RadioState::BUFFERING: return "BUFFERING";
    case RadioState::PLAYING: return "PLAYING";
    case RadioState::FAILED: return "FAILED";
    }
    return "?";
}
} // namespace

// --- PcmQueue ---------------------------------------------------------------
PcmQueue::PcmQueue(std::size_t capacityFrames)
    : samples_(capacityFrames * 2, 0), capacityFrames_(capacityFrames) {}

void PcmQueue::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    read_ = 0;
    size_ = 0;
    sampleRate_ = 0;
    channels_ = 0;
}

void PcmQueue::SetFormat(int sampleRate, int channels) {
    if (sampleRate <= 0 || (channels != 1 && channels != 2)) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (sampleRate_ == sampleRate && channels_ == channels) return;
    sampleRate_ = sampleRate;
    channels_ = channels;
    read_ = 0;
    size_ = 0; // format changed: old audio no longer matches
}

bool PcmQueue::Format(int& sampleRate, int& channels) const {
    std::lock_guard<std::mutex> lock(mutex_);
    sampleRate = sampleRate_;
    channels = channels_;
    return sampleRate_ > 0 && channels_ > 0;
}

std::size_t PcmQueue::Push(const std::int16_t* interleaved, std::size_t frames) {
    if (!interleaved || frames == 0) return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    if (channels_ <= 0) return 0;
    const std::size_t ch = static_cast<std::size_t>(channels_);
    const std::size_t freeFrames = capacityFrames_ - size_;
    const std::size_t count = std::min(frames, freeFrames);
    const std::size_t write = (read_ + size_) % capacityFrames_;
    const std::size_t first = std::min(count, capacityFrames_ - write);
    std::memcpy(samples_.data() + write * ch, interleaved, first * ch * sizeof(std::int16_t));
    if (count > first) {
        std::memcpy(samples_.data(), interleaved + first * ch, (count - first) * ch * sizeof(std::int16_t));
    }
    size_ += count;
    return count;
}

std::size_t PcmQueue::Pop(std::int16_t* out, std::size_t maxFrames) {
    if (!out || maxFrames == 0) return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    if (channels_ <= 0) return 0;
    const std::size_t ch = static_cast<std::size_t>(channels_);
    const std::size_t count = std::min(maxFrames, size_);
    const std::size_t first = std::min(count, capacityFrames_ - read_);
    std::memcpy(out, samples_.data() + read_ * ch, first * ch * sizeof(std::int16_t));
    if (count > first) {
        std::memcpy(out + first * ch, samples_.data(), (count - first) * ch * sizeof(std::int16_t));
    }
    read_ = (read_ + count) % capacityFrames_;
    size_ -= count;
    return count;
}

std::size_t PcmQueue::AvailableFrames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
}

// --- PcmPlaybackGate --------------------------------------------------------
std::size_t PcmPlaybackGate::Fill(PcmQueue& queue, std::int16_t* out, std::size_t frames,
                                 int channels) {
    if (!out || frames == 0 || channels <= 0) return 0;
    if (!playing_.load(std::memory_order_relaxed)) return 0;

    int queueRate = 0;
    int queueChannels = 0;
    if (!queue.Format(queueRate, queueChannels) || queueChannels != channels) return 0;

    const std::size_t got = queue.Pop(out, frames);
    if (got < frames) starving_.store(true, std::memory_order_relaxed);
    return got;
}

void PcmPlaybackGate::Poll(PcmQueue& queue) {
    int sampleRate = 0;
    int channels = 0;
    if (!queue.Format(sampleRate, channels) || sampleRate <= 0) return;

    // The device ran dry: stop feeding it and build the cushion again, otherwise
    // every following block would be padded with silence again.
    if (starving_.exchange(false, std::memory_order_relaxed) &&
        playing_.exchange(false, std::memory_order_relaxed)) {
        ++dropouts_;
        std::printf("[radio] underrun: rebuffering (%.0f ms needed)\n",
                    static_cast<double>(ResumeSeconds) * 1000.0);
        std::fflush(stdout);
    }
    if (playing_.load(std::memory_order_relaxed)) return;

    const float needed = everStarted_ ? ResumeSeconds : StartSeconds;
    const auto frames = static_cast<std::size_t>(needed * static_cast<float>(sampleRate));
    if (queue.AvailableFrames() < frames) return;

    playing_.store(true, std::memory_order_relaxed);
    everStarted_ = true;
    std::printf("[radio] playing (%.1f s buffered)\n", static_cast<double>(needed));
    std::fflush(stdout);
}

// --- Mp3Decoder -------------------------------------------------------------
struct Mp3Decoder::Impl {
    drmp3dec decoder{};
};

Mp3Decoder::Mp3Decoder() : impl_(std::make_unique<Impl>()) {
    drmp3dec_init(&impl_->decoder);
}

Mp3Decoder::~Mp3Decoder() = default;

void Mp3Decoder::Reset() {
    drmp3dec_init(&impl_->decoder);
    inBuf_.clear();
    decodedFrames_ = 0;
}

void Mp3Decoder::Feed(const std::uint8_t* data, std::size_t size, const PcmCallback& onPcm,
                      bool flush) {
    if (size > 0) inBuf_.insert(inBuf_.end(), data, data + size);

    std::size_t consumed = 0;
    float pcmFloat[1152 * 2];
    std::int16_t pcmS16[1152 * 2];

    const std::size_t minRemaining = flush ? 1 : kMp3LookaheadBytes;
    while (inBuf_.size() > consumed && inBuf_.size() - consumed >= minRemaining) {
        drmp3dec_frame_info info{};
        const std::size_t remaining = inBuf_.size() - consumed;
        const int available = static_cast<int>(std::min<std::size_t>(remaining, 32768));
        const int frames = drmp3dec_decode_frame(&impl_->decoder, inBuf_.data() + consumed, available,
                                                 pcmFloat, &info);
        if (info.frame_bytes <= 0) break;

        const std::size_t step = static_cast<std::size_t>(info.frame_bytes);
        if (frames > 0 && info.channels > 0 && info.channels <= 2 && info.hz > 0) {
            consumed += step;
            drmp3dec_f32_to_s16(pcmFloat, pcmS16,
                                static_cast<std::size_t>(frames) * static_cast<std::size_t>(info.channels));
            decodedFrames_ += static_cast<std::size_t>(frames);
            if (onPcm) onPcm(pcmS16, static_cast<std::size_t>(frames), info.hz, info.channels);
            continue;
        }
        // Incomplete or corrupt frame at the chunk boundary: keep the tail so the
        // next chunk can complete it instead of dropping audio.
        if (step >= static_cast<std::size_t>(available) && remaining > kMp3TailPreserveBytes) {
            consumed += std::min(step, remaining - kMp3TailPreserveBytes);
            continue;
        }
        consumed += step;
    }

    if (consumed > 0) inBuf_.erase(inBuf_.begin(), inBuf_.begin() + static_cast<std::ptrdiff_t>(consumed));
    if (inBuf_.size() > 65536) {
        inBuf_.erase(inBuf_.begin(), inBuf_.end() - static_cast<std::ptrdiff_t>(kMp3LookaheadBytes));
    }
}

// --- RadioEngine ------------------------------------------------------------
RadioEngine::~RadioEngine() { Stop(); }

RadioStatus RadioEngine::Status() const {
    std::lock_guard<std::mutex> lock(statusMutex_);
    RadioStatus status = status_;
    int sampleRate = 0;
    int channels = 0;
    if (queue_.Format(sampleRate, channels) && sampleRate > 0) {
        status.bufferedSec = static_cast<float>(queue_.AvailableFrames()) / static_cast<float>(sampleRate);
    }
    return status;
}

void RadioEngine::SetState(RadioState state, std::string text) {
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        if (status_.state == state && status_.statusText == text) return;
        status_.state = state;
        status_.statusText = text;
        std::printf("[radio] %s: %s\n", StateLabel(state), status_.statusText.c_str());
    }
    std::fflush(stdout);
}

void RadioEngine::Start(std::string stationName, std::vector<std::string> candidateUrls) {
    Stop();
    stopRequested_.store(false);
    queue_.Clear();
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        status_ = RadioStatus{};
        status_.enabled = true;
        status_.state = RadioState::CONNECTING;
        status_.stationName = stationName;
        status_.statusText = "Connecting...";
    }
    std::printf("[radio] start: %s (%zu url(s))\n", stationName.c_str(), candidateUrls.size());
    std::fflush(stdout);
    worker_ = std::thread(&RadioEngine::Run, this, std::move(candidateUrls));
}

void RadioEngine::Stop() {
    stopRequested_.store(true);
    if (worker_.joinable()) worker_.join();
    queue_.Clear();
    std::lock_guard<std::mutex> lock(statusMutex_);
    status_.enabled = false;
    status_.state = RadioState::OFF;
    status_.statusText = "Radio off";
}

void RadioEngine::PushBlocking(const std::int16_t* pcm, std::size_t frames, int channels) {
    if (channels <= 0) return;
    const std::size_t stride = static_cast<std::size_t>(channels);
    std::size_t written = 0;
    while (written < frames && !stopRequested_.load()) {
        const std::size_t n = queue_.Push(pcm + written * stride, frames - written);
        written += n;
        if (written < frames) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void RadioEngine::Run(std::vector<std::string> candidateUrls) {
    // Resolve .pls playlists up front; fall back to the direct stream URL.
    std::vector<std::string> urls;
    for (const std::string& candidate : candidateUrls) {
        if (stopRequested_.load()) return;
        if (!ParseUrl(candidate).valid) continue;
        if (LooksLikePlaylistUrl(candidate)) {
            std::string body;
            std::string error;
            if (FetchSmallText(candidate, body, stopRequested_, error)) {
                for (const auto& entry : ParsePlsPlaylist(body)) urls.push_back(entry.url);
            } else {
                std::printf("[radio] playlist failed (%s): %s\n", error.c_str(), candidate.c_str());
                std::fflush(stdout);
            }
        } else {
            urls.push_back(candidate);
        }
    }
    if (urls.empty()) {
        SetState(RadioState::FAILED, "No stream URL");
        return;
    }

    int failedRounds = 0;
    while (!stopRequested_.load()) {
        bool heardAudio = false;
        for (const std::string& url : urls) {
            if (stopRequested_.load()) return;
            SetState(RadioState::CONNECTING, "Connecting...");
            Mp3Decoder decoder;
            bool announced = false;
            std::size_t receivedBytes = 0;
            std::string error;

            const auto onData = [&](const std::uint8_t* bytes, std::size_t count) {
                receivedBytes += count;
                decoder.Feed(bytes, count,
                             [&](const std::int16_t* pcm, std::size_t frames, int sampleRate, int channels) {
                                 queue_.SetFormat(sampleRate, channels);
                                 PushBlocking(pcm, frames, channels);
                                 heardAudio = true;
                                 // The stream is live; PcmPlaybackGate decides when
                                 // the buffered audio actually reaches the device.
                                 if (!announced) {
                                     announced = true;
                                     SetState(RadioState::PLAYING, "Live");
                                 }
                             });
            };

            StreamHttp(url, stopRequested_, onData, error);
            if (stopRequested_.load()) return;
            if (!announced) {
                std::printf("[radio] %s (%s): %zu bytes, %zu decoded frames\n", error.c_str(),
                            url.c_str(), receivedBytes, decoder.DecodedFrames());
                if (receivedBytes > 0 && decoder.DecodedFrames() == 0) {
                    std::printf("[radio] stream is not MP3 data (dr_mp3 decoded nothing)\n");
                }
                std::fflush(stdout);
            }
        }

        if (stopRequested_.load()) return;
        if (!heardAudio && ++failedRounds >= 3) {
            SetState(RadioState::FAILED, "Stream unavailable");
            return;
        }
        if (heardAudio) failedRounds = 0;
        // If audio is still buffered, keep playing and retry quickly; otherwise
        // report buffering and back off so a dead station does not spin.
        int sampleRate = 0;
        int channels = 0;
        const bool haveFormat = queue_.Format(sampleRate, channels);
        const bool haveAudio =
            haveFormat && queue_.AvailableFrames() > static_cast<std::size_t>(sampleRate);
        if (!haveAudio) SetState(RadioState::BUFFERING, "Buffering...");
        std::this_thread::sleep_for(std::chrono::milliseconds(haveAudio ? 50 : 1000));
    }
}
