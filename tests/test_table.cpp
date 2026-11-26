#include "DeckRegister.hpp"
#include "TableLayout.hpp"
#include "TableGame.hpp"
#include "TableScene.hpp"
#include "ui/MatchCaptureLanes.hpp"
#include "ui/MatchResultMenu.hpp"
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
struct TextDraw { std::string text; Vector2 pos; unsigned int font; float size; Color color; };
struct RectDraw { int x, y, width, height; Color color; };
struct BorderDraw { Rectangle rect; float thickness; Color color; };
std::vector<TextDraw> textDraws;
std::vector<RectDraw> rectangleDraws;
std::vector<BorderDraw> borderDraws;
bool mouseButtonDown = false;
bool failAtlas = false, invalidAtlas = false, failCanvas = false, failStripe = false;

void Check(bool condition, const char* text) { if (!condition) throw std::runtime_error(text); }
bool Near(float a, float b, float tolerance = 0.01f) { return std::abs(a - b) < tolerance; }
void Reset() {
    vertices.clear(); unloaded.clear(); unloadedFonts.clear(); destinations.clear(); sources.clear();
    drawnTexts.clear(); textDraws.clear(); rectangleDraws.clear(); borderDraws.clear();
    blits.clear(); renderSizes.clear(); hintEdges = 0; mouseButtonDown = false;
    loads = renderUnloads = draws = 0; failAtlas = invalidAtlas = failCanvas = failStripe = false;
}
void TestCaptureLanes() {
    const std::array<CardId, 8> captures{2, 32, 1, 0, 4, 10, 8, 21};
    const auto lanes = Ui::MatchCaptureLanes(captures);
    Check(lanes[0] == std::vector<CardId>({0, 8}) &&
          lanes[1] == std::vector<CardId>({32, 4}) &&
          lanes[2] == std::vector<CardId>({1, 21}) &&
          lanes[3] == std::vector<CardId>({2, 10}),
          "Capture lanes must group by category and preserve capture order, including the cup");
    constexpr float cardWidth = 28.0f * 179.0f / 264.0f;
    Check(Near(Ui::MatchCaptureY(0, 28.0f), 19.0f) &&
          Near(Ui::MatchCaptureY(1, 28.0f), 54.5f),
          "Capture cards must be vertically centered between the panel frame and divider");
    for (bool you : {false, true}) for (std::size_t column = 0; column < 2; ++column) {
        const auto sparse = Ui::MatchCaptureLayout(you, column, 2, cardWidth);
        const auto crowded = Ui::MatchCaptureLayout(you, column, 20, cardWidth);
        Check(sparse.x >= (you ? 12.0f + (column == 0 ? 60.0f : 218.0f) * 398.0f / 397.0f :
                           950.0f + (column == 0 ? 7.0f : 179.0f)) + 5.0f - 0.01f,
              "Capture cards need an inset from the panel frame and dividers");
        Check(Near(sparse.step, cardWidth) && crowded.step < cardWidth && crowded.step > 0 &&
              19 * crowded.step + cardWidth <= crowded.width + 0.01f,
              "Crowded captures must overlap and stay inside their own cell");
        if (column == 0) {
            const auto next = Ui::MatchCaptureLayout(you, 1, 20, cardWidth);
            Check(crowded.x + crowded.width < next.x, "Capture groups must not cross the divider");
        }
    }
    const auto empty = Ui::MatchCaptureLanes(std::span<const CardId>{});
    Check(std::all_of(empty.begin(), empty.end(), [](const auto& lane) { return lane.empty(); }),
          "Empty captures must produce empty lanes");
}
std::array<Vector3, 4> ExpectedCardTop(const TableCard& card) {
    const auto p00 = TableLayout::OnPlane(card.rect.x, card.rect.y, 0.0f);
    const auto p11 = TableLayout::OnPlane(card.rect.x + card.rect.width,
                                          card.rect.y + card.rect.height, 0.0f);
    const float hx = 0.5f * (p11.x - p00.x);
    const float hz = 0.5f * (p11.z - p00.z);
    const float hy = 0.5f * card.thickness;
    const float rz = card.flipAngle + card.roll;
    const float rx = card.pitch;
    const float ry = card.yaw;
    const float cosZ = std::cos(rz), sinZ = std::sin(rz);
    const float cosX = std::cos(rx), sinX = std::sin(rx);
    const float cosY = std::cos(ry), sinY = std::sin(ry);
    const float extentY = std::abs(hx * sinZ * cosX) +
                          std::abs(hy * cosZ * cosX) +
                          std::abs(hz * sinX);
    const float cx = 0.5f * (p00.x + p11.x);
    const float cy = card.elevation + extentY;
    const float cz = 0.5f * (p00.z + p11.z);
    const auto transform = [&](float lx, float ly, float lz) {
        const float x1 = lx * cosZ - ly * sinZ;
        const float y1 = lx * sinZ + ly * cosZ;
        const float y2 = y1 * cosX - lz * sinX;
        const float z2 = y1 * sinX + lz * cosX;
        const float x3 = x1 * cosY + z2 * sinY;
        const float z3 = -x1 * sinY + z2 * cosY;
        return Vector3{cx + x3, cy + y2, cz + z3};
    };
    return {transform(-hx, hy, -hz), transform(-hx, hy, hz),
            transform(hx, hy, hz), transform(hx, hy, -hz)};
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
    TestCaptureLanes();
    {
        TableScene scene("city", "atlas", "logo", "heading", "body", "options");
        TableGame game(42);
        Check(!game.DealerSelectionActive() && game.TotalRounds() == TableLayout::MatchRounds,
              "Seeded TableGame(int) constructor must seed a standard match instead of overriding rounds");
        scene.Draw(game);
        const auto cards = game.Cards();
        Check(cards.size() > 2 && std::any_of(cards.begin(), cards.end(),
                                              [](const TableCard& c) { return c.id >= 0; }),
              "Seeded table draw must include face-up cards");
        std::size_t expectedShadows = 0;
        for (const auto& card : cards) if (card.shadow) ++expectedShadows;
        constexpr std::size_t ShadowVerticesPerCard = 32;
        const std::size_t shadowVertices = expectedShadows * ShadowVerticesPerCard;
        const std::size_t cardVertices = cards.size() * 24;
        Check(vertices.size() == shadowVertices + cardVertices, "Card render count differs from game state");
        for (std::size_t i = 0; i < expectedShadows; ++i) {
            bool hasFeatheredEdge = false, hasContactEdge = false;
            bool hasTransparentEdge = false;
            for (std::size_t j = 0; j < ShadowVerticesPerCard; ++j) {
                const auto& shadowVert = vertices[i * ShadowVerticesPerCard + j];
                Check(shadowVert.texture == 99 && shadowVert.y < TableLayout::CardElevation &&
                      shadowVert.color.a <= 64,
                      "Card shadow must sit below the card plane with translucent edges");
                if (j < 16) {
                    Check(shadowVert.color.r == 70 && shadowVert.color.g == 53 && shadowVert.color.b == 34 &&
                          Near(shadowVert.y, 0.00035f), "Ambient shadow tint must remain faint and warm");
                    hasFeatheredEdge = hasFeatheredEdge || shadowVert.color.a > 0;
                } else {
                    Check(shadowVert.color.r == 7 && shadowVert.color.g == 9 && shadowVert.color.b == 15 &&
                          Near(shadowVert.y, 0.00055f), "Contact shadow must remain cool and below the card");
                    hasContactEdge = hasContactEdge || shadowVert.color.a > 0;
                }
                hasTransparentEdge = hasTransparentEdge || shadowVert.color.a == 0;
            }
            Check(hasFeatheredEdge && hasContactEdge && hasTransparentEdge,
                  "Contact shadows must feather to transparent beyond the card frame");
        }
        for (std::size_t i = 0; i < cards.size(); ++i) {
            const auto& card = cards[i];
            const std::size_t base = shadowVertices + i * 24;
            const auto expectedTop = ExpectedCardTop(card);
            for (std::size_t j = 0; j < expectedTop.size(); ++j) {
                const auto& vert = vertices[base + j];
                Check(Near(vert.x, expectedTop[j].x, 0.00001f) &&
                      Near(vert.y, expectedTop[j].y, 0.00001f) &&
                      Near(vert.z, expectedTop[j].z, 0.00001f),
                      "Soft shadows must not move a card outline vertex");
            }
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
        Check(destinations.size() >= 4 && Near(destinations.back().height, 900) &&
              Near(destinations.back().width, 1360.0f * 900.0f / 768.0f) && sources.back().height < 0,
              "Window resize must preserve aspect ratio and flip render target");

        // Verify right-side pause button stays clear of decision banners and renders pause menu radio presets.
        const auto noOverlap = [](TableLayout::Rect a, TableLayout::Rect b) {
            return a.x + a.width <= b.x || b.x + b.width <= a.x ||
                   a.y + a.height <= b.y || b.y + b.height <= a.y;
        };
        Check(TableHud::RadioMenuButton.x == 1266.0f && TableHud::RadioMenuButton.y == 322.0f &&
              TableHud::RadioMenuButton.width == 76.0f && TableHud::RadioMenuButton.height == 58.0f &&
              TableHud::PauseButton.x == 1266.0f && TableHud::PauseButton.y == 390.0f &&
              TableHud::PauseButton.width == 76.0f && TableHud::PauseButton.height == 58.0f &&
              TableHud::HomeButton.x == 1266.0f && TableHud::HomeButton.y == 458.0f &&
              TableHud::HomeButton.width == 76.0f && TableHud::HomeButton.height == 58.0f,
              "Icon plates must preserve all three existing HUD hit rectangles");
        Check(TableHud::KoiKoiButton.x == 505.0f && TableHud::KoiKoiButton.y == 336.0f &&
              TableHud::KoiKoiButton.width == 165.0f && TableHud::KoiKoiButton.height == 48.0f &&
              TableHud::ShobuButton.x == 690.0f && TableHud::ShobuButton.y == 336.0f &&
              TableHud::ShobuButton.width == 165.0f && TableHud::ShobuButton.height == 48.0f &&
              TableHud::RoundEndButton.x == 520.0f && TableHud::RoundEndButton.y == 326.0f &&
              TableHud::RoundEndButton.width == 320.0f && TableHud::RoundEndButton.height == 72.0f,
              "Decision and round-end buttons must preserve their existing hit rectangles");
        Check(TableHud::PauseButton.x >= 1200.0f &&
              TableHud::HomeButton.x == TableHud::PauseButton.x &&
              TableHud::HomeButton.y > TableHud::PauseButton.y + TableHud::PauseButton.height &&
              noOverlap(TableHud::PauseButton, TableHud::HomeButton) &&
              noOverlap(TableHud::RadioMenuButton, TableHud::HomeButton) &&
              noOverlap(TableHud::PauseButton, TableHud::KoiKoiButton) &&
              noOverlap(TableHud::PauseButton, TableHud::ShobuButton) &&
              noOverlap(TableHud::PauseButton, TableHud::RoundEndButton),
              "Right-side pause/home buttons must not overlap each other or decision banners");
        Check(std::find(drawnTexts.begin(), drawnTexts.end(), "HOME") != drawnTexts.end() &&
              std::find(drawnTexts.begin(), drawnTexts.end(), "RADIO") != drawnTexts.end() &&
              std::find(drawnTexts.begin(), drawnTexts.end(), "PAUSE") != drawnTexts.end(),
              "HUD must draw all three bilingual icon plates");
        for (unsigned int icon : {14u, 15u, 16u, 17u})
            Check(std::find(blits.begin(), blits.end(), icon) != blits.end(),
                  "HUD icon and kana artwork must be loaded and blitted");

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

        drawnTexts.clear(); textDraws.clear(); rectangleDraws.clear();
        TableGame sixRoundGame(42u, 6);
        scene.Draw(sixRoundGame);
        Check(hasText("ROUND ") && hasText("1") && hasText("/") && hasText("6"),
              "The round plate must show the current and total round numerals");
        Check(std::any_of(textDraws.begin(), textDraws.end(), [](const TextDraw& draw) {
            return draw.text == "ROUND " && draw.font == 7 &&
                   Near(draw.pos.x + 11.0f * 22.0f / 4.0f, 680.0f) && Near(draw.pos.y, 22.0f);
        }), "Round indicator must use the game font and remain centered at the top");
        Check(std::any_of(textDraws.begin(), textDraws.end(), [](const TextDraw& draw) {
            return draw.text == "1" && draw.color.r == 255 && draw.color.g == 213 && draw.color.b == 25;
        }) && std::any_of(textDraws.begin(), textDraws.end(), [](const TextDraw& draw) {
            return draw.text == "6" && draw.color.r == 255 && draw.color.g == 213 && draw.color.b == 25;
        }), "Round numerals must use the pause-menu signal yellow");
        const auto hasRoundPlate = [] {
            return std::any_of(rectangleDraws.begin(), rectangleDraws.end(), [](const RectDraw& draw) {
                return draw.x == 574 && draw.y == 9 && draw.width == 212 && draw.height == 48 &&
                       draw.color.r == 8 && draw.color.g == 31 && draw.color.b == 122;
            });
        };
        Check(hasRoundPlate() && 574.0f > 410.0f && 574.0f + 212.0f < 950.0f,
              "Opaque navy round plate must sit between the player panels");
        Check(hasText("RADIO") && hasText("PAUSE") && hasText("HOME"),
              "Radio, pause and home controls must remain visible");

        width = 680; height = 384;
        drawnTexts.clear(); textDraws.clear(); rectangleDraws.clear(); destinations.clear();
        scene.Draw(sixRoundGame);
        Check(hasRoundPlate() && destinations.back().width == 680.0f &&
              destinations.back().height == 384.0f &&
              574.0f * 0.5f > 410.0f * 0.5f && (574.0f + 212.0f) * 0.5f < 950.0f * 0.5f,
              "Round plate must stay centered and clear of player panels at the 680x384 minimum");

        const auto hasBorder = [](TableLayout::Rect rect, Color expected) {
            return std::any_of(borderDraws.begin(), borderDraws.end(), [&](const BorderDraw& border) {
                return Near(border.rect.x, rect.x) && Near(border.rect.y, rect.y) &&
                       Near(border.rect.width, rect.width) && Near(border.rect.height, rect.height) &&
                       border.color.r == expected.r && border.color.g == expected.g && border.color.b == expected.b;
            });
        };
        const auto hasFill = [](int x, int y, int w, int h, Color expected) {
            return std::any_of(rectangleDraws.begin(), rectangleDraws.end(), [&](const RectDraw& rect) {
                return rect.x == x && rect.y == y && rect.width == w && rect.height == h &&
                       rect.color.r == expected.r && rect.color.g == expected.g && rect.color.b == expected.b;
            });
        };
        TableHudState activeHud{};
        activeHud.radio.enabled = true;
        drawnTexts.clear(); textDraws.clear(); rectangleDraws.clear(); borderDraws.clear();
        scene.Draw(sixRoundGame, activeHud);
        Check(hasFill(1266, 322, 76, 58, {7, 26, 100, 255}) &&
              hasText("RADIO") && hasBorder(TableHud::RadioMenuButton, {196, 207, 227, 255}),
              "An active radio entry must use the navy plate and signal-yellow detail");

        TableHudState hoverHud{};
        hoverHud.pointer = {TableHud::PauseButton.x + TableHud::PauseButton.width * 0.5f,
                            TableHud::PauseButton.y + TableHud::PauseButton.height * 0.5f};
        drawnTexts.clear(); textDraws.clear(); rectangleDraws.clear(); borderDraws.clear();
        scene.Draw(sixRoundGame, hoverHud);
        Check(hasFill(1266, 390, 76, 58, {17, 45, 142, 255}) &&
              hasBorder(TableHud::PauseButton, {255, 213, 25, 255}),
              "Hovering a side plate must create a signal-yellow focus bevel");

        TableHudState pressedHud{};
        pressedHud.pointer = {TableHud::HomeButton.x + TableHud::HomeButton.width * 0.5f,
                              TableHud::HomeButton.y + TableHud::HomeButton.height * 0.5f};
        mouseButtonDown = true;
        drawnTexts.clear(); textDraws.clear(); rectangleDraws.clear(); borderDraws.clear();
        scene.Draw(sixRoundGame, pressedHud);
        mouseButtonDown = false;
        Check(hasFill(1267, 459, 74, 56, {4, 13, 46, 255}) &&
              hasBorder(TableHud::HomeButton, {255, 213, 25, 255}),
              "Pressing a side plate must show the inset pressed state without changing its hit rect");
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
        Check(std::find(drawnTexts.begin(), drawnTexts.end(), "PICK A CARD TO DETERMINE DEALER") != drawnTexts.end() &&
              std::find(drawnTexts.begin(), drawnTexts.end(), "PICK CARD") != drawnTexts.end() &&
              std::find(blits.begin(), blits.end(), 17u) != blits.end(),
              "Dealer selection must use the paired kana/English signage prompt");
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
        Check(std::find(drawnTexts.begin(), drawnTexts.end(), "PICK CARD") <
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

void TestMatchResultMenu() {
    // 1. Opening lucky hand combo extraction (Teshi + Both Opening Draw).
    {
        // Player hand = 4 Jan cards (Teshi) + 4 other cards; Opponent = normal; Field = 8 distinct months.
        std::vector<CardId> perm{
            0, 1, 2, 3, 4, 8, 12, 16,      // Player (Teshi on Jan)
            5, 9, 13, 17, 20, 24, 28, 32,  // Opponent (no lucky hand)
            6, 10, 14, 18, 21, 25, 29, 33  // Field (8 distinct months -> valid)
        };
        for (int id = 0; id < 48; ++id) {
            if (std::find(perm.begin(), perm.end(), id) == perm.end()) perm.push_back(id);
        }
        KoiKoiStateMachine sm(Player::PLAYER);
        Check(sm.TryStartDeal(perm) && sm.GetPhase() == Phase::ROUND_END &&
              sm.Result()->reason == EndReason::OPENING_WIN &&
              sm.Result()->winner == Player::PLAYER,
              "Expected Teshi opening win for player");
        const auto youCombos = Ui::MatchResultMenu::BuildPlayerCombos(sm, Player::PLAYER);
        const auto cpuCombos = Ui::MatchResultMenu::BuildPlayerCombos(sm, Player::OPPONENT);
        Check(youCombos.size() == 1 && youCombos[0].name == "Teshi (Lucky)" &&
              youCombos[0].points == KoiKoiProfile::OpeningPoints &&
              youCombos[0].cards.size() == 8 &&
              std::is_sorted(youCombos[0].cards.begin(), youCombos[0].cards.end()) &&
              cpuCombos.empty(),
              "Lucky hand combo extraction must return sorted 8-card hand with 6 pts");

        // All 8 KK-ADD-10 stacked combo families simultaneously, including sake cup as phantom plain in Kasu.
        const std::vector<CardId> megaCapture{
            0, 8, 28, 40, 44,                       // Goko (10)
            20, 24, 36, 4, 32,                      // Ino-shika-cho (5) + Tane (5 animals -> 1) + Hanami (5) + Tsukimi (5) + sake cup
            1, 5, 9, 21, 33, 37,                    // Akatan + Aotan (10) + Tanzaku (6 ribbons -> 3)
            2, 3, 6, 7, 10, 11, 14, 15, 18          // 9 physical plains + sake cup (32) = 10 kasu units -> Kasu (1)
        };
        const auto megaCombos = Ui::MatchResultMenu::BuildCombosFromCaptures(megaCapture);
        const YakuScore expectedMega = ScoreYaku(megaCapture);
        int megaSum = 0;
        bool hasGoko = false, hasAkatanAotan = false, hasKasuWithCup = false;
        for (const auto& c : megaCombos) {
            megaSum += c.points;
            if (c.name == "Goko" && c.cards.size() == 5) hasGoko = true;
            if (c.name == "Akatan + Aotan" && c.cards.size() == 6) hasAkatanAotan = true;
            if (c.name == "Kasu (10)" && c.cards.size() == 10 &&
                std::find(c.cards.begin(), c.cards.end(), 32) != c.cards.end()) hasKasuWithCup = true;
        }
        Check(megaCombos.size() == 8 && megaSum == expectedMega.base &&
              hasGoko && hasAkatanAotan && hasKasuWithCup,
              "Stacked KK-ADD-10 combo extraction must match ScoreYaku and include sake cup in Kasu");
    }

    // 2. Full 3-round match: round-end popup, mini card blits, pause-scale geometry, two-step match summary.
    Reset();
    {
        TableScene scene("city", "atlas", "logo", "heading", "body", "options");
        Ui::MatchResultMenu menu("app");
        const auto reachesPlayerDecision = [](unsigned seed) {
            TableGame candidate(seed, 3);
            for (int step = 0; step < 6000; ++step) {
                if (candidate.Busy()) { candidate.Update(0.05f); continue; }
                const auto& rules = candidate.Rules();
                const Phase phase = rules.GetPhase();
                if (phase == Phase::ROUND_END) return false;
                if (rules.CurrentPlayer() == Player::PLAYER) {
                    if (phase == Phase::AWAIT_DECISION) return true;
                    if (phase == Phase::AWAIT_HAND_PLAY) {
                        const int card = rules.Hand(Player::PLAYER).front();
                        TableLayout::Rect handRect{};
                        for (const auto& c : candidate.Cards())
                            if (c.id == card) { handRect = c.rect; break; }
                        if (handRect.width <= 0.0f || handRect.height <= 0.0f) return false;
                        const TableLayout::Point2 start{handRect.x + handRect.width * 0.5f,
                                                        handRect.y + handRect.height * 0.5f};
                        candidate.PointerDown(start);
                        TableLayout::Point2 drop{430.0f, 355.0f};
                        for (int fieldCard : rules.Field()) {
                            if (DeckRegister::At(fieldCard).month == DeckRegister::At(card).month) {
                                const auto fieldRect = candidate.FieldRect(fieldCard);
                                drop = {fieldRect.x + fieldRect.width * 0.5f,
                                        fieldRect.y + fieldRect.height * 0.5f};
                                break;
                            }
                        }
                        candidate.PointerUp(drop);
                        continue;
                    }
                    if (phase == Phase::AWAIT_DRAW_CHOICE) {
                        if (rules.Choices().empty()) return false;
                        const auto fieldRect = candidate.FieldRect(rules.Choices().front());
                        candidate.PointerDown({fieldRect.x + fieldRect.width * 0.5f,
                                               fieldRect.y + fieldRect.height * 0.5f});
                        continue;
                    }
                }
                candidate.Update(0.05f);
            }
            return false;
        };
        unsigned matchSeed = 1;
        while (matchSeed <= 500 && !reachesPlayerDecision(matchSeed)) ++matchSeed;
        Check(matchSeed <= 500,
              "Cross-platform seed search must find a player decision for HUD styling coverage");
        TableGame game(matchSeed, 3);
        width = 1360; height = 768;
        bool sawDecisionButtons = false;

        auto playUntilRoundEnd = [&](TableGame& g) {
            for (int step = 0; step < 6000 && g.Rules().GetPhase() != Phase::ROUND_END; ++step) {
                if (g.Busy()) { g.Update(0.05f); continue; }
                const auto& r = g.Rules();
                if (r.CurrentPlayer() == Player::PLAYER) {
                    if (r.GetPhase() == Phase::AWAIT_HAND_PLAY) {
                        const int card = r.Hand(Player::PLAYER).front();
                        TableLayout::Rect handRect{};
                        for (const auto& c : g.Cards()) if (c.id == card) { handRect = c.rect; break; }
                        const TableLayout::Point2 start{handRect.x + handRect.width * 0.5f,
                                                        handRect.y + handRect.height * 0.5f};
                        g.PointerDown(start);
                        TableLayout::Point2 drop{430.0f, 355.0f};
                        for (int fid : r.Field()) {
                            if (DeckRegister::At(fid).month == DeckRegister::At(card).month) {
                                const auto fr = g.FieldRect(fid);
                                drop = {fr.x + fr.width * 0.5f, fr.y + fr.height * 0.5f};
                                break;
                            }
                        }
                        g.PointerUp(drop);
                        continue;
                    }
                    if (r.GetPhase() == Phase::AWAIT_DRAW_CHOICE) {
                        const auto fr = g.FieldRect(r.Choices().front());
                        g.PointerDown({fr.x + fr.width * 0.5f, fr.y + fr.height * 0.5f});
                        continue;
                    }
                    if (r.GetPhase() == Phase::AWAIT_DECISION) {
                        TableHudState decisionHud{};
                        decisionHud.pointer = {TableHud::KoiKoiButton.x + TableHud::KoiKoiButton.width * 0.5f,
                                               TableHud::KoiKoiButton.y + TableHud::KoiKoiButton.height * 0.5f};
                        mouseButtonDown = true;
                        drawnTexts.clear(); textDraws.clear(); blits.clear();
                        rectangleDraws.clear(); borderDraws.clear();
                        scene.Draw(g, decisionHud);
                        mouseButtonDown = false;
                        Check(std::find(drawnTexts.begin(), drawnTexts.end(), "KOI-KOI") != drawnTexts.end() &&
                              std::find(drawnTexts.begin(), drawnTexts.end(), "SHOBU") != drawnTexts.end() &&
                              std::find(blits.begin(), blits.end(), 17u) != blits.end(),
                              "Koi-Koi and Shobu must render paired kana/English signage");
                        Check(std::any_of(borderDraws.begin(), borderDraws.end(), [](const BorderDraw& border) {
                            return Near(border.rect.x, TableHud::KoiKoiButton.x) &&
                                   Near(border.rect.y, TableHud::KoiKoiButton.y) &&
                                   border.color.r == 255 && border.color.g == 213 && border.color.b == 25;
                        }) && std::any_of(textDraws.begin(), textDraws.end(), [](const TextDraw& draw) {
                            return draw.text == "KOI-KOI" && draw.color.r == 255 &&
                                   draw.color.g == 213 && draw.color.b == 25;
                        }), "Hovered, pressed decision buttons must show signal-yellow focus");
                        sawDecisionButtons = true;
                        g.Choose(Decision::SHOUBU);
                        continue;
                    }
                }
                g.Update(0.05f);
            }
            for (int settle = 0; settle < 200 && g.Busy(); ++settle) g.Update(0.05f);
            Check(g.Rules().GetPhase() == Phase::ROUND_END && !g.Busy(),
                  "Test round must finish cleanly");
            TableHudState roundEndHud{};
            roundEndHud.pointer = {TableHud::RoundEndButton.x + TableHud::RoundEndButton.width * 0.5f,
                                   TableHud::RoundEndButton.y + TableHud::RoundEndButton.height * 0.5f};
            mouseButtonDown = true;
            drawnTexts.clear(); textDraws.clear(); blits.clear(); rectangleDraws.clear(); borderDraws.clear();
            scene.Draw(g, roundEndHud);
            mouseButtonDown = false;
            const char* title = g.MatchFinished() ? "MATCH OVER" : "NEXT ROUND";
            const char* action = g.MatchFinished() ? "CLICK TO RESTART" : "CLICK TO CONTINUE";
            Check(std::find(drawnTexts.begin(), drawnTexts.end(), title) != drawnTexts.end() &&
                  std::find(drawnTexts.begin(), drawnTexts.end(), action) != drawnTexts.end() &&
                  std::find(blits.begin(), blits.end(), 17u) != blits.end(),
                  "Round-end actions must render bilingual, state-appropriate labels");
            Check(std::any_of(borderDraws.begin(), borderDraws.end(), [](const BorderDraw& border) {
                return Near(border.rect.x, TableHud::RoundEndButton.x) &&
                       Near(border.rect.y, TableHud::RoundEndButton.y) &&
                       border.color.r == 255 && border.color.g == 213 && border.color.b == 25;
            }), "Pressed round-end action must keep its exact bounds and yellow focus");
        };

        bool sawAnyMiniCards = false;
        for (int round = 1; round <= 3; ++round) {
            playUntilRoundEnd(game);
            menu.SyncRoundEnd(game);
            menu.SyncRoundEnd(game); // Idempotent within the same round.
            Check(menu.IsOpen() && menu.CurrentMode() == Ui::MatchResultMode::ROUND &&
                  static_cast<int>(menu.Ledger().size()) == round,
                  "MatchResultMenu must open in ROUND mode and record each round once");

            // Check combo points match ScoreYaku base for normal Shoubu/draw rounds.
            if (game.Rules().Result()->reason == EndReason::SHOUBU ||
                game.Rules().Result()->reason == EndReason::EXHAUSTIVE_DRAW) {
                for (Player p : {Player::PLAYER, Player::OPPONENT}) {
                    const int expectedBase = ScoreYaku(game.Rules().Captured(p)).base;
                    int comboSum = 0;
                    for (const auto& c : menu.Combos(p)) comboSum += c.points;
                    Check(comboSum == expectedBase, "Displayed combo points must sum to ScoreYaku base");
                }
            }

            TableHudState hud{};
            hud.matchResult = &menu;
            for (Resolution size : {Resolution{680, 384}, Resolution{1600, 900}, Resolution{1920, 768}}) {
                width = size.width; height = size.height;
                drawnTexts.clear(); blits.clear(); sources.clear(); destinations.clear();
                scene.Draw(game, hud);

                const auto panelIt = std::find(blits.begin(), blits.end(), 11u);
                Check(panelIt != blits.end(), "MatchResultMenu must blit matchresult.png");
                const auto panelDst = destinations[static_cast<std::size_t>(panelIt - blits.begin())];
                const float scale = std::min(width / 1360.0f, height / 768.0f);
                Check(Near(panelDst.x + panelDst.width * 0.5f, width * 0.5f) &&
                      Near(panelDst.y + panelDst.height * 0.5f, height * 0.5f) &&
                      Near(panelDst.width, Ui::MatchResultPanel.width * scale) &&
                      Near(panelDst.height, Ui::MatchResultPanel.height * scale),
                      "MatchResultMenu must keep 81.5% pause-menu scale and centered letterboxing");

                Check(std::find(drawnTexts.begin(), drawnTexts.end(), "NEXT ROUND") == drawnTexts.end() &&
                      std::find(drawnTexts.begin(), drawnTexts.end(), "MATCH OVER") == drawnTexts.end(),
                      "Old placeholder round-end box must stay hidden when MatchResultMenu is attached");

                for (std::size_t i = 0; i < blits.size(); ++i) {
                    if (blits[i] == 12u) {
                        sawAnyMiniCards = true;
                        Check(Near(sources[i].width, 179.0f) && Near(sources[i].height, 264.0f),
                              "Miniature cards must sample complete 179x264 atlas cells");
                    }
                }
            }
            width = 1600; height = 900;

            // Pause overlay must render above MatchResultMenu when open.
            drawnTexts.clear();
            hud.overlay = TableOverlay::PAUSE;
            scene.Draw(game, hud);
            const auto hasText = [&](const std::string& needle) {
                return std::any_of(drawnTexts.begin(), drawnTexts.end(),
                                   [&](const std::string& s) { return s.find(needle) != std::string::npos; });
            };
            Check(hasText("Master Volume"), "Pause menu must render above MatchResultMenu");

            // Check Back and Next button hit testing + keyboard navigation.
            const TableLayout::Point2 backCanvas{
                Ui::MatchResultPanel.x + (Ui::MatchResultLayout::BackButton.x + 60.0f) * Ui::MatchResultArtScale,
                Ui::MatchResultPanel.y + (Ui::MatchResultLayout::BackButton.y + 25.0f) * Ui::MatchResultArtScale};
            const TableLayout::Point2 nextCanvas{
                Ui::MatchResultPanel.x + (Ui::MatchResultLayout::NextButton.x + 60.0f) * Ui::MatchResultArtScale,
                Ui::MatchResultPanel.y + (Ui::MatchResultLayout::NextButton.y + 25.0f) * Ui::MatchResultArtScale};
            Check(menu.Update(backCanvas, true, true, 0, false) == Ui::MatchResultAction::BACK,
                  "Clicking Back half of pill must return BACK");
            Check(menu.Update(nextCanvas, true, true, 0, false) == Ui::MatchResultAction::NEXT,
                  "Clicking Next half of pill must return NEXT");
            Check(menu.Update({0, 0}, false, false, -1, false) == Ui::MatchResultAction::NONE &&
                  menu.FocusedButton() == 0 &&
                  menu.Update({0, 0}, false, false, 0, true) == Ui::MatchResultAction::BACK,
                  "Left arrow + Enter must activate Back");
            Check(menu.Update({0, 0}, false, false, 1, true) == Ui::MatchResultAction::NEXT,
                  "Right arrow + Enter must activate Next");

            if (round < 3) {
                Check(!game.MatchFinished(), "Match must not finish before final round");
                menu.CloseRoundPopup();
                Check(!menu.IsOpen(), "CloseRoundPopup must close popup for next round");
                game.NextRound();
            } else {
                Check(game.MatchFinished(), "Match must be finished on round 3 of 3");
                menu.ShowMatchSummary();
                Check(menu.IsOpen() && menu.CurrentMode() == Ui::MatchResultMode::MATCH,
                      "Final round Next must switch popup to MATCH summary mode");
                drawnTexts.clear();
                hud.overlay = TableOverlay::NONE;
                scene.Draw(game, hud);
                Check(hasText("ROUND LEDGER") &&
                      (hasText("YOU WIN THE MATCH!") || hasText("CPU WINS THE MATCH!") || hasText("MATCH DRAW!")),
                      "Match summary must draw round ledger and match winner banner");
            }
        }
        Check(sawDecisionButtons, "A player decision state must exercise Koi-Koi/Shobu hover and pressed styling");
        Check(sawAnyMiniCards, "Expected at least one combo with miniature cards across a 3-round match");
        menu.ResetMatch();
        Check(!menu.IsOpen() && menu.Ledger().empty(), "ResetMatch must clear ledger and close popup");
    }
    Check(std::find(unloaded.begin(), unloaded.end(), 11u) != unloaded.end() &&
          std::find(unloaded.begin(), unloaded.end(), 12u) != unloaded.end(),
          "MatchResultMenu must unload its panel and atlas textures on destruction");
}
} // namespace

const char* GetApplicationDirectory() { return "."; }
Texture2D LoadTexture(const char* path) {
    ++loads;
    if (loads == 2 && failAtlas) return {};
    if (loads == 1) return {1, 1360, 768, 1, 0};
    if (loads == 2) return {2, invalidAtlas ? 2147 : 2148, 1056, 1, 0};
    if (std::filesystem::path(path).filename() == "MatchUI.png") return {13, 1334, 81, 1, 0};
    if (std::filesystem::path(path).filename() == "radio-icon.png") return {14, 64, 64, 1, 0};
    if (std::filesystem::path(path).filename() == "pause-icon.png") return {15, 64, 64, 1, 0};
    if (std::filesystem::path(path).filename() == "home-icon.png") return {16, 64, 64, 1, 0};
    if (std::filesystem::path(path).filename() == "hud-kana.png") return {17, 192, 432, 1, 0};
    if (std::filesystem::path(path).filename() == "menu.png") return {8, 1205, 763, 1, 0};
    if (std::filesystem::path(path).filename() == "setting-stripe.png")
        return failStripe ? Texture2D{} : Texture2D{9, 810, 63, 1, 0};
    if (std::filesystem::path(path).filename() == "matchresult.png") return {11, 1164, 785, 1, 0};
    if (std::filesystem::path(path).filename() == "cards_atlas.png") return {12, 2148, 1056, 1, 0};
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
bool IsMouseButtonDown(int button) { return button == MOUSE_BUTTON_LEFT && mouseButtonDown; }
void DrawTextEx(Font font, const char* text, Vector2 pos, float size, float, Color tint) {
    if (text) { drawnTexts.emplace_back(text); textDraws.push_back({text, pos, font.texture.id, size, tint}); }
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
void DrawRectangle(int x, int y, int w, int h, Color c) { rectangleDraws.push_back({x, y, w, h, c}); }
void DrawRectangleLinesEx(Rectangle rect, float thickness, Color c) { borderDraws.push_back({rect, thickness, c}); }
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
        if (argc == 2 && std::string(argv[1]) == "--options") {
            TestOptions(); TestPauseRendering(); TestCardHints();
            std::cout << "Pause options, artwork, hints and anti-aliasing passed.\n";
            return 0;
        }
        TestGeometry(); TestDraw(); TestMatchResultMenu(); TestFailures(); std::cout << "Table layout and rendering commands passed.\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
