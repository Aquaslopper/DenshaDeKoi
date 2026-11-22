#include "TableLayout.hpp"
#include "TableGame.hpp"
#include "TableScene.hpp"
#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Vertex { float x, y, z, u, v; Color color; unsigned int texture; };
std::vector<Vertex> vertices;
std::vector<unsigned int> unloaded;
std::vector<Rectangle> destinations;
std::vector<Rectangle> sources;
Camera3D camera{};
Color color{};
float u = 0, v = 0;
unsigned int textureId = 0;
int width = 1600, height = 900;
int loads = 0, renderUnloads = 0;
int draws = 0;
std::vector<std::string> drawnTexts;
bool failAtlas = false, invalidAtlas = false, failCanvas = false;

void Check(bool condition, const char* text) { if (!condition) throw std::runtime_error(text); }
bool Near(float a, float b, float tolerance = 0.01f) { return std::abs(a - b) < tolerance; }
void Reset() {
    vertices.clear(); unloaded.clear(); destinations.clear(); sources.clear();
    drawnTexts.clear();
    loads = renderUnloads = draws = 0; failAtlas = invalidAtlas = failCanvas = false;
}
void TestGeometry() {
    using namespace TableLayout;
    const auto a = Project(OnPlane(0, 0));
    const auto b = Project(OnPlane(PlanWidth, 0));
    const auto c = Project(OnPlane(0, PlanHeight));
    const auto d = Project(OnPlane(PlanWidth, PlanHeight));
    Check(Near(a.x, 270.4f, 2) && Near(a.y, 284.0f, 2) && Near(b.x, 1089.6f, 2) &&
          Near(c.x, -146.2f, 2) && Near(c.y, 745.0f, 2) && Near(d.x, 1506.2f, 2),
          "Plane projection does not align with the baked table");
    const auto within = [](Rect r) { return r.x >= 0 && r.y >= 0 && r.x + r.width <= PlanWidth &&
                                           r.y + r.height <= PlanHeight; };
    Check(MatchRounds == 3 && within(Stock) && within(OpponentCapture) && within(PlayerCapture),
          "Wrong rounds, stock, or capture position");
    for (auto r : OpponentHand) Check(within(r), "Opponent hand outside plane");
    for (auto r : PlayerHand) Check(within(r), "Player hand outside plane");
    for (auto r : Field) Check(within(r), "Field slot outside plane");
    for (std::size_t i = 0; i < FieldSlotCount; ++i) Check(within(FieldSlot(i, true)), "Compact field slot outside plane");

    std::vector<Rect> allSlots{Stock, OpponentCapture, PlayerCapture};
    allSlots.insert(allSlots.end(), OpponentHand.begin(), OpponentHand.end());
    allSlots.insert(allSlots.end(), PlayerHand.begin(), PlayerHand.end());
    allSlots.insert(allSlots.end(), Field.begin(), Field.end());
    const auto separated = [](Rect x, Rect y) {
        return x.x + x.width + 5.0f <= y.x || y.x + y.width + 5.0f <= x.x ||
               x.y + x.height + 5.0f <= y.y || y.y + y.height + 5.0f <= x.y;
    };
    for (std::size_t i = 0; i < allSlots.size(); ++i)
        for (std::size_t j = i + 1; j < allSlots.size(); ++j)
            Check(separated(allSlots[i], allSlots[j]), "Card slots must not touch or overlap");
}
void TestDraw() {
    Reset();
    {
        TableScene scene("city", "atlas");
        TableGame game(42);
        scene.Draw(game);
        const auto cards = game.Cards();
        std::size_t expectedShadows = 0;
        for (const auto& card : cards) if (card.shadow) ++expectedShadows;
        const std::size_t shadowVertices = expectedShadows * 4;
        const std::size_t cardVertices = cards.size() * 24;
        Check(vertices.size() == shadowVertices + cardVertices, "Card render count differs from game state");
        for (std::size_t i = 0; i < expectedShadows; ++i) {
            for (std::size_t j = 0; j < 4; ++j) {
                const auto& shadowVert = vertices[i * 4 + j];
                Check(shadowVert.texture == 99 && shadowVert.y < TableLayout::CardElevation &&
                      shadowVert.color.r == 0 && shadowVert.color.g == 0 && shadowVert.color.b == 0 &&
                      shadowVert.color.a > 0 && shadowVert.color.a < 255,
                      "Card shadow must sit below the card plane");
            }
        }
        for (std::size_t i = 0; i < cards.size(); ++i) {
            const auto& card = cards[i];
            const std::size_t base = shadowVertices + i * 24;
            for (std::size_t j = 0; j < 4; ++j) {
                const auto& vert = vertices[base + j];
                Check(Near(vert.y, card.elevation + card.thickness) && vert.color.a == 255,
                      "Card top face is not at card thickness height");
                if (card.id < 0) Check(vert.texture == 4 && vert.color.r == 255 && vert.color.g == 255 && vert.color.b == 255,
                                       "Card backs must use the matte-black back texture untinted");
                else Check(vert.texture == 2 && vert.color.r == 255 && vert.color.g == 255 && vert.color.b == 255,
                           "Card faces must use the atlas untinted");
            }
            for (std::size_t j = 4; j < 8; ++j) {
                const auto& bottomVert = vertices[base + j];
                Check(Near(bottomVert.y, card.elevation) && bottomVert.texture == 4 &&
                      bottomVert.color.r == 255 && bottomVert.color.g == 255 && bottomVert.color.b == 255 &&
                      bottomVert.color.a == 255,
                      "Card bottom face must use the matte-black back texture");
            }
            for (std::size_t j = 8; j < 24; ++j) {
                const auto& sideVert = vertices[base + j];
                Check(sideVert.texture == 99 && sideVert.color.a == 255 &&
                      sideVert.y >= card.elevation - 0.0001f &&
                      sideVert.y <= card.elevation + card.thickness + 0.0001f,
                      "Card 3D side edge vertices out of bounds");
            }
            if (card.id >= 0) {
                const auto& topLeft = vertices[base];
                const auto& bottomLeft = vertices[base + 1];
                const auto& bottomRight = vertices[base + 2];
                const auto& topRight = vertices[base + 3];
                const float expectedU = static_cast<float>(card.id / 4) / 12.0f;
                const float expectedV = static_cast<float>(card.id % 4) / 4.0f;
                Check(Near(topLeft.u, expectedU) && Near(topLeft.v, expectedV) &&
                      Near(bottomLeft.u, expectedU) && Near(bottomLeft.v, expectedV + 0.25f) &&
                      Near(bottomRight.u, expectedU + 1.0f / 12.0f) && Near(bottomRight.v, expectedV + 0.25f) &&
                      Near(topRight.u, expectedU + 1.0f / 12.0f) && Near(topRight.v, expectedV),
                      "Card face UVs must use the complete original atlas cell");
                if (Near(card.yaw, 0.0f)) {
                    Check(bottomLeft.z > topLeft.z && bottomRight.x > bottomLeft.x && topRight.x > topLeft.x,
                          "Card quad winding or orientation is wrong");
                }
            }
        }
        Check(camera.projection == CAMERA_PERSPECTIVE && Near(camera.fovy, 45), "Wrong 3D camera");
        Check(destinations.size() == 2 && Near(destinations[1].height, 900) &&
              Near(destinations[1].width, 1360.0f * 900.0f / 768.0f) && sources[1].height < 0,
              "Window resize must preserve aspect ratio and flip render target");

        // Verify right-side pause button stays clear of decision banners and renders pause menu radio presets.
        const auto noOverlap = [](TableLayout::Rect a, TableLayout::Rect b) {
            return a.x + a.width <= b.x || b.x + b.width <= a.x ||
                   a.y + a.height <= b.y || b.y + b.height <= a.y;
        };
        Check(TableHud::PauseButton.x >= 1200.0f &&
              noOverlap(TableHud::PauseButton, TableHud::KoiKoiButton) &&
              noOverlap(TableHud::PauseButton, TableHud::ShobuButton) &&
              noOverlap(TableHud::PauseButton, TableHud::RoundEndButton),
              "Right-side pause button must not overlap top-right decision banners");

        drawnTexts.clear();
        TableHudState pausedHud{};
        pausedHud.paused = true;
        pausedHud.radio.enabled = true;
        pausedHud.radio.state = RadioState::PLAYING;
        pausedHud.radio.stationIndex = 0;
        pausedHud.radio.stationName = "J-Pop Powerplay";
        pausedHud.radio.nowPlaying = "YOASOBI - Idol";
        pausedHud.radio.statusText = "LIVE";
        pausedHud.radio.volume = 0.75f;
        scene.Draw(game, pausedHud);
        const auto hasText = [&](const std::string& needle) {
            return std::any_of(drawnTexts.begin(), drawnTexts.end(),
                               [&](const std::string& s) { return s.find(needle) != std::string::npos; });
        };
        Check(hasText("PAUSED") && hasText("J-Pop Powerplay") && hasText("YOASOBI - Idol") && hasText("75%"),
              "Pause menu must render radio state, station presets, and track title");
    }
    Check(renderUnloads == 1 && unloaded == std::vector<unsigned int>({4, 2, 1}), "Resources not released");
}
void TestFailures() {
    Reset(); failAtlas = true;
    bool threw = false;
    try { TableScene scene("city", "atlas"); } catch (const std::runtime_error&) { threw = true; }
    Check(threw && unloaded == std::vector<unsigned int>{1}, "Atlas failure or background cleanup failed");
    Reset(); invalidAtlas = true;
    threw = false;
    try { TableScene scene("city", "atlas"); } catch (const std::runtime_error&) { threw = true; }
    Check(threw && unloaded == std::vector<unsigned int>({2, 1}), "Invalid atlas or texture cleanup failed");
    Reset(); failCanvas = true;
    threw = false;
    try { TableScene scene("city", "atlas"); } catch (const std::runtime_error&) { threw = true; }
    Check(threw && unloaded == std::vector<unsigned int>({4, 2, 1}) && renderUnloads == 0, "Canvas failure or texture cleanup failed");
}
} // namespace

