#include "TableLayout.hpp"
#include "TableGame.hpp"
#include "TableScene.hpp"
#include "ui/PauseMenu.hpp"
#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

void TestOptions();
void TestMatchSetup();

namespace {
struct Vertex { float x, y, z, u, v; Color color; unsigned int texture; };
std::vector<Vertex> vertices;
std::vector<unsigned int> unloaded;
std::vector<unsigned int> unloadedFonts;
std::vector<Rectangle> destinations;
std::vector<Rectangle> sources;
std::vector<unsigned int> blits;
std::vector<Resolution> renderSizes;
int hintEdges = 0;
Camera3D camera{};
Color color{};
float u = 0, v = 0;
unsigned int textureId = 0;
int width = 1600, height = 900;
int loads = 0, renderUnloads = 0;
int draws = 0;
std::vector<std::string> drawnTexts;
bool failAtlas = false, invalidAtlas = false, failCanvas = false, failStripe = false;

void Check(bool condition, const char* text) { if (!condition) throw std::runtime_error(text); }
bool Near(float a, float b, float tolerance = 0.01f) { return std::abs(a - b) < tolerance; }
void Reset() {
    vertices.clear(); unloaded.clear(); unloadedFonts.clear(); destinations.clear(); sources.clear();
    drawnTexts.clear(); blits.clear(); renderSizes.clear(); hintEdges = 0;
    loads = renderUnloads = draws = 0; failAtlas = invalidAtlas = failCanvas = failStripe = false;
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
        TableScene scene("city", "atlas", "logo", "heading", "body", "options");
        TableGame game(42u);
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
        pausedHud.overlay = TableOverlay::RADIO;
        pausedHud.radio.enabled = true;
        pausedHud.radio.state = RadioState::PLAYING;
        pausedHud.radio.stationIndex = 0;
        pausedHud.radio.stationName = "J-Pop Powerplay";
        pausedHud.radio.statusText = "Live";
        pausedHud.radio.volume = 0.75f;
        pausedHud.radio.bufferedSec = 2.8f;
        scene.Draw(game, pausedHud);
        const auto hasText = [&](const std::string& needle) {
            return std::any_of(drawnTexts.begin(), drawnTexts.end(),
                               [&](const std::string& s) { return s.find(needle) != std::string::npos; });
        };
        Check(hasText("PAUSED") && hasText("J-Pop Powerplay") && hasText("75%") &&
              hasText("Live") && hasText("BUFFER 2.8s") && hasText("J-Pop Sakura"),
              "Pause menu must render the radio state, station presets, volume and buffer readout");
        Check(noOverlap(TableHud::VolumeHitBox, TableHud::StationButton(0)) &&
              noOverlap(TableHud::RadioPowerButton, TableHud::RadioMuteButton) &&
              noOverlap(TableHud::VolumeHitBox, TableHud::RadioMuteButton),
              "Radio controls must not overlap each other or the station grid");
    }
    Check(renderUnloads == 1 && unloaded == std::vector<unsigned int>({9, 8, 4, 5, 2, 1}) &&
          unloadedFonts == std::vector<unsigned int>({7, 6}), "Resources not released");
}
void TestPauseRendering() {
    Reset();
    TableScene scene("city", "atlas", "logo", "heading", "body", "options");
    TableGame game; // Pause during the two-card dealer prompt.
    TableHudState hud;
    hud.overlay = TableOverlay::PAUSE;
    hud.options.masterVolume = 43;
    hud.options.sfxVolume = 67;
    hud.focusedOption = Ui::OptionRow::AA;
    for (Resolution size : {Resolution{680, 384}, Resolution{1200, 900}, Resolution{1920, 768}}) {
        width = size.width; height = size.height;
        drawnTexts.clear(); blits.clear(); destinations.clear();
        scene.Draw(game, hud);
        for (const char* text : {"Master Volume", "Sound Effects", "Card Hints", "Display Resolution", "Screen Mode",
                                 "Anti-Aliasing", "43", "67", "ON", "OFF", "Windowed", "Borderless", "Fullscreen", "2x", "3x", "4x"})
            Check(std::find(drawnTexts.begin(), drawnTexts.end(), text) != drawnTexts.end(), "Options text missing");
        Check(std::count(blits.begin(), blits.end(), 8) == 1 && std::count(blits.begin(), blits.end(), 9) >= 6,
              "Expected the supplied panel and six textured stripes");
        const auto panel = destinations[std::find(blits.begin(), blits.end(), 8) - blits.begin()];
        const float scale = std::min(width / 1360.0f, height / 768.0f);
        Check(Near(panel.x + panel.width / 2, width / 2.0f) && Near(panel.width, Ui::PausePanel.width * scale) &&
              Near(panel.y, (height - 768 * scale) / 2 + Ui::PausePanel.y * scale), "Pause artwork lost letterboxing");
        Check(std::find(drawnTexts.begin(), drawnTexts.end(), "J-Pop Powerplay") == drawnTexts.end(), "Options must not draw radio presets");
        Check(std::find(drawnTexts.begin(), drawnTexts.end(), "CLICK TO PICK") <
              std::find(drawnTexts.begin(), drawnTexts.end(), "Master Volume"), "Game prompts must stay behind pause");
    }
    width = 1600; height = 900;
    for (int n : {2, 3, 4}) {
        Check(scene.SetAntiAliasing(n) == n, "Could not enable AA");
        const auto r = renderSizes.back();
        Check(r.width * 768 == r.height * 1360 && Near(float(r.width * r.height) / (1360 * 768), float(n), 0.08f),
              "AA must increase scene pixel density without changing camera aspect");
        const auto allocations = renderSizes.size();
        Check(scene.SetAntiAliasing(n) == n && allocations == renderSizes.size(), "Unchanged AA must reuse its target");
        destinations.clear(); sources.clear();
        scene.Draw(game, hud);
        Check(Near(destinations[0].width, r.width) && Near(sources[1].height, -r.height) &&
              Near(destinations[1].width, 1360), "Supersampled world must resolve to the logical canvas");
    }
    failCanvas = true;
    Check(scene.SetAntiAliasing(2) == 4, "Failed allocation must preserve the last working AA level");
}

