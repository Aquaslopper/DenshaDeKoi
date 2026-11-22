#pragma once

#include <string_view>

// Xeno-koi KK-ADD-10 scoring constants. This is not a runtime rules option.
namespace KoiKoiProfile {
inline constexpr std::string_view Id = "KK-ADD-10";
// Source: Aquaslopper/Xeno-koi main, commit f422bacf2519cdfd1b7582a647015356478ce66b.
inline constexpr std::string_view Version = "xeno-f422bac";
inline constexpr int Goko = 10;
inline constexpr int Shiko = 8;
inline constexpr int AmeShiko = 7;
inline constexpr int Sanko = 5;
inline constexpr int FixedSet = 5;
inline constexpr int AnimalThreshold = 5;
inline constexpr int RibbonThreshold = 4;
inline constexpr int PlainThreshold = 10;
inline constexpr int SevenThreshold = 7;
inline constexpr int OpeningPoints = 6;
inline constexpr int MaxCallerFactor = 6;
} // namespace KoiKoiProfile
