#include "RadioPlaylist.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <utility>

namespace {
std::string Trim(std::string_view sv) {
    std::size_t first = 0;
    while (first < sv.size() && std::isspace(static_cast<unsigned char>(sv[first]))) ++first;
    std::size_t last = sv.size();
    while (last > first && std::isspace(static_cast<unsigned char>(sv[last - 1]))) --last;
    return std::string(sv.substr(first, last - first));
}

std::string ToLower(std::string_view sv) {
    std::string out(sv);
    for (char& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return out;
}

bool StartsWithNoCase(std::string_view s, std::string_view prefix) {
    if (s.size() < prefix.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(s[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

bool EndsWithNoCase(std::string_view s, std::string_view suffix) {
    if (s.size() < suffix.size()) return false;
    return StartsWithNoCase(s.substr(s.size() - suffix.size()), suffix);
}

bool ParseUnsignedInt(std::string_view sv, int& out) {
    if (sv.empty()) return false;
    int value = 0;
    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec != std::errc{} || ptr != sv.data() + sv.size() || value <= 0) return false;
    out = value;
    return true;
}
} // namespace

const std::vector<RadioStation>& DefaultRadioStations() {
    static const std::vector<RadioStation> stations = {
        {
            "J-Pop Powerplay",
            "Asia DREAM Radio",
            "https://asiadreamradio.torontocast.stream/stations/ewExternalFiles/JPopPowerplay1.pls",
            "http://kathy.torontocast.com:3560/stream",
        },
        {
            "J-Pop Powerplay Kawaii",
            "Cute J-Pop & Idols",
            "https://asiadreamradio.torontocast.stream/stations/ewExternalFiles/kawaii1.pls",
            "http://kathy.torontocast.com:3060/stream",
        },
        {
            "J-Pop Sakura",
            "Nostalgic 80s-00s",
            "https://asiadreamradio.torontocast.stream/stations/ewExternalFiles/JPopSakura1.pls",
            "http://quincy.torontocast.com:2070/;",
        },
        {
            "Japan Hits",
            "Tokyo Top Charts",
            "https://asiadreamradio.torontocast.stream/stations/ewExternalFiles/JapanHits1.pls",
            "http://quincy.torontocast.com:2020/",
        },
        {
            "J-Rock Powerplay",
            "J-Rock & Anime",
            "https://asiadreamradio.torontocast.stream/stations/ewExternalFiles/JRockPowerplay1.pls",
            "http://kathy.torontocast.com:3340/stream",
        },
        {
            "Jazz Sakura",
            "Japanese Smooth Jazz",
            "https://asiadreamradio.torontocast.stream/stations/ewExternalFiles/jazzsakura1.pls",
            "http://kathy.torontocast.com:3330/stream",
        },
    };
    return stations;
}

ParsedUrl ParseUrl(std::string_view raw) {
    const std::string trimmed = Trim(raw);
    const std::size_t schemePos = trimmed.find("://");
    if (schemePos == std::string::npos) return {};
    const std::string scheme = ToLower(std::string_view(trimmed).substr(0, schemePos));
    if (scheme != "http" && scheme != "https") return {};

    std::string_view rest = std::string_view(trimmed).substr(schemePos + 3);
    const std::size_t slashPos = rest.find('/');
    const std::size_t queryPos = rest.find('?');
    const std::size_t pathStart = std::min(slashPos, queryPos);
    std::string_view authority = pathStart == std::string_view::npos ? rest : rest.substr(0, pathStart);
    std::string path = "/";
    if (pathStart != std::string_view::npos) {
        if (rest[pathStart] == '/') path = std::string(rest.substr(pathStart));
        else path = "/" + std::string(rest.substr(pathStart));
    }

    // Strip optional userinfo@ if present.
    if (const std::size_t atPos = authority.rfind('@'); atPos != std::string_view::npos) {
        authority = authority.substr(atPos + 1);
    }
    if (authority.empty()) return {};

    std::string host;
    unsigned short port = scheme == "https" ? 443 : 80;
    if (authority.front() == '[') {
        const std::size_t closeBracket = authority.find(']');
        if (closeBracket == std::string_view::npos || closeBracket == 1) return {};
        host = std::string(authority.substr(1, closeBracket - 1));
        const std::string_view after = authority.substr(closeBracket + 1);
        if (!after.empty()) {
            if (after.front() != ':' || after.size() == 1) return {};
            int parsedPort = 0;
            if (!ParseUnsignedInt(after.substr(1), parsedPort) || parsedPort > 65535) return {};
            port = static_cast<unsigned short>(parsedPort);
        }
    } else {
        const std::size_t colonPos = authority.find(':');
        if (colonPos != std::string_view::npos) {
            host = std::string(authority.substr(0, colonPos));
            const std::string_view portStr = authority.substr(colonPos + 1);
            int parsedPort = 0;
            if (portStr.empty() || !ParseUnsignedInt(portStr, parsedPort) || parsedPort > 65535) return {};
            port = static_cast<unsigned short>(parsedPort);
        } else {
            host = std::string(authority);
        }
    }
    if (host.empty()) return {};
    return {true, scheme, host, port, path};
}

bool LooksLikePlaylistUrl(std::string_view url) {
    const ParsedUrl parsed = ParseUrl(url);
    if (!parsed.valid) return false;
    std::string_view path = parsed.path;
    if (const std::size_t q = path.find('?'); q != std::string_view::npos) path = path.substr(0, q);
    return EndsWithNoCase(path, ".pls") || EndsWithNoCase(path, ".m3u") || EndsWithNoCase(path, ".m3u8");
}

std::vector<PlaylistEntry> ParsePlsPlaylist(std::string_view text) {
    std::map<int, std::string> files;
    std::map<int, std::string> titles;
    std::vector<PlaylistEntry> m3uEntries;

    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        const std::string line = Trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (line.empty() || line.front() == '#' || line.front() == ';' || line.front() == '[') continue;

        if (const std::size_t eq = line.find('='); eq != std::string::npos) {
            const std::string key = ToLower(Trim(std::string_view(line).substr(0, eq)));
            const std::string val = Trim(std::string_view(line).substr(eq + 1));
            int index = 0;
            if (StartsWithNoCase(key, "file") && ParseUnsignedInt(std::string_view(key).substr(4), index)) {
                if (ParseUrl(val).valid) files[index] = val;
            } else if (StartsWithNoCase(key, "title") && ParseUnsignedInt(std::string_view(key).substr(5), index)) {
                titles[index] = val;
            }
        } else if (ParseUrl(line).valid) {
            m3uEntries.push_back({line, ""});
        }
    }

    if (!files.empty()) {
        std::vector<PlaylistEntry> result;
        result.reserve(files.size());
        for (const auto& [idx, url] : files) {
            const auto it = titles.find(idx);
            result.push_back({url, it != titles.end() ? it->second : ""});
        }
        return result;
    }
    return m3uEntries;
}

std::string ExtractIcyStreamTitle(std::string_view metadata) {
    constexpr std::string_view prefix = "StreamTitle=";
    std::size_t pos = 0;
    while (pos < metadata.size()) {
        if (StartsWithNoCase(metadata.substr(pos), prefix)) {
            std::size_t start = pos + prefix.size();
            if (start >= metadata.size()) return "";
            const char quote = metadata[start];
            if (quote == '\'' || quote == '"') {
                ++start;
                for (std::size_t end = start; end < metadata.size(); ++end) {
                    if (metadata[end] == quote &&
                        (end + 1 >= metadata.size() || metadata[end + 1] == ';')) {
                        return Trim(metadata.substr(start, end - start));
                    }
                }
                return Trim(metadata.substr(start));
            }
            const std::size_t semi = metadata.find(';', start);
            return Trim(metadata.substr(start, semi == std::string_view::npos ? metadata.size() - start : semi - start));
        }
        ++pos;
    }
    return "";
}

std::string SanitizeAsciiDisplay(std::string_view text, std::size_t maxLen) {
    std::string out;
    out.reserve(std::min(text.size(), maxLen));
    bool lastSpace = true;
    for (std::size_t i = 0; i < text.size() && out.size() < maxLen;) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            if (c >= 0x20 && c <= 0x7E) {
                if (c == ' ') {
                    if (!lastSpace) out.push_back(' ');
                    lastSpace = true;
                } else {
                    out.push_back(static_cast<char>(c));
                    lastSpace = false;
                }
            }
            ++i;
        } else {
            // Skip multi-byte UTF-8 sequence cleanly and emit a placeholder only
            // if we don't already have non-whitespace separator context.
            std::size_t seqLen = 1;
            if ((c & 0xE0) == 0xC0) seqLen = 2;
            else if ((c & 0xF0) == 0xE0) seqLen = 3;
            else if ((c & 0xF8) == 0xF0) seqLen = 4;
            i += std::min(seqLen, text.size() - i);
            if (out.size() < maxLen) {
                out.push_back('?');
                lastSpace = false;
            }
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    if (text.size() > maxLen && out.size() >= 3) {
        out.replace(out.size() - 3, 3, "...");
    }
    return out;
}
