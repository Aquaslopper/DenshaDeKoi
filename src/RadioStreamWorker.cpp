#include "RadioStreamWorker.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4505)
#endif
#define DRMP3_API static
#define DRMP3_PRIVATE static
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "third_party/dr_mp3.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
constexpr std::size_t kBufferReadyFrames = 44100; // ~1.0s at 44.1 kHz
constexpr std::size_t kMp3LookaheadBytes = 4096;  // Matches proven low-latency MP3 streaming lookahead
constexpr std::size_t kMp3TailPreserveBytes = 2304; // Matches DRMP3_MAX_FREE_FORMAT_FRAME_SIZE
constexpr std::size_t kMaxPlaylistBytes = 65536;

std::string ToLowerAscii(std::string_view sv) {
    std::string out(sv);
    for (char& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return out;
}

std::string TrimAscii(std::string_view sv) {
    std::size_t first = 0;
    while (first < sv.size() && std::isspace(static_cast<unsigned char>(sv[first]))) ++first;
    std::size_t last = sv.size();
    while (last > first && std::isspace(static_cast<unsigned char>(sv[last - 1]))) --last;
    return std::string(sv.substr(first, last - first));
}

bool ContainsNoCase(std::string_view haystack, std::string_view needle) {
    return ToLowerAscii(haystack).find(ToLowerAscii(needle)) != std::string::npos;
}

std::size_t ParseIcyMetaIntHeader(std::string_view headers) {
    std::size_t pos = 0;
    while (pos < headers.size()) {
        std::size_t end = headers.find('\n', pos);
        if (end == std::string_view::npos) end = headers.size();
        const std::string line = TrimAscii(headers.substr(pos, end - pos));
        pos = end + 1;
        if (const std::size_t colon = line.find(':'); colon != std::string::npos) {
            const std::string key = ToLowerAscii(TrimAscii(std::string_view(line).substr(0, colon)));
            if (key == "icy-metaint") {
                const std::string val = TrimAscii(std::string_view(line).substr(colon + 1));
                std::size_t parsed = 0;
                const auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), parsed);
                if (ec == std::errc{} && ptr == val.data() + val.size()) return parsed;
            }
        }
    }
    return 0;
}

std::string ParseHeaderValue(std::string_view headers, std::string_view targetKey) {
    std::size_t pos = 0;
    while (pos < headers.size()) {
        std::size_t end = headers.find('\n', pos);
        if (end == std::string_view::npos) end = headers.size();
        const std::string line = TrimAscii(headers.substr(pos, end - pos));
        pos = end + 1;
        if (const std::size_t colon = line.find(':'); colon != std::string::npos) {
            const std::string key = ToLowerAscii(TrimAscii(std::string_view(line).substr(0, colon)));
            if (key == targetKey) return TrimAscii(std::string_view(line).substr(colon + 1));
        }
    }
    return "";
}

std::string ResolveRedirectUrl(const ParsedUrl& base, std::string_view location) {
    const std::string trimmed = TrimAscii(location);
    if (trimmed.empty()) return "";
    if (ParseUrl(trimmed).valid) return trimmed;
    if (trimmed.front() == '/') {
        const std::string portPart =
            ((base.scheme == "http" && base.port == 80) || (base.scheme == "https" && base.port == 443))
            ? ""
            : ":" + std::to_string(base.port);
        return base.scheme + "://" + base.host + portPart + trimmed;
    }
    return "";
}

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void CloseSocket(SocketHandle s) { if (s != INVALID_SOCKET) closesocket(s); }

struct WinsockInit {
    WinsockInit() {
        WSADATA wsa{};
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    ~WinsockInit() { WSACleanup(); }
};
void EnsureWinsock() {
    static WinsockInit init;
}

bool SetNonBlocking(SocketHandle s, bool nb) {
    u_long mode = nb ? 1 : 0;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
}

bool LastSocketWouldBlock() {
    const int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
}

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
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<std::size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), len, nullptr, nullptr);
    return out;
}