void TestCardHints() {
    Reset();
    TableScene scene("city", "atlas", "logo", "heading", "body", "options");
    // The standard library's shuffle differs between our Windows toolchains.
    for (unsigned seed = 1; seed < 200; ++seed) {
        TableGame game(seed);
        if (game.Busy() || game.Rules().GetPhase() != Phase::AWAIT_HAND_PLAY ||
            game.Rules().CurrentPlayer() != Player::PLAYER) continue;
        TableHudState hud;
        for (const auto& card : game.Cards()) {
            const auto& hand = game.Rules().Hand(Player::PLAYER);
            if (std::find(hand.begin(), hand.end(), card.id) == hand.end()) continue;
            const auto& field = game.Rules().Field();
            const int matches = std::count_if(field.begin(), field.end(), [&](int id) { return id / 4 == card.id / 4; });
            if (!matches) continue;
            hud.pointer = TableLayout::Project(TableLayout::OnPlane(card.rect.x + card.rect.width / 2,
                                           card.rect.y + card.rect.height / 2, TableLayout::CardElevation));
            scene.Draw(game, hud);
            Check(hintEdges == matches * 4, "Show matching field cards for the hovered hand card");
            hud.options.cardHints = false;
            hintEdges = 0; drawnTexts.clear();
            scene.Draw(game, hud);
            Check(hintEdges == 0 && std::find(drawnTexts.begin(), drawnTexts.end(), "Drag a card to a match; right-click to cancel") == drawnTexts.end(),
                  "Disabled card hints must suppress outlines and helper text");
            return;
        }
    }
    throw std::runtime_error("Hint test needs a playable hand match");
}
void TestFailures() {
    Reset(); failAtlas = true;
    bool threw = false;
    try { TableScene scene("city", "atlas", "logo", "heading", "body", "options"); } catch (const std::runtime_error&) { threw = true; }
    Check(threw && unloaded == std::vector<unsigned int>{1}, "Atlas failure or background cleanup failed");
    Reset(); invalidAtlas = true;
    threw = false;
    try { TableScene scene("city", "atlas", "logo", "heading", "body", "options"); } catch (const std::runtime_error&) { threw = true; }
    Check(threw && unloaded == std::vector<unsigned int>({5, 2, 1}), "Invalid atlas or texture cleanup failed");
    Reset(); failStripe = true;
    threw = false;
    try { TableScene scene("city", "atlas", "logo", "heading", "body", "options"); }
    catch (const std::runtime_error& e) { threw = std::string(e.what()).find("setting-stripe.png") != std::string::npos; }
    Check(threw && unloaded == std::vector<unsigned int>({8, 4, 5, 2, 1}), "Missing stripe: identify the file and release the panel");
    Reset(); failCanvas = true;
    threw = false;
    try { TableScene scene("city", "atlas", "logo", "heading", "body", "options"); } catch (const std::runtime_error&) { threw = true; }
    Check(threw && unloaded == std::vector<unsigned int>({9, 8, 4, 5, 2, 1}) && renderUnloads == 0, "Canvas failure or texture cleanup failed");
}
} // namespace