Texture2D LoadTexture(const char*) {
    ++loads;
    if (loads == 2 && failAtlas) return {};
    if (loads == 1) return {1, 1360, 768, 1, 0};
    return {2, invalidAtlas ? 2147 : 2148, 1056, 1, 0};
}
Texture2D LoadTextureFromImage(Image img) {
    Check(img.data != nullptr && img.width == 179 && img.height == 264, "Invalid matte back image");
    return {4, img.width, img.height, 1, img.format};
}
void UnloadTexture(Texture2D t) { unloaded.push_back(t.id); }
void SetTextureFilter(Texture2D, int) {}
RenderTexture2D LoadRenderTexture(int w, int h) { return failCanvas ? RenderTexture2D{} : RenderTexture2D{3, {4,w,h,1,0}, {}}; }
void UnloadRenderTexture(RenderTexture2D) { ++renderUnloads; }
void BeginTextureMode(RenderTexture2D) {}
void EndTextureMode() {}
void BeginMode3D(Camera3D c) { camera = c; }
void EndMode3D() {}
void ClearBackground(Color) {}
void DrawText(const char* text, int, int, int, Color) { if (text) drawnTexts.emplace_back(text); }
void DrawRectangle(int, int, int, int, Color) {}
void DrawRectangleLinesEx(Rectangle, float, Color) {}
int GetScreenWidth() { return width; }
int GetScreenHeight() { return height; }
void BeginDrawing() {}
void EndDrawing() {}
void DrawTexturePro(Texture2D, Rectangle src, Rectangle dst, Vector2, float, Color) { sources.push_back(src); destinations.push_back(dst); }
unsigned int rlGetTextureIdDefault() { return 99; }
void rlSetTexture(unsigned int id) { textureId = id; }
void rlBegin(int) {}
void rlEnd() {}
void rlColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a) { color = {r,g,b,a}; }
void rlTexCoord2f(float x, float y) { u = x; v = y; }
void rlVertex3f(float x, float y, float z) { vertices.push_back({x,y,z,u,v,color,textureId}); }

int main() {
    try { TestGeometry(); TestDraw(); TestFailures(); std::cout << "Table layout and rendering commands passed.\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