bool FetchTextWinHttp(const std::string& url, std::string& outBody, const std::atomic<bool>& stop) {
    const ParsedUrl parsed = ParseUrl(url);
    if (!parsed.valid) return false;
    const std::wstring wHost = ToWide(parsed.host);
    const std::wstring wPath = ToWide(parsed.path);
    HINTERNET hSession = WinHttpOpen(L"DenshaDeKoi/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    WinHttpSetTimeouts(hSession, 4000, 4000, 4000, 5000);
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), parsed.port, 0);
    HINTERNET hRequest = nullptr;
    bool ok = false;
    if (hConnect) {
        const DWORD flags = parsed.scheme == "https" ? WINHTTP_FLAG_SECURE : 0;
        hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), nullptr,
                                      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (hRequest && WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, nullptr)) {
            DWORD status = 0;
            DWORD statusSize = sizeof(status);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                                    WINHTTP_NO_HEADER_INDEX) &&
                status >= 200 && status < 300) {
                outBody.clear();
                char buf[4096];
                DWORD bytesRead = 0;
                while (!stop.load() && WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
                    const std::size_t take = std::min<std::size_t>(bytesRead, kMaxPlaylistBytes - outBody.size());
                    outBody.append(buf, take);
                    if (outBody.size() >= kMaxPlaylistBytes) break;
                }
                ok = !outBody.empty() && !stop.load();
            }
        }
    }
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

bool StreamWinHttpAudio(const std::string& url,
                        const std::atomic<bool>& stop,
                        const std::function<void(std::size_t metaInt, const std::string& icyName)>& onHeaders,
                        const std::function<bool(const std::uint8_t*, std::size_t)>& onChunk) {
    const ParsedUrl parsed = ParseUrl(url);
    if (!parsed.valid) return false;
    const std::wstring wHost = ToWide(parsed.host);
    const std::wstring wPath = ToWide(parsed.path);
    HINTERNET hSession = WinHttpOpen(L"DenshaDeKoi/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    WinHttpSetTimeouts(hSession, 5000, 5000, 5000, 6000);
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), parsed.port, 0);
    HINTERNET hRequest = nullptr;
    bool ok = false;
    if (hConnect) {
        const DWORD flags = parsed.scheme == "https" ? WINHTTP_FLAG_SECURE : 0;
        hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), nullptr,
                                      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (hRequest) {
            WinHttpAddRequestHeaders(hRequest, L"Icy-MetaData: 1\r\n", static_cast<DWORD>(-1L),
                                     WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hRequest, nullptr)) {
                DWORD status = 0;
                DWORD statusSize = sizeof(status);
                if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                                        WINHTTP_NO_HEADER_INDEX) &&
                    status >= 200 && status < 300) {
                    std::string headersUtf8;
                    DWORD hdrBytes = 0;
                    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                        WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &hdrBytes,
                                        WINHTTP_NO_HEADER_INDEX);
                    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && hdrBytes > 0) {
                        std::wstring rawHdr(hdrBytes / sizeof(wchar_t), L'\0');
                        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                                WINHTTP_HEADER_NAME_BY_INDEX, rawHdr.data(), &hdrBytes,
                                                WINHTTP_NO_HEADER_INDEX)) {
                            const std::size_t actualLen = wcsnlen(rawHdr.data(), rawHdr.size());
                            headersUtf8 = ToUtf8(std::wstring_view(rawHdr.data(), actualLen));
                        }
                    }
                    onHeaders(ParseIcyMetaIntHeader(headersUtf8), ParseHeaderValue(headersUtf8, "icy-name"));

                    std::uint8_t buf[8192];
                    DWORD bytesRead = 0;
                    ok = true;
                    while (!stop.load() && WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead)) {
                        if (bytesRead == 0) break;
                        if (!onChunk(buf, bytesRead)) {
                            ok = false;
                            break;
                        }
                    }
                }
            }
        }
    }
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
void CloseSocket(SocketHandle s) { if (s >= 0) close(s); }
void EnsureWinsock() {}

bool SetNonBlocking(SocketHandle s, bool nb) {
    const int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(s, F_SETFL, nb ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) == 0;
}

bool LastSocketWouldBlock() {
    return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS;
}
#endif

SocketHandle ConnectTcpSocket(const ParsedUrl& url, const std::atomic<bool>& stop) {
    EnsureWinsock();
    const std::string portStr = std::to_string(url.port);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(url.host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) return kInvalidSocket;

    SocketHandle sock = kInvalidSocket;
    for (addrinfo* p = res; p != nullptr && !stop.load(); p = p->ai_next) {
        SocketHandle candidate = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (candidate == kInvalidSocket) continue;
        const int rcvBufBytes = 262144;
        setsockopt(candidate, SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<const char*>(&rcvBufBytes), sizeof(rcvBufBytes));
        SetNonBlocking(candidate, true);
        const int rc = connect(candidate, p->ai_addr, static_cast<int>(p->ai_addrlen));
        if (rc == 0) {
            sock = candidate;
            break;
        }
        if (LastSocketWouldBlock()) {
            bool connected = false;
            for (int slice = 0; slice < 25 && !stop.load(); ++slice) {
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(candidate, &wfds);
                timeval tv{0, 200000};
#if defined(_WIN32)
                const int sel = select(0, nullptr, &wfds, nullptr, &tv);
#else
                const int sel = select(candidate + 1, nullptr, &wfds, nullptr, &tv);
#endif
                if (sel > 0) {
                    int soErr = 0;
                    socklen_t len = sizeof(soErr);
                    getsockopt(candidate, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soErr), &len);
                    connected = (soErr == 0);
                    break;
                }
                if (sel < 0) break;
            }
            if (connected) {
                sock = candidate;
                break;
            }
        }
        CloseSocket(candidate);
    }
    freeaddrinfo(res);
    return sock;
}

