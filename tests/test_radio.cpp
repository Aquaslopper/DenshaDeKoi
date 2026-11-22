#include "RadioEngine.hpp"
#include "RadioPlaylist.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::size_t kWholeFrameSamples = 1152; // one MPEG-1 Layer III frame of 44.1 kHz audio

void Check(bool ok, const char* msg) {
    if (!ok) throw std::runtime_error(msg);
}

void TestDefaultStations() {
    const auto& stations = DefaultRadioStations();
    Check(stations.size() == 6, "Expected 6 default J-Pop stations");
    Check(stations[0].name == "J-Pop Powerplay", "First station must be J-Pop Powerplay");
    for (const auto& s : stations) {
        Check(LooksLikePlaylistUrl(s.plsUrl), "Station plsUrl must be a .pls URL");
        Check(ParseUrl(s.fallbackUrl).valid, "Station fallbackUrl must be a valid URL");
    }
}

void TestUrlAndPlaylistParsing() {
    const auto a = ParseUrl("  http://kathy.torontocast.com:3560/stream  ");
    Check(a.valid && a.scheme == "http" && a.host == "kathy.torontocast.com" && a.port == 3560 &&
              a.path == "/stream",
          "Failed to parse a SHOUTcast URL with port");

    const auto b = ParseUrl("https://asiadreamradio.torontocast.stream/stations/x.pls");
    Check(b.valid && b.scheme == "https" && b.port == 443, "Failed to parse an HTTPS .pls URL");
    Check(!ParseUrl("ftp://example.com/test.pls").valid, "Non-HTTP scheme must be rejected");
    Check(!ParseUrl("not a url").valid, "Garbage must be rejected");

    const std::string pls =
        "[playlist]\r\n"
        "numberofentries=2\r\n"
        "File2=http://backup.example.com:8000/stream\r\n"
        "Title2=Backup\r\n"
        "File1=http://kathy.torontocast.com:3560/;\r\n"
        "Title1=J-Pop Powerplay\r\n";
    const auto entries = ParsePlsPlaylist(pls);
    Check(entries.size() == 2 && entries[0].url == "http://kathy.torontocast.com:3560/;" &&
              entries[0].title == "J-Pop Powerplay",
          "PLS parser returned the wrong order or fields");
}

// Builds a stream of real 128 kbps / 44.1 kHz MPEG-1 Layer III frames (silence).
std::vector<std::uint8_t> SilentMp3(std::size_t frames) {
    std::vector<std::uint8_t> out;
    std::vector<std::uint8_t> frame(417, 0);
    frame[0] = 0xFF;
    frame[1] = 0xFB;
    frame[2] = 0x90;
    frame[3] = 0x00;
    for (std::size_t i = 0; i < frames; ++i) out.insert(out.end(), frame.begin(), frame.end());
    return out;
}

void TestMp3Decoder() {
    constexpr std::size_t kFrames = 200;
    const auto stream = SilentMp3(kFrames);
    const std::size_t expected = kWholeFrameSamples * kFrames;

    // Feeding in network-sized chunks must not lose or duplicate audio.
    for (std::size_t chunk : {std::size_t(64), std::size_t(1460), std::size_t(4096)}) {
        Mp3Decoder decoder;
        std::size_t decoded = 0;
        int sampleRate = 0;
        int channels = 0;
        for (std::size_t off = 0; off < stream.size(); off += chunk) {
            const std::size_t n = std::min(chunk, stream.size() - off);
            decoder.Feed(stream.data() + off, n,
                         [&](const std::int16_t*, std::size_t frames, int hz, int ch) {
                             decoded += frames;
                             sampleRate = hz;
                             channels = ch;
                         });
        }
        decoder.Feed(nullptr, 0, [&](const std::int16_t*, std::size_t frames, int, int) { decoded += frames; },
                     /*flush=*/true);
        Check(decoded == expected, "Decoder lost or duplicated PCM across chunk boundaries");
        Check(sampleRate == 44100 && channels == 2, "Decoder reported the wrong stream format");
    }

    // Random bytes must not crash or be mistaken for audio.
    Mp3Decoder decoder;
    std::vector<std::uint8_t> noise(8192, 0x5A);
    std::size_t decoded = 0;
    decoder.Feed(noise.data(), noise.size(),
                 [&](const std::int16_t*, std::size_t frames, int, int) { decoded += frames; });
    Check(decoder.DecodedFrames() == 0 && decoded == 0, "Garbage decoded as audio");
}

void TestPcmQueue() {
    PcmQueue queue(8);
    const std::int16_t frames[12] = {1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6};
    int sampleRate = 0;
    int channels = 0;
    Check(!queue.Format(sampleRate, channels), "A fresh queue must report no format");

    queue.SetFormat(44100, 2);
    Check(queue.Push(frames, 6) == 6, "Push must accept frames that fit");
    Check(queue.Push(frames, 6) == 2, "Push must clamp to the remaining capacity");
    Check(queue.AvailableFrames() == 8, "Queue frame count mismatch");

    std::int16_t out[16]{};
    Check(queue.Pop(out, 4) == 4 && out[0] == 1 && out[7] == -4, "Queue pop returned wrong frames");
    Check(queue.Pop(out, 16) == 4, "Queue pop must return only what is buffered");
    Check(queue.Pop(out, 4) == 0 && queue.AvailableFrames() == 0, "Empty queue must return 0");

    // Wrapping around the end of the ring must keep frame order intact.
    queue.Clear();
    queue.SetFormat(48000, 1);
    const std::int16_t mono[6] = {10, 20, 30, 40, 50, 60};
    Check(queue.Push(mono, 6) == 6, "Mono push failed");
    Check(queue.Pop(out, 2) == 2 && out[0] == 10 && out[1] == 20, "Mono pop order wrong");
    Check(queue.Push(mono, 4) == 4, "Push across the ring end failed");
    Check(queue.Pop(out, 8) == 8, "Wrap-around pop returned the wrong count");
    const std::int16_t expect[8] = {30, 40, 50, 60, 10, 20, 30, 40};
    Check(std::memcmp(out, expect, sizeof(expect)) == 0, "Wrap-around pop order wrong");

    // A format change must drop stale audio of the previous format.
    queue.Clear();
    queue.SetFormat(44100, 2);
    queue.Push(frames, 6);
    queue.SetFormat(48000, 2);
    Check(queue.AvailableFrames() == 0, "Format change must clear stale audio");
}
} // namespace

int main() {
    try {
        TestDefaultStations();
        TestUrlAndPlaylistParsing();
        TestMp3Decoder();
        TestPcmQueue();
        std::cout << "Radio playlist, PCM queue, and MP3 decoder checks passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
