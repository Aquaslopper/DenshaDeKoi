#include <raylib.h>

#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int InspectorMain(int argc, char* argv[]);

namespace {
struct DrawnCard { Rectangle source; Rectangle destination; };
struct Frame {
    Color background{};
    std::vector<DrawnCard> cards;
    std::vector<Rectangle> borders;
    std::vector<std::string> texts;
};
std::vector<Frame> frames;
std::string loadedPath;
int textureWidth = 2148;
int textureHeight = 1056;
int unloadCount = 0;
int targetFps = 0;
bool windowOpen = false;
bool unloadedBeforeClose = false;
constexpr int FrameLimit = 3;

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool Near(float a, float b) { return std::abs(a - b) < 0.01f; }

void Reset(int width = 2148, int height = 1056) {
    frames.clear();
    loadedPath.clear();
    textureWidth = width;
    textureHeight = height;
    unloadCount = 0;
    targetFps = 0;
    windowOpen = false;
    unloadedBeforeClose = false;
}

int Run(const char* overridePath = nullptr) {
    char name[] = "inspector";
    std::string path = overridePath ? overridePath : "";
    std::array<char*, 3> args{name, path.data(), nullptr};
    return InspectorMain(overridePath ? 2 : 1, args.data());
}

void TestGrid() {
    Reset();
    Check(Run() == 0, "Inspector failed");
    Check(loadedPath == (std::filesystem::path("fake_app_dir") / "cards_atlas.png").string(),
          "Default atlas path was not resolved beside the application");
    Check(targetFps == 60 && unloadCount == 1 && unloadedBeforeClose, "Incorrect lifecycle or FPS");
    Check(frames.size() == FrameLimit, "Unexpected frame count");
    for (const auto& frame : frames) {
        Check(frame.background.r == 0x1a && frame.background.g == 0x1c &&
              frame.background.b == 0x23, "Incorrect background");
        Check(frame.cards.size() == 48, "Frame does not contain 48 cards");
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 12; ++col) {
                const auto& card = frame.cards[row * 12 + col];
                const auto& first = frame.cards.front().destination;
                Check(Near(card.source.x, col * 179.0f) && Near(card.source.y, row * 264.0f) &&
                      Near(card.source.width, 179.0f) && Near(card.source.height, 264.0f),
                      "Wrong source cell");
                Check(Near(card.destination.x, first.x + col * (first.width + 10.0f)) &&
                      Near(card.destination.y, first.y + row * (first.height + 10.0f)) &&
                      Near(card.destination.width, first.width) && Near(card.destination.height, first.height),
                      "Card grid is not evenly spaced");
                Check(card.destination.x >= 0 && card.destination.y >= 0 &&
                      card.destination.x + card.destination.width <= 1280 &&
                      card.destination.y + card.destination.height < 683,
                      "Card grid extends outside its available area");
            }
        }
    }
    Check(frames[0].borders.size() == 1 && frames[0].texts.at(0) == "Month: 1   Card Index: 0",
          "First-card hover failed");
    Check(frames[1].borders.size() == 1 && frames[1].texts.at(0) == "Month: 12   Card Index: 3",
          "Last-card hover failed");
    Check(frames[2].borders.empty() && frames[2].texts.at(0) == "Hover over a card to inspect it",
          "Hover in the gap selected a card");
    for (int i = 0; i < 2; ++i) {
        const auto& border = frames[i].borders.front();
        const auto& card = frames[i].cards[i == 0 ? 0 : 47].destination;
        Check(Near(border.x, card.x - 3.0f) && Near(border.y, card.y - 3.0f) &&
              Near(border.width, card.width + 6.0f) && Near(border.height, card.height + 6.0f),
              "Hover outline must not cover the card's original border");
    }
}

void TestLoading() {
    Reset();
    Check(Run("custom/atlas.png") == 0 && loadedPath == "custom/atlas.png",
          "Explicit atlas path was not honored");
    Reset(2268, 1190); // The original image has headings and gutters, not a packed 12x4 grid.
    Check(Run() != 0 && unloadCount == 1 && unloadedBeforeClose,
          "Malformed atlas was not rejected and unloaded");
    Check(frames[0].cards.empty() && !frames[0].texts.empty(), "Load failure was not displayed");
    Reset(0, 0);
    Check(Run() != 0 && unloadCount == 0, "Missing texture was not rejected");
}
} // namespace

void InitWindow(int width, int height, const char*) {
    Check(width == 1280 && height == 720, "Wrong window size");
    windowOpen = true;
}
bool IsWindowReady() { return windowOpen; }
void CloseWindow() { unloadedBeforeClose = unloadCount > 0; windowOpen = false; }
void SetTargetFPS(int fps) { targetFps = fps; }
bool WindowShouldClose() { return frames.size() >= FrameLimit; }
Vector2 GetMousePosition() {
    if (frames.size() == 0) return {35, 80};
    if (frames.size() == 1) return {1200, 600};
    return {125, 80}; // gap between the first two cards
}
void BeginDrawing() { frames.emplace_back(); }
void EndDrawing() {}
void ClearBackground(Color color) { frames.back().background = color; }
bool CheckCollisionPointRec(Vector2 point, Rectangle r) {
    return point.x >= r.x && point.x < r.x + r.width && point.y >= r.y && point.y < r.y + r.height;
}
void DrawRectangleLinesEx(Rectangle rect, float thickness, Color color) {
    Check(Near(thickness, 2.0f) && color.r == 255 && color.g == 255 && color.b == 255,
          "Hover border is not white or has incorrect thickness");
    frames.back().borders.push_back(rect);
}
void DrawText(const char* text, int, int, int, Color) { frames.back().texts.emplace_back(text); }
const char* TextFormat(const char* format, ...) {
    static char text[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return text;
}
const char* GetApplicationDirectory() { return "fake_app_dir"; }
Texture2D LoadTexture(const char* fileName) {
    loadedPath = fileName;
    return {textureWidth > 0 && textureHeight > 0 ? 1u : 0u, textureWidth, textureHeight, 1, 0};
}
void UnloadTexture(Texture2D) { Check(windowOpen, "Texture unloaded after window closed"); ++unloadCount; }
void SetTextureFilter(Texture2D, int filter) { Check(filter == TEXTURE_FILTER_POINT, "Wrong filter"); }
void DrawTexturePro(Texture2D, Rectangle source, Rectangle destination, Vector2, float, Color) {
    frames.back().cards.push_back({source, destination});
}

int main() {
    try {
        TestGrid();
        TestLoading();
        std::cout << "Inspector layout, hover, asset selection and cleanup checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Inspector test failed: " << error.what() << '\n';
        return 1;
    }
}