bool RecvWithWait(SocketHandle sock, char* out, int cap, int& outRead, const std::atomic<bool>& stop) {
    outRead = 0;
    for (int slice = 0; slice < 25 && !stop.load(); ++slice) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        timeval tv{0, 200000};
#if defined(_WIN32)
        const int sel = select(0, &rfds, nullptr, nullptr, &tv);
#else
        const int sel = select(sock + 1, &rfds, nullptr, nullptr, &tv);
#endif
        if (sel > 0) {
            const int n = recv(sock, out, cap, 0);
            if (n > 0) {
                outRead = n;
                return true;
            }
            if (n == 0) return false;
            if (!LastSocketWouldBlock()) return false;
        } else if (sel < 0) {
            return false;
        }
    }
    return false;
}

bool SendAll(SocketHandle sock, std::string_view data, const std::atomic<bool>& stop) {
    std::size_t sent = 0;
    while (sent < data.size() && !stop.load()) {
        const int n = send(sock, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
            continue;
        }
        if (!LastSocketWouldBlock()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return sent == data.size();
}

bool StreamHttpSocketAudio(const std::string& initialUrl,
                           const std::atomic<bool>& stop,
                           const std::function<void(std::size_t metaInt, const std::string& icyName)>& onHeaders,
                           const std::function<bool(const std::uint8_t*, std::size_t)>& onChunk) {
    std::string currentUrl = initialUrl;
    for (int redirect = 0; redirect < 5 && !stop.load(); ++redirect) {
        const ParsedUrl parsed = ParseUrl(currentUrl);
        if (!parsed.valid) return false;
        if (parsed.scheme == "https") {
#if defined(_WIN32)
            return StreamWinHttpAudio(currentUrl, stop, onHeaders, onChunk);
#else
            return false;
#endif
        }

        SocketHandle sock = ConnectTcpSocket(parsed, stop);
        if (sock == kInvalidSocket) return false;

        const std::string hostHeader = (parsed.port == 80)
            ? parsed.host
            : parsed.host + ":" + std::to_string(parsed.port);
        const std::string req =
            "GET " + parsed.path + " HTTP/1.0\r\n"
            "Host: " + hostHeader + "\r\n"
            "User-Agent: DenshaDeKoi/1.0\r\n"
            "Accept: */*\r\n"
            "Icy-MetaData: 1\r\n"
            "Connection: close\r\n\r\n";
        if (!SendAll(sock, req, stop)) {
            CloseSocket(sock);
            return false;
        }

        std::string headBuf;
        headBuf.reserve(2048);
        std::size_t bodyStart = std::string::npos;
        char chunk[4096];
        int n = 0;
        while (!stop.load() && headBuf.size() < 16384 && RecvWithWait(sock, chunk, sizeof(chunk), n, stop)) {
            headBuf.append(chunk, static_cast<std::size_t>(n));
            if (const std::size_t pos = headBuf.find("\r\n\r\n"); pos != std::string::npos) {
                bodyStart = pos + 4;
                break;
            }
            if (const std::size_t pos = headBuf.find("\n\n"); pos != std::string::npos) {
                bodyStart = pos + 2;
                break;
            }
        }
        if (bodyStart == std::string::npos) {
            CloseSocket(sock);
            return false;
        }

        const std::string headerBlock = headBuf.substr(0, bodyStart);
        const std::size_t firstLineEnd = headerBlock.find('\n');
        const std::string statusLine = TrimAscii(headerBlock.substr(0, firstLineEnd));
        int statusCode = 0;
        if (const std::size_t sp = statusLine.find(' '); sp != std::string::npos) {
            const std::string rest = TrimAscii(std::string_view(statusLine).substr(sp + 1));
            std::from_chars(rest.data(), rest.data() + rest.size(), statusCode);
        }

        if (statusCode >= 300 && statusCode < 400) {
            const std::string loc = ParseHeaderValue(headerBlock, "location");
            CloseSocket(sock);
            const std::string next = ResolveRedirectUrl(parsed, loc);
            if (next.empty()) return false;
            currentUrl = next;
            continue;
        }
        if (statusCode < 200 || statusCode >= 300) {
            CloseSocket(sock);
            return false;
        }

        onHeaders(ParseIcyMetaIntHeader(headerBlock), ParseHeaderValue(headerBlock, "icy-name"));

        const bool isChunked = ContainsNoCase(ParseHeaderValue(headerBlock, "transfer-encoding"), "chunked");
        HttpChunkedDecoder chunked;
        chunked.Reset(isChunked);
        bool keepGoing = true;
        const auto emitBody = [&](const std::uint8_t* bytes, std::size_t count) -> bool {
            chunked.Feed(bytes, count, [&](const std::uint8_t* payload, std::size_t payloadLen) {
                if (keepGoing && !onChunk(payload, payloadLen)) keepGoing = false;
            });
            return keepGoing;
        };

        if (bodyStart < headBuf.size()) {
            const auto* ptr = reinterpret_cast<const std::uint8_t*>(headBuf.data() + bodyStart);
            if (!emitBody(ptr, headBuf.size() - bodyStart)) {
                CloseSocket(sock);
                return false;
            }
        }
        while (!stop.load() && RecvWithWait(sock, chunk, sizeof(chunk), n, stop)) {
            if (!emitBody(reinterpret_cast<const std::uint8_t*>(chunk), static_cast<std::size_t>(n))) {
                CloseSocket(sock);
                return false;
            }
        }
        CloseSocket(sock);
        return true;
    }
    return false;
}

bool FetchPlaylistText(const std::string& url, std::string& outBody, const std::atomic<bool>& stop) {
    const ParsedUrl parsed = ParseUrl(url);
    if (!parsed.valid) return false;
#if defined(_WIN32)
    return FetchTextWinHttp(url, outBody, stop);
#else
    if (parsed.scheme == "https") return false;
    SocketHandle sock = ConnectTcpSocket(parsed, stop);
    if (sock == kInvalidSocket) return false;
    const std::string hostHeader = (parsed.port == 80)
        ? parsed.host
        : parsed.host + ":" + std::to_string(parsed.port);
    const std::string req =
        "GET " + parsed.path + " HTTP/1.0\r\n"
        "Host: " + hostHeader + "\r\n"
        "User-Agent: DenshaDeKoi/1.0\r\n"
        "Connection: close\r\n\r\n";
    if (!SendAll(sock, req, stop)) {
        CloseSocket(sock);
        return false;
    }
    std::string raw;
    char buf[4096];
    int n = 0;
    while (!stop.load() && raw.size() < kMaxPlaylistBytes && RecvWithWait(sock, buf, sizeof(buf), n, stop)) {
        raw.append(buf, static_cast<std::size_t>(n));
    }
    CloseSocket(sock);
    if (const std::size_t pos = raw.find("\r\n\r\n"); pos != std::string::npos) {
        outBody = raw.substr(pos + 4);
        return !outBody.empty();
    }
    return false;
#endif
}
} // namespace

void HttpChunkedDecoder::Reset(bool enabled) {
    enabled_ = enabled;
    phase_ = Phase::SIZE_LINE;
    chunkRemaining_ = 0;
    sizeLine_.clear();
}

void HttpChunkedDecoder::Feed(const std::uint8_t* data, std::size_t size,
                              const std::function<void(const std::uint8_t*, std::size_t)>& onPayload) {
    if (!enabled_) {
        if (size > 0) onPayload(data, size);
        return;
    }
    std::size_t off = 0;
    while (off < size) {
        if (phase_ == Phase::SIZE_LINE) {
            const char c = static_cast<char>(data[off++]);
            if (c == '\n') {
                std::string hex = TrimAscii(sizeLine_);
                if (const std::size_t semi = hex.find(';'); semi != std::string::npos) hex.resize(semi);
                hex = TrimAscii(hex);
                std::size_t len = 0;
                const auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), len, 16);
                sizeLine_.clear();
                if (ec != std::errc{} || ptr != hex.data() + hex.size()) {
                    // Malformed chunk line: fall back to pass-through to avoid stalling forever.
                    enabled_ = false;
                    if (off < size) onPayload(data + off, size - off);
                    return;
                }
                chunkRemaining_ = len;
                phase_ = (len == 0) ? Phase::TRAILER : Phase::DATA;
            } else if (c != '\r' && sizeLine_.size() < 64) {
                sizeLine_.push_back(c);
            }
        } else if (phase_ == Phase::DATA) {
            const std::size_t take = std::min(size - off, chunkRemaining_);
            if (take > 0) {
                onPayload(data + off, take);
                off += take;
                chunkRemaining_ -= take;
            }
            if (chunkRemaining_ == 0) phase_ = Phase::DATA_CR;
        } else if (phase_ == Phase::DATA_CR) {
            if (data[off] == '\r') {
                ++off;
                phase_ = Phase::DATA_LF;
            } else if (data[off] == '\n') {
                ++off;
                phase_ = Phase::SIZE_LINE;
            } else {
                phase_ = Phase::SIZE_LINE;
            }
        } else if (phase_ == Phase::DATA_LF) {
            if (data[off] == '\n') ++off;
            phase_ = Phase::SIZE_LINE;
        } else {
            off = size;
        }
    }
}

