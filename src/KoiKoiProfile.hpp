#pragma once

#include <string_view>

// Frozen KK-ADD-10 1.0.0 scoring constants. This is not a runtime rules option.
namespace KoiKoiProfile {
inline constexpr std::string_view Id = "KK-ADD-10";
inline constexpr std::string_view Version = "1.0.0";
inline constexpr int Goko = 10;
inline constexpr int Shiko = 8;
inline constexpr int AmeShiko = 7;
inline constexpr int Sanko = 5;
inline constexpr int FixedSet = 5;
inline constexpr int AnimalThreshold = 5;
inline constexpr int RibbonThreshold = 5;
inline constexpr int PlainThreshold = 10;
inline constexpr int SevenThreshold = 7;
inline constexpr int OpeningPoints = 6;
} // namespace KoiKoiProfile