Texture2D LoadTexture(const char* path) {
    ++loads;
    if (loads == 2 && failAtlas) return {};
    if (loads == 1) return {1, 1360, 768, 1, 0};
    if (loads == 2) return {2, invalidAtlas ? 2147 : 2148, 1056, 1, 0};
    if (std::filesystem::path(path).filename() == "menu.png") return {8, 1205, 763, 1, 0};
    if (std::filesystem::path(path).filename() == "setting-stripe.png")
        return failStripe ? Texture2D{} : Texture2D{9, 810, 63, 1, 0};
    return {5, 187, 82, 1, 0};
}
Texture2D LoadTextureFromImage(Image img) {
    Check(img.data != nullptr && img.width == 179 && img.height == 264, "Invalid matte back image");
    return {4, img.width, img.height, 1, img.format};
}
bool FileExists(const char*) { return true; }
Font LoadFontEx(const char* name, int size, int*, int) {
    const unsigned int id = std::string(name) == "heading" ? 6u : 7u;
    return {size, 96, 0, {id, 256, 256, 1, 0}, nullptr, nullptr};
}
Font GetFontDefault() { return {10, 96, 0, {98, 128, 128, 1, 0}, nullptr, nullptr}; }
void UnloadFont(Font font) { unloadedFonts.push_back(font.texture.id); }
void DrawTextEx(Font, const char* text, Vector2, float, float, Color) {
    if (text) drawnTexts.emplace_back(text);
}
void UnloadTexture(Texture2D t) { unloaded.push_back(t.id); }
void SetTextureFilter(Texture2D, int) {}
RenderTexture2D LoadRenderTexture(int w, int h) {
    renderSizes.push_back({w, h});
    return failCanvas ? RenderTexture2D{} : RenderTexture2D{3, {4,w,h,1,0}, {10,w,h,1,0}};
}
bool rlFramebufferComplete(unsigned int) { return true; }
void UnloadRenderTexture(RenderTexture2D) { ++renderUnloads; }
void BeginTextureMode(RenderTexture2D) {}
void EndTextureMode() {}
Shader LoadShaderFromMemory(const char*, const char*) { return {5, nullptr}; }
void UnloadShader(Shader) {}
int GetShaderLocation(Shader, const char*) { return 1; }
void SetShaderValue(Shader, int, const void*, int) {}
void BeginShaderMode(Shader) {}
void EndShaderMode() {}
void BeginMode3D(Camera3D c) { camera = c; }
void EndMode3D() {}
void ClearBackground(Color) {}
void DrawText(const char* text, int, int, int, Color) { if (text) drawnTexts.emplace_back(text); }
void DrawRectangle(int, int, int, int, Color) {}
void DrawRectangleRec(Rectangle, Color) {}
void DrawCircle(int, int, float, Color) {}
void DrawRectangleLinesEx(Rectangle, float, Color) {}
Vector2 MeasureTextEx(Font, const char* text, float size, float) { return {std::string(text).size() * size / 2, size}; }
void DrawLineEx(Vector2, Vector2, float, Color c) { if (c.r == 255 && c.g == 218) ++hintEdges; }
void DrawTriangle(Vector2, Vector2, Vector2, Color) {}
void DrawRing(Vector2, float, float, float, float, int, Color) {}
void DrawRectangleRounded(Rectangle, float, int, Color) {}
void DrawRectangleGradientV(int, int, int, int, Color, Color) {}
int GetScreenWidth() { return width; }
int GetScreenHeight() { return height; }
void BeginDrawing() {}
void EndDrawing() {}
void GenTextureMipmaps(Texture2D*) {}
void TraceLog(int, const char*, ...) {}
void DrawTexturePro(Texture2D texture, Rectangle src, Rectangle dst, Vector2, float, Color) {
    blits.push_back(texture.id); sources.push_back(src); destinations.push_back(dst);
}
unsigned int rlGetTextureIdDefault() { return 99; }
void rlSetTexture(unsigned int id) { textureId = id; }
void rlBegin(int) {}
void rlEnd() {}
void rlColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a) { color = {r,g,b,a}; }
void rlTexCoord2f(float x, float y) { u = x; v = y; }
void rlVertex3f(float x, float y, float z) { vertices.push_back({x,y,z,u,v,color,textureId}); }

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--match-setup") {
            TestMatchSetup();
            std::cout << "Match setup popup and round counts passed.\n";
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--options") {
            TestOptions(); TestPauseRendering(); TestCardHints();
            std::cout << "Pause options, artwork, hints and anti-aliasing passed.\n";
            return 0;
        }
        TestGeometry(); TestDraw(); TestFailures(); std::cout << "Table layout and rendering commands passed.\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