void IcyDemuxer::Reset(std::size_t metaInterval) {
    metaInterval_ = metaInterval;
    phase_ = Phase::AUDIO;
    audioRemaining_ = metaInterval;
    metaRemaining_ = 0;
    metaBuffer_.clear();
}

void IcyDemuxer::Feed(const std::uint8_t* data, std::size_t size,
                      const std::function<void(const std::uint8_t*, std::size_t)>& onAudio,
                      const std::function<void(const std::string&)>& onMeta) {
    if (metaInterval_ == 0) {
        if (size > 0) onAudio(data, size);
        return;
    }
    std::size_t offset = 0;
    while (offset < size) {
        if (phase_ == Phase::AUDIO) {
            const std::size_t take = std::min(size - offset, audioRemaining_);
            if (take > 0) {
                onAudio(data + offset, take);
                offset += take;
                audioRemaining_ -= take;
            }
            if (audioRemaining_ == 0) phase_ = Phase::META_LEN;
        } else if (phase_ == Phase::META_LEN) {
            const std::size_t blocks = data[offset++];
            metaRemaining_ = blocks * 16;
            metaBuffer_.clear();
            if (metaRemaining_ == 0) {
                phase_ = Phase::AUDIO;
                audioRemaining_ = metaInterval_;
            } else {
                metaBuffer_.reserve(metaRemaining_);
                phase_ = Phase::META_BODY;
            }
        } else {
            const std::size_t take = std::min(size - offset, metaRemaining_);
            metaBuffer_.append(reinterpret_cast<const char*>(data + offset), take);
            offset += take;
            metaRemaining_ -= take;
            if (metaRemaining_ == 0) {
                while (!metaBuffer_.empty() && metaBuffer_.back() == '\0') metaBuffer_.pop_back();
                const std::string title = ExtractIcyStreamTitle(metaBuffer_);
                if (!title.empty()) onMeta(title);
                metaBuffer_.clear();
                phase_ = Phase::AUDIO;
                audioRemaining_ = metaInterval_;
            }
        }
    }
}

