#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct RadioStation {
    std::string name;
    std::string subtitle;
    std::string plsUrl;
    std::string fallbackUrl;
};

struct ParsedUrl {
    bool valid = false;
    std::string scheme;
    std::string host;
    unsigned short port = 0;
    std::string path;
};

struct PlaylistEntry {
    std::string url;
    std::string title;
};

const std::vector<RadioStation>& DefaultRadioStations();

ParsedUrl ParseUrl(std::string_view url);
bool LooksLikePlaylistUrl(std::string_view url);
std::vector<PlaylistEntry> ParsePlsPlaylist(std::string_view text);
std::string ExtractIcyStreamTitle(std::string_view metadata);
std::string SanitizeAsciiDisplay(std::string_view text, std::size_t maxLen = 52);
