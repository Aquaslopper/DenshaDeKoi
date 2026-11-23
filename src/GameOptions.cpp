#include "GameOptions.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <tuple>
#include <vector>

Resolution StepResolution(Resolution current, Resolution desktop, int direction) {
    constexpr std::array<Resolution, 8> presets{{{960, 540}, {1280, 720}, {1360, 768}, {1600, 900},
                                                 {1920, 1080}, {2560, 1440}, {3200, 1800}, {3840, 2160}}};
    const auto less = [](Resolution a, Resolution b) {
        return std::tie(a.width, a.height) < std::tie(b.width, b.height);
    };
    if (desktop.width <= 0 || desktop.height <= 0 || direction == 0) return current;
    std::vector<Resolution> sizes;
    for (auto r : presets)
        if (r.width <= desktop.width && r.height <= desktop.height && r != desktop) sizes.push_back(r);
    sizes.push_back(desktop);
    if (direction > 0) {
        const auto next = std::upper_bound(sizes.begin(), sizes.end(), current, less);
        return next == sizes.end() ? sizes.front() : *next;
    }
    const auto next = std::lower_bound(sizes.begin(), sizes.end(), current, less);
    return next == sizes.begin() ? sizes.back() : *std::prev(next);
}

std::filesystem::path OptionsFile() {
    std::filesystem::path root;
#ifdef _WIN32
    if (const wchar_t* dir = _wgetenv(L"APPDATA"); dir && *dir) root = dir;
#elif defined(__APPLE__)
    if (const char* dir = std::getenv("HOME"); dir && *dir) root = std::filesystem::path(dir) / "Library/Application Support";
#else
    if (const char* dir = std::getenv("XDG_CONFIG_HOME"); dir && *dir) root = dir;
    else if (const char* home = std::getenv("HOME"); home && *home) root = std::filesystem::path(home) / ".config";
#endif
    return root.empty() ? root : root / "DenshaDeKoi" / "options.txt";
}

GameOptions LoadOptions(const std::filesystem::path& file) {
    std::ifstream in(file);
    GameOptions opt;
    std::string version;
    int mode;
    if (!(in >> version >> opt.masterVolume >> opt.sfxVolume >> opt.cardHints
             >> opt.resolution.width >> opt.resolution.height >> mode >> opt.aaSamples) ||
        version != "DDK-OPTIONS-1" || opt.masterVolume < 0 || opt.masterVolume > 100 ||
        opt.sfxVolume < 0 || opt.sfxVolume > 100 || mode < 0 || mode > 2 ||
        opt.resolution.width < 680 || opt.resolution.width > 16384 ||
        opt.resolution.height < 384 || opt.resolution.height > 16384 ||
        opt.aaSamples < 1 || opt.aaSamples > 4) return {};
    opt.screenMode = static_cast<ScreenMode>(mode);
    return opt;
}

void SaveOptions(const std::filesystem::path& file, const GameOptions& opt) {
    if (file.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file);
    out << "DDK-OPTIONS-1\n" << opt.masterVolume << ' ' << opt.sfxVolume << ' ' << opt.cardHints << '\n'
        << opt.resolution.width << ' ' << opt.resolution.height << ' ' << static_cast<int>(opt.screenMode)
        << ' ' << opt.aaSamples << '\n';
    out.close();
    if (out.fail()) std::fprintf(stderr, "[options] Could not save settings.\n");
}