struct Mp3StreamDecoder::Impl {
    drmp3dec dec{};
};

Mp3StreamDecoder::Mp3StreamDecoder() : impl_(std::make_unique<Impl>()) {
    drmp3dec_init(&impl_->dec);
}

Mp3StreamDecoder::~Mp3StreamDecoder() = default;

void Mp3StreamDecoder::Reset() {
    drmp3dec_init(&impl_->dec);
    inBuf_.clear();
    decodedFrames_ = 0;
    resyncCount_ = 0;
}

void Mp3StreamDecoder::Feed(
    const std::uint8_t* data, std::size_t size, bool flush,
    const std::function<void(const std::int16_t*, std::size_t, int, int)>& onPcm) {
    if (size > 0) inBuf_.insert(inBuf_.end(), data, data + size);

    const std::size_t minRemaining = flush ? 1 : kMp3LookaheadBytes;
    std::size_t consumed = 0;
    float pcmFloat[1152 * 2];
    std::int16_t pcmS16[1152 * 2];

    while (inBuf_.size() > consumed && (inBuf_.size() - consumed) >= minRemaining) {
        drmp3dec_frame_info info{};
        const std::size_t rem = inBuf_.size() - consumed;
        const int available = static_cast<int>(std::min<std::size_t>(rem, 32768));
        const int samples = drmp3dec_decode_frame(
            &impl_->dec, inBuf_.data() + consumed, available, pcmFloat, &info);
        if (info.frame_bytes <= 0) break;

        const std::size_t step = static_cast<std::size_t>(info.frame_bytes);
        if (samples > 0 && info.channels > 0 && info.channels <= 2 && info.hz > 0) {
            consumed += step;
            drmp3dec_f32_to_s16(pcmFloat, pcmS16,
                                static_cast<std::size_t>(samples) * static_cast<std::size_t>(info.channels));
            decodedFrames_ += static_cast<std::size_t>(samples);
            onPcm(pcmS16, static_cast<std::size_t>(samples), info.hz, info.channels);
        } else {
            // Preserve a full max-frame tail when syncing mid-stream so a partial frame
            // straddling a network chunk boundary is never discarded.
            if (!flush && step >= static_cast<std::size_t>(available)) {
                if (rem > kMp3TailPreserveBytes) {
                    if (decodedFrames_ > 0) ++resyncCount_;
                    consumed += std::min(step, rem - kMp3TailPreserveBytes);
                    continue;
                }
                break;
            }
            if (decodedFrames_ > 0) ++resyncCount_;
            consumed += step;
        }
    }
    if (consumed > 0) {
        inBuf_.erase(inBuf_.begin(), inBuf_.begin() + static_cast<std::ptrdiff_t>(consumed));
    }
    if (inBuf_.size() > 65536) {
        inBuf_.erase(inBuf_.begin(), inBuf_.end() - static_cast<std::ptrdiff_t>(kMp3LookaheadBytes));
    }
}

