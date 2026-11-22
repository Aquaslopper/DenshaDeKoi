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
bool failAtlas = false, invalidAtlas = false, failCanvas = false;

void Check(bool condition, const char* text) { if (!condition) throw std::runtime_error(text); }
bool Near(float a, float b, float tolerance = 0.01f) { return std::abs(a - b) < tolerance; }
void Reset() {
    vertices.clear(); unloaded.clear(); destinations.clear(); sources.clear();
    loads = renderUnloads = 0; failAtlas = invalidAtlas = failCanvas = false;
}
void TestGeometry() {
    using namespace TableLayout;
    const auto a = Project(OnPlane(0, 0));
    const auto b = Project(OnPlane(PlanWidth, 0));
    const auto c = Project(OnPlane(0, PlanHeight));
    const auto d = Project(OnPlane(PlanWidth, PlanHeight));
    Check(Near(a.x, 392.5f, 2) && Near(a.y, 277.9f, 2) && Near(b.x, 967.5f, 2) &&
          Near(c.x, 104.3f, 2) && Near(c.y, 714.0f, 2) && Near(d.x, 1255.7f, 2),
          "Plane projection does not align with the baked table");
    const auto within = [](Rect r) { return r.x >= 0 && r.y >= 0 && r.x + r.width <= PlanWidth &&
                                           r.y + r.height <= PlanHeight; };
    Check(MatchRounds == 3 && within(Stock), "Wrong rounds or stock position");
    for (auto r : OpponentHand) Check(within(r), "Opponent hand outside plane");
    for (auto r : PlayerHand) Check(within(r), "Player hand outside plane");
    for (auto r : Field) Check(within(r), "Field slot outside plane");

}
void TestDraw() {
    Reset();
    {
        TableScene scene("city", "atlas");
        TableGame game(42);
        scene.Draw(game);
        const auto cards = game.Cards();
        Check(vertices.size() == cards.size() * 4, "Card render count differs from game state");
        for (std::size_t i = 0; i < cards.size(); ++i) {
            const auto& card = cards[i];
            for (std::size_t j = 0; j < 4; ++j) {
                const auto& vert = vertices[i * 4 + j];
                Check(Near(vert.y, card.elevation) && vert.color.a == 255, "Card is not on its plane");
                if (card.id < 0) Check(vert.texture == 99 && vert.color.r == 0 && vert.color.g == 0 && vert.color.b == 0,
                                       "Card backs must be solid black");
                else Check(vert.texture == 2 && vert.color.r == 255 && vert.color.g == 255 && vert.color.b == 255,
                           "Card faces must use the atlas untinted");
            }
            if (card.id >= 0) {
                const auto& topLeft = vertices[i * 4];
                const auto& bottomLeft = vertices[i * 4 + 1];
                const auto& bottomRight = vertices[i * 4 + 2];
                const auto& topRight = vertices[i * 4 + 3];
                const float expectedU = static_cast<float>(card.id / 4) / 12.0f;
                const float expectedV = static_cast<float>(card.id % 4) / 4.0f;
                Check(Near(topLeft.u, expectedU) && Near(topLeft.v, expectedV) &&
                      Near(bottomLeft.u, expectedU) && Near(bottomLeft.v, expectedV + 0.25f) &&
                      Near(bottomRight.u, expectedU + 1.0f / 12.0f) && Near(bottomRight.v, expectedV + 0.25f) &&
                      Near(topRight.u, expectedU + 1.0f / 12.0f) && Near(topRight.v, expectedV),
                      "Card face UVs must use the complete original atlas cell");
                Check(bottomLeft.z > topLeft.z && bottomRight.x > bottomLeft.x && topRight.x > topLeft.x,
                      "Card quad winding or orientation is wrong");
            }
        }
        Check(camera.projection == CAMERA_PERSPECTIVE && Near(camera.fovy, 45), "Wrong 3D camera");
        Check(destinations.size() == 2 && Near(destinations[1].height, 900) &&
              Near(destinations[1].width, 1360.0f * 900.0f / 768.0f) && sources[1].height < 0,
              "Window resize must preserve aspect ratio and flip render target");
    }
    Check(renderUnloads == 1 && unloaded == std::vector<unsigned int>({2, 1}), "Resources not released");
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
    Check(threw && unloaded == std::vector<unsigned int>({2, 1}) && renderUnloads == 0, "Canvas failure or texture cleanup failed");
}
} // namespace

Texture2D LoadTexture(const char*) {
    ++loads;
    if (loads == 2 && failAtlas) return {};
    if (loads == 1) return {1, 1360, 768, 1, 0};
    return {2, invalidAtlas ? 2147 : 2148, 1056, 1, 0};
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
void DrawText(const char*, int, int, int, Color) {}
void DrawRectangle(int, int, int, int, Color) {}
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
