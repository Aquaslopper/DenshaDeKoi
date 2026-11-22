#include "AtlasLayout.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::uint32_t ReadBigEndian32(const unsigned char* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) |
           static_cast<std::uint32_t>(bytes[3]);
}

void VerifyAtlas(const std::filesystem::path& path) {
    Check(std::filesystem::is_regular_file(path), "Atlas file does not exist");
    std::ifstream file(path, std::ios::binary);
    std::array<unsigned char, 24> header{};
    file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    Check(file.gcount() == static_cast<std::streamsize>(header.size()), "Atlas PNG header is truncated");
    constexpr std::array<unsigned char, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
    for (std::size_t i = 0; i < signature.size(); ++i) {
        Check(header[i] == signature[i], "Atlas is not a PNG file");
    }
    Check(ReadBigEndian32(header.data() + 8) == 13 && header[12] == 'I' &&
          header[13] == 'H' && header[14] == 'D' && header[15] == 'R',
          "Atlas PNG has an invalid IHDR");
    const auto width = ReadBigEndian32(header.data() + 16);
    const auto height = ReadBigEndian32(header.data() + 20);
    Check(width > 0 && height > 0 && width % AtlasLayout::MonthCount == 0 &&
          height % AtlasLayout::CardsPerMonth == 0, "Atlas is not an even 12x4 grid");
    Check(width == 2148 && height == 1056, "Unexpected normalized atlas dimensions");

    for (int month = 1; month <= AtlasLayout::MonthCount; ++month) {
        for (int index = 0; index < AtlasLayout::CardsPerMonth; ++index) {
            const auto rect = AtlasLayout::GetCardSourceRect(static_cast<float>(width),
                                                              static_cast<float>(height), month, index);
            const float u0 = rect.x / width;
            const float v0 = rect.y / height;
            const float u1 = (rect.x + rect.width) / width;
            const float v1 = (rect.y + rect.height) / height;
            Check(u0 >= 0.0f && v0 >= 0.0f && u1 <= 1.0f && v1 <= 1.0f &&
                  u0 < u1 && v0 < v1, "Card UV bounds are invalid");
            Check(rect.x == (month - 1) * 179.0f && rect.y == index * 264.0f &&
                  rect.width == 179.0f && rect.height == 264.0f,
                  "Card source rectangle does not match its grid cell");
        }
    }
    for (const auto& invalid : std::array<std::array<int, 2>, 4>{{{0, 0}, {13, 0}, {1, -1}, {1, 4}}}) {
        bool rejected = false;
        try {
            (void)AtlasLayout::GetCardSourceRect(static_cast<float>(width), static_cast<float>(height),
                                                 invalid[0], invalid[1]);
        } catch (const std::out_of_range&) {
            rejected = true;
        }
        Check(rejected, "Invalid card coordinates were accepted");
    }
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: test_atlas_math <path-to-cards_atlas.png>\n";
        return 1;
    }
    try {
        VerifyAtlas(argv[1]);
        std::cout << "Verified atlas and all 48 card rectangles.\n";
    } catch (const std::exception& error) {
        std::cerr << "Atlas verification failed: " << error.what() << '\n';
        return 1;
    }
}