PcmRingBuffer::PcmRingBuffer(std::size_t maxFrames)
    : samples_(maxFrames * 2, 0), maxFrames_(maxFrames) {}

void PcmRingBuffer::Clear(int sampleRate, int channels) {
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
    sampleRate_.store(sampleRate, std::memory_order_release);
    channels_.store(channels, std::memory_order_release);
}

std::size_t PcmRingBuffer::Push(const std::int16_t* interleaved, std::size_t frames, int sampleRate, int channels) {
    if (frames == 0 || (channels != 1 && channels != 2) || sampleRate <= 0) return 0;

    int curSr = sampleRate_.load(std::memory_order_relaxed);
    int curCh = channels_.load(std::memory_order_relaxed);
    if (curSr == 0 || curCh == 0) {
        sampleRate_.store(sampleRate, std::memory_order_release);
        channels_.store(channels, std::memory_order_release);
        curSr = sampleRate;
        curCh = channels;
    }
    if (curSr != sampleRate || curCh != channels) {
        return 0;
    }

    const std::size_t h = head_.load(std::memory_order_acquire);
    const std::size_t t = tail_.load(std::memory_order_relaxed);
    const std::size_t used = t - h;
    const std::size_t freeFrames = used < maxFrames_ ? (maxFrames_ - used) : 0;
    const std::size_t toWrite = std::min(frames, freeFrames);
    if (toWrite == 0) return 0;

    const std::size_t ch = static_cast<std::size_t>(channels);
    const std::size_t tailIdx = t % maxFrames_;
    const std::size_t first = std::min(toWrite, maxFrames_ - tailIdx);
    std::memcpy(samples_.data() + tailIdx * ch, interleaved, first * ch * sizeof(std::int16_t));
    if (toWrite > first) {
        std::memcpy(samples_.data(), interleaved + first * ch, (toWrite - first) * ch * sizeof(std::int16_t));
    }
    tail_.store(t + toWrite, std::memory_order_release);
    return toWrite;
}

std::size_t PcmRingBuffer::Pop(std::int16_t* out, std::size_t exactFrames, int& outSampleRate, int& outChannels) {
    const std::size_t t = tail_.load(std::memory_order_acquire);
    const std::size_t h = head_.load(std::memory_order_relaxed);
    outSampleRate = sampleRate_.load(std::memory_order_acquire);
    outChannels = channels_.load(std::memory_order_acquire);
    if (outChannels <= 0 || (t - h) < exactFrames) return 0;

    const std::size_t ch = static_cast<std::size_t>(outChannels);
    const std::size_t headIdx = h % maxFrames_;
    const std::size_t first = std::min(exactFrames, maxFrames_ - headIdx);
    std::memcpy(out, samples_.data() + headIdx * ch, first * ch * sizeof(std::int16_t));
    if (exactFrames > first) {
        std::memcpy(out + first * ch, samples_.data(), (exactFrames - first) * ch * sizeof(std::int16_t));
    }
    head_.store(h + exactFrames, std::memory_order_release);
    return exactFrames;
}

