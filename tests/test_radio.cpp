#include "RadioPlaylist.hpp"
#include "RadioStreamWorker.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Check(bool ok, const char* msg) {
    if (!ok) throw std::runtime_error(msg);
}

void TestDefaultStations() {
    const auto& stations = DefaultRadioStations();
    Check(stations.size() == 6, "Expected 6 default J-Pop stations");
    Check(stations[0].name == "J-Pop Powerplay", "First station must be J-Pop Powerplay");
    for (const auto& s : stations) {
        Check(LooksLikePlaylistUrl(s.plsUrl), "Station plsUrl must be a .pls URL");
        Check(ParseUrl(s.fallbackUrl).valid, "Station fallbackUrl must be valid");
    }
}

void TestUrlParser() {
    const auto a = ParseUrl("  http://kathy.torontocast.com:3560/;  ");
    Check(a.valid && a.scheme == "http" && a.host == "kathy.torontocast.com" && a.port == 3560 && a.path == "/;",
          "Failed to parse SHOUTcast URL with port and /; path");

    const auto b = ParseUrl("https://asiadreamradio.torontocast.stream/stations/ewExternalFiles/JPopPowerplay1.pls");
    Check(b.valid && b.scheme == "https" && b.port == 443 &&
          b.path == "/stations/ewExternalFiles/JPopPowerplay1.pls",
          "Failed to parse HTTPS .pls URL");
    Check(LooksLikePlaylistUrl("https://example.com/listen.pls?id=1"), "Query-string .pls URL not detected");
    Check(!LooksLikePlaylistUrl("http://example.com:8000/stream"), "Direct stream misclassified as playlist");

    const auto c = ParseUrl("http://[2001:db8::1]:8000/live");
    Check(c.valid && c.host == "2001:db8::1" && c.port == 8000 && c.path == "/live", "IPv6 URL failed");

    Check(!ParseUrl("ftp://example.com/test.pls").valid, "Non-HTTP scheme must fail");
    Check(!ParseUrl("http://example.com:99999/test").valid, "Out-of-range port must fail");
    Check(!ParseUrl("not a url").valid, "Invalid URL accepted");
}

void TestPlsAndIcyMetadata() {
    const std::string pls =
        "[playlist]\r\n"
        "numberofentries=2\r\n"
        "File2=http://backup.example.com:8000/stream\r\n"
        "Title2=Backup Stream\r\n"
        "File1=http://kathy.torontocast.com:3560/;\r\n"
        "Title1=J-Pop Powerplay\r\n"
        "Length1=-1\r\n"
        "version=2\r\n";
    const auto entries = ParsePlsPlaylist(pls);
    Check(entries.size() == 2 &&
          entries[0].url == "http://kathy.torontocast.com:3560/;" &&
          entries[0].title == "J-Pop Powerplay" &&
          entries[1].url == "http://backup.example.com:8000/stream",
          "PLS parser ordering or fields incorrect");

    const auto m3u = ParsePlsPlaylist("#EXTM3U\n#EXTINF:-1,J-Pop\nhttp://quincy.torontocast.com/JHits\n");
    Check(m3u.size() == 1 && m3u[0].url == "http://quincy.torontocast.com/JHits", "M3U fallback parse failed");

    Check(ExtractIcyStreamTitle("StreamTitle='YOASOBI - Don't Stop';StreamUrl='';") == "YOASOBI - Don't Stop",
          "ICY StreamTitle extraction with apostrophe failed");
    Check(SanitizeAsciiDisplay("  Perfume -   TOKYO GIRL  \xE3\x81\x82 ", 40) == "Perfume - TOKYO GIRL ?",
          "ASCII display sanitization failed");
}

void TestIcyDemuxerAndRingBuffer() {
    // 5 bytes audio + 1 meta len byte (1 => 16 meta bytes) + 16 meta bytes + 4 bytes audio
    std::vector<std::uint8_t> packet = {'A', 'B', 'C', 'D', 'E', 1};
    const std::string meta = "StreamTitle='Ado';"; // 17 chars? Trim to 16: "StreamTitle='X';"
    const std::string meta16 = "StreamTitle='X';";
    Check(meta16.size() == 16, "Test fixture must be 16 bytes");
    packet.insert(packet.end(), meta16.begin(), meta16.end());
    packet.push_back('F');
    packet.push_back('G');
    packet.push_back('H');
    packet.push_back('I');

    IcyDemuxer demux(5);
    std::string audioOut;
    std::vector<std::string> metas;
    // Feed 3 bytes at a time to exercise chunk boundary splitting.
    for (std::size_t off = 0; off < packet.size(); off += 3) {
        const std::size_t n = std::min<std::size_t>(3, packet.size() - off);
        demux.Feed(packet.data() + off, n,
                   [&](const std::uint8_t* a, std::size_t len) {
                       audioOut.append(reinterpret_cast<const char*>(a), len);
                   },
                   [&](const std::string& m) { metas.push_back(m); });
    }
    Check(audioOut == "ABCDEFGHI" && metas.size() == 1 && metas[0] == "X",
          "ICY demuxer corrupted audio or missed metadata across chunk boundaries");

    PcmRingBuffer ring(8);
    const std::int16_t f1[10] = {1, -1, 2, -2, 3, -3, 4, -4, 5, -5};
    const std::int16_t f2[10] = {6, -6, 7, -7, 8, -8, 9, -9, 10, -10};
    ring.Push(f1, 5, 44100, 2);
    ring.Push(f2, 5, 44100, 2); // overwrites oldest 2 frames -> keeps 3..10
    int sr = 0, ch = 0;
    std::size_t avail = 0;
    ring.FormatAndAvailable(sr, ch, avail);
    Check(sr == 44100 && ch == 2 && avail == 8 && ring.AvailableFrames() == 8, "Ring buffer count/format mismatch");
    std::int16_t popped[16]{};
    Check(ring.Pop(popped, 8, sr, ch) == 8 && popped[0] == 3 && popped[1] == -3 &&
          popped[14] == 10 && popped[15] == -10,
          "Ring buffer wraparound pop mismatch");

    ring.Push(f1, 5, 44100, 2);
    std::size_t rem = 99;
    std::int16_t cbOut[8]{};
    Check(!ring.PopExact(cbOut, 6, 44100, 2, rem) && rem == 5,
          "PopExact must reject insufficient frames without consuming");
    Check(!ring.PopExact(cbOut, 4, 48000, 2, rem) && rem == 5,
          "PopExact must reject sample rate mismatch");
    Check(ring.PopExact(cbOut, 4, 44100, 2, rem) && rem == 1 && cbOut[0] == 1 && cbOut[7] == -4,
          "PopExact failed on valid read");

    Mp3StreamDecoder decoder;
    std::vector<std::uint8_t> noise(4096, 0);
    decoder.Feed(noise.data(), noise.size(), true,
                 [](const std::int16_t*, std::size_t, int, int) {});
    Check(decoder.DecodedFrames() == 0, "Zeroed bytes must not produce valid MP3 frames");
}
} // namespace

int main() {
    try {
        TestDefaultStations();
        TestUrlParser();
        TestPlsAndIcyMetadata();
        TestIcyDemuxerAndRingBuffer();
        std::cout << "Radio playlist, ICY demuxer, and stream buffer checks passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