bool PcmRingBuffer::PopExact(std::int16_t* out, std::size_t exactFrames, int expectedSampleRate, int expectedChannels,
                             std::size_t& outRemaining) {
    const std::size_t t = tail_.load(std::memory_order_acquire);
    const std::size_t h = head_.load(std::memory_order_relaxed);
    const std::size_t avail = t - h;
    const int sr = sampleRate_.load(std::memory_order_acquire);
    const int ch = channels_.load(std::memory_order_acquire);
    if (sr != expectedSampleRate || ch != expectedChannels || ch <= 0 || avail < exactFrames) {
        outRemaining = avail;
        return false;
    }

    const std::size_t chCount = static_cast<std::size_t>(ch);
    const std::size_t headIdx = h % maxFrames_;
    const std::size_t first = std::min(exactFrames, maxFrames_ - headIdx);
    std::memcpy(out, samples_.data() + headIdx * chCount, first * chCount * sizeof(std::int16_t));
    if (exactFrames > first) {
        std::memcpy(out + first * chCount, samples_.data(), (exactFrames - first) * chCount * sizeof(std::int16_t));
    }
    head_.store(h + exactFrames, std::memory_order_release);
    outRemaining = avail - exactFrames;
    return true;
}

void PcmRingBuffer::FormatAndAvailable(int& outSampleRate, int& outChannels, std::size_t& outFrames) const {
    const std::size_t t = tail_.load(std::memory_order_acquire);
    const std::size_t h = head_.load(std::memory_order_acquire);
    outSampleRate = sampleRate_.load(std::memory_order_acquire);
    outChannels = channels_.load(std::memory_order_acquire);
    outFrames = t >= h ? (t - h) : 0;
}

std::size_t PcmRingBuffer::AvailableFrames() const {
    const std::size_t t = tail_.load(std::memory_order_acquire);
    const std::size_t h = head_.load(std::memory_order_acquire);
    return t >= h ? (t - h) : 0;
}

RadioStreamWorker::~RadioStreamWorker() {
    Stop();
}

void RadioStreamWorker::Start(std::string stationName, std::vector<std::string> candidateUrls) {
    Stop();
    stopRequested_.store(false);
    ring_.Clear();
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        status_.state = RadioState::CONNECTING;
        status_.stationName = stationName;
        status_.nowPlaying.clear();
        status_.statusText = "Connecting...";
        status_.resyncs = 0;
        status_.reconnects = 0;
    }
    worker_ = std::thread(&RadioStreamWorker::Run, this, std::move(stationName), std::move(candidateUrls));
}

void RadioStreamWorker::SetTargetBufferSeconds(float seconds) {
    targetBufferSeconds_.store(std::clamp(seconds, 1.5f, 24.0f), std::memory_order_relaxed);
}

void RadioStreamWorker::Stop() {
    stopRequested_.store(true);
    if (worker_.joinable()) worker_.join();
    ring_.Clear();
    std::lock_guard<std::mutex> lock(statusMutex_);
    status_.state = RadioState::OFF;
    status_.nowPlaying.clear();
    status_.statusText = "Radio off";
    status_.resyncs = 0;
    status_.reconnects = 0;
}

RadioStreamWorker::Status RadioStreamWorker::GetStatus() const {
    std::lock_guard<std::mutex> lock(statusMutex_);
    return status_;
}

void RadioStreamWorker::SetStatus(RadioState state, const std::string& statusText) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    status_.state = state;
    status_.statusText = statusText;
}

void RadioStreamWorker::SetNowPlaying(const std::string& title) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    status_.nowPlaying = SanitizeAsciiDisplay(title, 56);
}

void RadioStreamWorker::SetStationName(const std::string& name) {
    const std::string clean = SanitizeAsciiDisplay(name, 36);
    if (clean.empty()) return;
    std::lock_guard<std::mutex> lock(statusMutex_);
    if (status_.stationName.empty() || status_.stationName == "Custom Stream") {
        status_.stationName = clean;
    }
}

void RadioStreamWorker::Run(std::string stationName, std::vector<std::string> candidateUrls) {
    std::vector<std::string> resolvedUrls;
    for (const std::string& url : candidateUrls) {
        if (stopRequested_.load()) return;
        if (!ParseUrl(url).valid) continue;
        if (LooksLikePlaylistUrl(url)) {
            std::string body;
            if (FetchPlaylistText(url, body, stopRequested_)) {
                const auto entries = ParsePlsPlaylist(body);
                for (const auto& entry : entries) {
                    if (std::find(resolvedUrls.begin(), resolvedUrls.end(), entry.url) == resolvedUrls.end()) {
                        resolvedUrls.push_back(entry.url);
                    }
                    if (!entry.title.empty() && stationName == "Custom Stream") {
                        SetStationName(entry.title);
                    }
                }
            }
        } else {
            if (std::find(resolvedUrls.begin(), resolvedUrls.end(), url) == resolvedUrls.end()) {
                resolvedUrls.push_back(url);
            }
        }
    }

    if (resolvedUrls.empty()) {
        SetStatus(RadioState::FAILED, "No valid stream URL");
        return;
    }

    int failures = 0;
    while (!stopRequested_.load()) {
        bool anyDecoded = false;
        for (const std::string& streamUrl : resolvedUrls) {
            if (stopRequested_.load()) return;
            SetStatus(RadioState::CONNECTING, "Connecting...");
            ring_.Clear();

            IcyDemuxer demuxer(0);
            Mp3StreamDecoder decoder;
            std::size_t rawAudioBytes = 0;
            bool playingAnnounced = false;
            bool abortNonMp3 = false;

            const bool ok = StreamHttpSocketAudio(
                streamUrl, stopRequested_,
                [&](std::size_t metaInt, const std::string& icyName) {
                    demuxer.Reset(metaInt);
                    decoder.Reset();
                    SetStationName(icyName);
                    SetStatus(RadioState::BUFFERING, "Buffering...");
                },
                [&](const std::uint8_t* chunk, std::size_t len) -> bool {
                    if (stopRequested_.load()) return false;
                    demuxer.Feed(
                        chunk, len,
                        [&](const std::uint8_t* audio, std::size_t audioLen) {
                            rawAudioBytes += audioLen;
                            const std::uint64_t prevResyncs = decoder.ResyncCount();
                            decoder.Feed(audio, audioLen, false,
                                [&](const std::int16_t* pcm, std::size_t frames, int hz, int ch) {
                                    anyDecoded = true;
                                    // Never let the live PCM ring exceed the configured target buffer.
                                    const std::size_t sr = static_cast<std::size_t>(hz > 0 ? hz : 44100);
                                    const auto computeMaxFrames = [&]() -> std::size_t {
                                        return std::min<std::size_t>(
                                            ring_.CapacityFrames() - sr,
                                            static_cast<std::size_t>(
                                                targetBufferSeconds_.load(std::memory_order_relaxed) *
                                                static_cast<float>(sr)));
                                    };
                                    std::size_t written = 0;
                                    while (written < frames && !stopRequested_.load()) {
                                        const std::size_t maxRingFrames = computeMaxFrames();
                                        const std::size_t availNow = ring_.AvailableFrames();
                                        const std::size_t headroom = availNow < maxRingFrames
                                            ? (maxRingFrames - availNow)
                                            : 0;
                                        if (headroom == 0) {
                                            std::this_thread::sleep_for(std::chrono::milliseconds(15));
                                            continue;
                                        }
                                        const std::size_t chunkFrames = std::min(frames - written, headroom);
                                        const std::size_t n = ring_.Push(
                                            pcm + written * static_cast<std::size_t>(ch),
                                            chunkFrames, hz, ch);
                                        written += n;
                                        if (written < frames) {
                                            int ringSr = 0, ringCh = 0;
                                            std::size_t ringAvail = 0;
                                            ring_.FormatAndAvailable(ringSr, ringCh, ringAvail);
                                            if (ringSr > 0 && ringCh > 0 && (ringSr != hz || ringCh != ch)) {
                                                break;
                                            }
                                            std::this_thread::sleep_for(std::chrono::milliseconds(15));
                                        }
                                    }
                                    if (!playingAnnounced) {
                                        int srCheck = 0, c = 0;
                                        std::size_t avail = 0;
                                        ring_.FormatAndAvailable(srCheck, c, avail);
                                        if (avail >= kBufferReadyFrames) {
                                            playingAnnounced = true;
                                            SetStatus(RadioState::PLAYING, "LIVE");
                                        }
                                    }
                                });
                            const std::uint64_t newResyncs = decoder.ResyncCount();
                            if (newResyncs > prevResyncs) {
                                std::lock_guard<std::mutex> lock(statusMutex_);
                                status_.resyncs += (newResyncs - prevResyncs);
                            }
                            if (rawAudioBytes > 131072 && decoder.DecodedFrames() == 0) {
                                abortNonMp3 = true;
                            }
                        },
                        [&](const std::string& title) { SetNowPlaying(title); });
                    return !abortNonMp3 && !stopRequested_.load();
                });

            if (stopRequested_.load()) return;
            if (ok && anyDecoded) break;
        }

        if (stopRequested_.load()) return;
        if (!anyDecoded && ++failures >= 3) {
            SetStatus(RadioState::FAILED, "Stream unavailable");
            return;
        }
        {
            std::lock_guard<std::mutex> lock(statusMutex_);
            ++status_.reconnects;
        }
        SetStatus(RadioState::CONNECTING, "Reconnecting...");
        for (int i = 0; i < 15 && !stopRequested_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
