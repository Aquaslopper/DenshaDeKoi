// ============================================================================
//  viewer.cpp - headless viewer for the Densha de Koi client.
//  Ported from SPACESHIP/tools/viewer. Not built by CMake or CI. Drives the
//  game's real menus and table renderer against raylib's software renderer
//  (PLATFORM_MEMORY) and writes PNG frames, so the screens can be looked at
//  on a machine with no display.
//
//  A frame shows what the game computes: layout, strings, sizes, hit boxes.
//  It does not show GPU anti-aliasing, the display-adjustment shader or
//  sound. It is a look, not a test: nothing is asserted.
//
//  No game file is copied or edited. Two things reach into it at link time:
//    - --wrap=DrawTextEx records every string the game paints.
//    - --wrap for the magnification shader, the clock, the file dialogs and
//      a handful of raylib functions the software build cannot satisfy.
//
//  Build, usage and the script commands: README.md beside this file.
// ============================================================================
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// The game's card enums are parsed before raylib defines its BLUE colour
// macro (the same order the client's own translation units use).
#include "DeckRegister.hpp"
#include "KoiKoiProfile.hpp"
#include "KoiKoiRules.hpp"
#include "KoiKoiStateMachine.hpp"
#include "RadioEngine.hpp"
#include "TableGame.hpp"
#include "TableLayout.hpp"
#include "TableScene.hpp"
#include "ui/Hud.hpp"
#include "ui/UiViewport.hpp"

#include "raylib.h"
#include "rlgl.h"

// Viewer only: read the menus' private state so a script can set focus, open
// the gallery viewer and report what the game holds. No game file is changed;
// the macro lives only in this translation unit, and the headers are included
// before their public includes so every declaration seen here is public.
#define private public
#include "ui/CardGallery.hpp"
#include "ui/CardViewer.hpp"
#include "ui/LoadingScreen.hpp"
#include "ui/MatchResultMenu.hpp"
#include "ui/MatchSettingsMenu.hpp"
#include "ui/PauseMenu.hpp"
#include "ui/PauseRadioMenu.hpp"
#include "ui/StartMenu.hpp"
#undef private

namespace fs = std::filesystem;

// ============================================================================
// 1. Text recorder
// ----------------------------------------------------------------------------
//  Every string the game paints through raylib is recorded with the size and
//  measured width it was painted at. The link flag -Wl,--wrap=DrawTextEx routes
//  the game's own calls through here. The viewer paints its own frames
//  straight into the framebuffer and records nothing itself.
// ============================================================================
struct TextCall {
    std::string text;
    float x = 0.0f, y = 0.0f;      // window pixels, after canvas mapping
    float size = 0.0f, width = 0.0f;
    Color tint{};
    unsigned int font = 0;         // texture id, 0 = raylib default face
};
static bool g_writeTextLog = false;
static std::vector<TextCall> g_text;

// The recorded canvas -> window transform. Every game frame is drawn into a
// 1360x768 render texture and then letterboxed into the window, so text drawn
//  inside the canvas must be mapped or a .text.tsv would disagree with the
//  PNG. The viewer maintains these while it draws.
static bool g_inCanvas = false;
static float g_mapScale = 1.0f;
static float g_mapOffsetX = 0.0f;
static float g_mapOffsetY = 0.0f;

extern "C" void __real_DrawTextEx(Font font, const char* text, Vector2 pos, float size,
                                  float spacing, Color tint);
extern "C" void __wrap_DrawTextEx(Font font, const char* text, Vector2 pos, float size,
                                  float spacing, Color tint) {
    const char* safe = text != nullptr ? text : "";
    TextCall call;
    call.text = safe;
    call.size = size;
    call.width = MeasureTextEx(font, safe, size, spacing).x;
    call.tint = tint;
    call.font = font.texture.id;
    if (g_inCanvas) {
        call.x = g_mapOffsetX + pos.x * g_mapScale;
        call.y = g_mapOffsetY + pos.y * g_mapScale;
        call.size *= g_mapScale;
        call.width *= g_mapScale;
    } else {
        call.x = pos.x;
        call.y = pos.y;
    }
    g_text.push_back(std::move(call));
    __real_DrawTextEx(font, text, pos, size, spacing, tint);
}

// ============================================================================
// 2. Wrapped raylib functions
// ----------------------------------------------------------------------------
//  The software renderer has no shader programs, so the table's magnification
//  shader is stubbed out here instead of editing src/TableScene.cpp (which is
//  a protected file). The clock is scripted so fades and animations are the
//  same on every machine. File dialogs are absent on a headless box. Fonts load
//  with padding and bilinear filtering, which the game leaves to chance.
// ============================================================================
static double g_virtualSeconds = 1000.0;   // the viewer's clock
static float g_frameSeconds = 1.0f / 60.0f; // what the last frame consumed

// Scripted frame clock: the link uses --wrap=GetFrameTime.
extern "C" float __real_GetFrameTime(void);
extern "C" float __wrap_GetFrameTime(void) { return g_frameSeconds; }

extern "C" const char* __real_GetApplicationDirectory(void);
extern "C" const char* __wrap_GetApplicationDirectory(void) {
    // Hud.cpp's static MatchHudTextures resolves its images from the executable
    // directory. run.sh stages build/run with the assets the client's CMake
    // copies beside its executable, and the viewer runs there.
    static std::string dir;
    if (dir.empty()) {
        std::error_code ec;
        fs::path here = fs::current_path(ec);
        dir = here.string();
        dir += '/';
    }
    return dir.c_str();
}

extern "C" Shader LoadShaderFromMemory(const char* vs, const char* fs);
extern "C" Shader __wrap_LoadShaderFromMemory(const char* vs, const char* fs) {
    // TableScene only checks that the id is non-zero; the software backend has
    // no program objects and BeginShaderMode is a no-op below.
    Shader shader{};
    shader.id = 1;
    return shader;
}
extern "C" int GetShaderLocation(Shader shader, const char* name);
extern "C" int __wrap_GetShaderLocation(Shader shader, const char* name) {
    (void)shader; (void)name;
    return -1;
}
extern "C" void SetShaderValue(Shader shader, int loc, const void* value, int uniformType);
extern "C" void __wrap_SetShaderValue(Shader shader, int loc, const void* value, int uniformType) {
    (void)shader; (void)loc; (void)value; (void)uniformType;
}
extern "C" void BeginShaderMode(Shader shader);
extern "C" void __wrap_BeginShaderMode(Shader shader) { (void)shader; }
extern "C" void EndShaderMode(void);
extern "C" void __wrap_EndShaderMode(void) {}
extern "C" void UnloadShader(Shader shader);
extern "C" void __wrap_UnloadShader(Shader shader) { (void)shader; }

// Fonts: the game calls LoadFontEx with no glyph padding, which the software
// rasterizer lets bleed into neighbouring cells. Load with padding and a
// bilinear filter, and tag the faces for the text log.
extern "C" Font __real_LoadFontEx(const char* fileName, int fontSize, const int* codepoints, int codepointCount);
extern "C" Font __wrap_LoadFontEx(const char* fileName, int fontSize, const int* codepoints, int codepointCount) {
    std::vector<int> room;
    if (codepoints == nullptr || codepointCount == 0) {
        // Same default as raylib: the 95 printable ASCII glyphs.
        for (int cp = 32; cp <= 126; ++cp) room.push_back(cp);
        codepoints = room.data();
        codepointCount = static_cast<int>(room.size());
    }
    Font font = __real_LoadFontEx(fileName, fontSize, codepoints, codepointCount);
    if (font.texture.id != 0) SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}

// ============================================================================
// 3. Main loop state
// ============================================================================
namespace {

constexpr int kCanvasWidth = TableLayout::ImageWidth;
constexpr int kCanvasHeight = TableLayout::ImageHeight;

struct Screen {
    Ui::Viewport viewport; // 1360x768 design space mapped into the window
    float canvasScale = 1.0f; // canvas pixels -> window pixels
    float canvasX = 0.0f;
    float canvasY = 0.0f;
};

Screen MakeScreen(int width, int height) {
    const float scale = std::min(static_cast<float>(width) / kCanvasWidth,
                                 static_cast<float>(height) / kCanvasHeight);
    const float w = kCanvasWidth * scale;
    const float h = kCanvasHeight * scale;
    Screen screen;
    screen.viewport = Ui::Viewport{{(width - w) / 2.0f, (height - h) / 2.0f, w, h}, scale};
    screen.canvasScale = scale;
    screen.canvasX = (width - w) / 2.0f;
    screen.canvasY = (height - h) / 2.0f;
    return screen;
}

TableLayout::Point2 ToCanvas(const Screen& screen, float x, float y) {
    return {(x - screen.canvasX) / screen.canvasScale, (y - screen.canvasY) / screen.canvasScale};
}

TableLayout::Point2 PlanPoint(TableLayout::Point2 canvas) {
    auto p = TableLayout::Unproject(canvas);
    p.x = std::clamp(p.x, -200.0f, TableLayout::PlanWidth + 200.0f);
    p.y = std::clamp(p.y, -200.0f, TableLayout::PlanHeight + 200.0f);
    return p;
}

bool In(TableLayout::Point2 p, TableLayout::Rect r) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}

bool InRect(TableLayout::Point2 p, Rectangle r) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}

// The game's own window: everything the client keeps between frames.
struct Game {
    TableGame game;
    Ui::StartMenu startMenu;
    Ui::MatchSettingsMenu matchSettingsMenu;
    Ui::MatchResultMenu matchResultMenu;
    Ui::LoadingScreen loading;
    Ui::PauseMenu pause;
    GameOptions options;
    RadioStatus radio;
    TableOverlay overlay = TableOverlay::NONE;
    MagnifyEffect magnify{};
    bool inStartMenu = true;
    bool matchSettingsOpen = false;
    bool galleryOpen = false;
    bool draggingVolume = false;
    bool inspectingCard = false;
    bool quitRequested = false;

    Game(const fs::path& appDirectory, const fs::path& variantFile, int rounds)
        : game(TableGame::WithRounds(rounds)),
          startMenu(appDirectory, variantFile),
          matchSettingsMenu(appDirectory),
          matchResultMenu(appDirectory),
          loading(appDirectory) {
        Ui::activeLoadingScreen = &loading;
    }

    void SetOverlay(TableOverlay next) {
        overlay = next;
        draggingVolume = false;
        inspectingCard = false;
        pause = {};
        if (next != TableOverlay::NONE) game.CancelDrag();
    }
    void ResetMatch() {
        game = TableGame::WithRounds(matchSettingsMenu.SelectedRounds());
        matchResultMenu.ResetMatch();
    }
    void ReturnToTitle() {
        inStartMenu = true;
        galleryOpen = false;
        matchSettingsOpen = false;
        startMenu.AdvanceVariant();
        ResetMatch();
        SetOverlay(TableOverlay::NONE);
    }
    void BeginTransition(bool toMatch) {
        loading.Begin(toMatch);
        matchSettingsOpen = false;
        galleryOpen = false;
        SetOverlay(TableOverlay::NONE);
    }

    // The frame the client draws inside a match: the table with the HUD
    // overlay state layered over it.
    void DrawMatchFrame(TableScene& scene, TableLayout::Point2 pointer, float dt) {
        const float desiredMagnification =
            overlay == TableOverlay::NONE && !matchResultMenu.IsOpen() && inspectingCard ? 0.85f : 0.0f;
        const float magnifyBlend = 1.0f - std::exp(-12.0f * dt);
        magnify.amount += (desiredMagnification - magnify.amount) * magnifyBlend;
        magnify.focus.x += (pointer.x - magnify.focus.x) * magnifyBlend;
        magnify.focus.y += (pointer.y - magnify.focus.y) * magnifyBlend;
        game.TakeSfx(); // no sound device; the cues are consumed and dropped
        scene.Draw(game, {overlay, radio, options, pause.Focus(), pointer, &matchResultMenu}, magnify);
    }
};

} // namespace

// ============================================================================
// 4. Reporting: text log, overlaps, stage, hits, state
// ============================================================================
namespace {

std::string EscapeTabs(std::string text) {
    for (char& c : text) if (c == '\t') c = ' ';
    return text;
}

std::string TintText(Color tint) {
    return std::to_string(static_cast<int>(tint.r)) + ',' + std::to_string(static_cast<int>(tint.g)) +
           ',' + std::to_string(static_cast<int>(tint.b)) + ',' + std::to_string(static_cast<int>(tint.a));
}

struct Box {
    const TextCall* call;
    float x0, y0, x1, y1;
};

Box Bounds(const TextCall& call) {
    return {&call, call.x, call.y, call.x + call.width, call.y + call.size};
}

bool Intersects(const Box& a, const Box& b) {
    return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

// The game paints a black drop shadow a pixel or two before the same string
// in its foreground colour. Those pairs are intentional, not collisions.
bool ShadowPair(const TextCall& a, const TextCall& b) {
    const bool aShadow = a.tint.r == 0 && a.tint.g == 0 && a.tint.b == 0;
    const bool bShadow = b.tint.r == 0 && b.tint.g == 0 && b.tint.b == 0;
    if (aShadow == bShadow) return false;
    if (a.text != b.text) return false;
    const float dx = std::abs(a.x - b.x);
    const float dy = std::abs(a.y - b.y);
    return dx <= 4.0f && dy <= 4.0f;
}

void WriteTextLog(const fs::path& outDir, const std::string& name,
                  const std::map<unsigned int, std::string>& fontNames) {
    std::ofstream log(outDir / (name + ".text.tsv"));
    log << "size\tx\ty\twidth\tfont\trgba\ttext\n";
    for (const TextCall& call : g_text) {
        auto it = fontNames.find(call.font);
        const std::string face = it != fontNames.end() ? it->second
                                : call.font == 0 ? "default" : "font-" + std::to_string(call.font);
        log << call.size << '\t' << call.x << '\t' << call.y << '\t' << call.width << '\t'
            << face << '\t' << TintText(call.tint) << '\t' << EscapeTabs(call.text) << '\n';
    }
}

void ReportOverlaps(bool crop, float cx, float cy, float cw, float ch, std::FILE* out) {
    std::vector<Box> boxes;
    boxes.reserve(g_text.size());
    for (const TextCall& call : g_text) {
        if (call.text.empty()) continue;
        Box box = Bounds(call);
        if (crop && (box.x1 < cx || box.x0 > cx + cw || box.y1 < cy || box.y0 > cy + ch)) continue;
        boxes.push_back(box);
    }
    int pairs = 0;
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        for (std::size_t j = i + 1; j < boxes.size(); ++j) {
            if (!Intersects(boxes[i], boxes[j])) continue;
            if (ShadowPair(*boxes[i].call, *boxes[j].call)) continue;
            ++pairs;
            std::fprintf(out, "overlap %dx%d+%dx%d '%s' | %dx%d+%dx%d '%s'\n",
                         static_cast<int>(boxes[i].x0), static_cast<int>(boxes[i].y0),
                         static_cast<int>(boxes[i].x1 - boxes[i].x0),
                         static_cast<int>(boxes[i].y1 - boxes[i].y0), boxes[i].call->text.c_str(),
                         static_cast<int>(boxes[j].x0), static_cast<int>(boxes[j].y0),
                         static_cast<int>(boxes[j].x1 - boxes[j].x0),
                         static_cast<int>(boxes[j].y1 - boxes[j].y0), boxes[j].call->text.c_str());
        }
    }
    std::fprintf(out, "overlaps: %zu text calls, %d colliding pair%s\n", g_text.size(), pairs,
                 pairs == 1 ? "" : "s");
}

void ReportStage(std::FILE* out) {
    // Group consecutive calls that share a baseline and colour: those are a
    // single label the game painted piece by piece (shadow + face, or two
    // colours). A run wider than the window is a label that does not fit.
    std::fprintf(out, "stage: %zu text calls\n", g_text.size());
    int wide = 0;
    for (const TextCall& call : g_text) {
        const float left = call.x;
        const float right = call.x + call.width;
        if (left < 0.0f || right > static_cast<float>(kCanvasWidth) * 4.0f) {
            ++wide;
            std::fprintf(out, "  outside: '%s' x=%.1f..%.1f y=%.1f size=%.1f\n",
                         call.text.c_str(), left, right, call.y, call.size);
        }
    }
    std::fprintf(out, "stage: %d label%s painted outside the frame\n", wide, wide == 1 ? "" : "s");
}

std::string PhaseName(Phase phase) {
    switch (phase) {
        case Phase::DEAL_CHECK: return "DEAL_CHECK";
        case Phase::AWAIT_HAND_PLAY: return "AWAIT_HAND_PLAY";
        case Phase::AWAIT_HAND_CHOICE: return "AWAIT_HAND_CHOICE";
        case Phase::RESOLVE_HAND: return "RESOLVE_HAND";
        case Phase::REVEAL_DRAW: return "REVEAL_DRAW";
        case Phase::AWAIT_DRAW_CHOICE: return "AWAIT_DRAW_CHOICE";
        case Phase::RESOLVE_DRAW: return "RESOLVE_DRAW";
        case Phase::CHECK_YAKU: return "CHECK_YAKU";
        case Phase::AWAIT_DECISION: return "AWAIT_DECISION";
        case Phase::NEXT_PLAYER: return "NEXT_PLAYER";
        case Phase::ROUND_END: return "ROUND_END";
    }
    return "?";
}

std::string CardName(CardId id) {
    const Card& card = DeckRegister::At(static_cast<int>(id));
    return std::string(card.stableId) + " " + card.name;
}

std::string CardsText(const std::vector<CardId>& cards) {
    std::string out;
    for (CardId id : cards) {
        if (!out.empty()) out += ' ';
        out += CardName(id);
    }
    return out;
}

void ReportState(const Game& g, std::FILE* out) {
    const auto& rules = g.game.Rules();
    std::fprintf(out, "STATE screen=%s overlay=%s rounds=%d/%d dealer=%s player=%s phase=%s busy=%d\n",
                 g.inStartMenu ? "title" : "match",
                 g.overlay == TableOverlay::PAUSE ? "pause" : g.overlay == TableOverlay::RADIO ? "radio" : "none",
                 g.game.Round(), g.game.TotalRounds(),
                 rules.Dealer() == Player::PLAYER ? "player" : "cpu",
                 rules.CurrentPlayer() == Player::PLAYER ? "player" : "cpu",
                 PhaseName(rules.GetPhase()).c_str(), g.game.Busy() ? 1 : 0);
    std::fprintf(out, "STATE scores: you=%d cpu=%d stock=%zu misdeals=%zu\n",
                 g.game.Scores()[1], g.game.Scores()[0], rules.StockRemaining(), rules.Misdeals());
    std::fprintf(out, "STATE hand you: %s\n", CardsText(rules.Hand(Player::PLAYER)).c_str());
    std::fprintf(out, "STATE hand cpu: %s\n", CardsText(rules.Hand(Player::OPPONENT)).c_str());
    std::fprintf(out, "STATE field: %s\n", CardsText(rules.Field()).c_str());
    std::fprintf(out, "STATE captured you: %s\n", CardsText(rules.Captured(Player::PLAYER)).c_str());
    std::fprintf(out, "STATE captured cpu: %s\n", CardsText(rules.Captured(Player::OPPONENT)).c_str());
    const auto pending = rules.PendingCard();
    std::fprintf(out, "STATE pending=%s choices=%s koicalls you=%d cpu=%d\n",
                 pending ? CardName(*pending).c_str() : "-",
                 CardsText(rules.Choices()).c_str(),
                 rules.KoiCalls(Player::PLAYER), rules.KoiCalls(Player::OPPONENT));
    if (g.game.AwaitingDealerChoice()) {
        std::fprintf(out, "STATE dealer cards: left=%s right=%s (awaiting pick)\n",
                     CardName(static_cast<CardId>(g.game.DealerCards()[0])).c_str(),
                     CardName(static_cast<CardId>(g.game.DealerCards()[1])).c_str());
    }
    if (g.game.DealerSelectionActive()) {
        std::fprintf(out, "STATE dealer selection active choice=%d\n", g.game.DealerChoice());
    }
    std::fprintf(out, "STATE intro: roundintro=%d bullet=%.2f cue=%.2f timer=%.2f\n",
                 g.game.RoundIntroActive() ? 1 : 0, g.game.RoundIntroBulletProgress(),
                 g.game.RoundIntroCueOpacity(), g.game.RoundIntroTimer());
    std::fprintf(out, "STATE result: open=%d mode=%s round=%d leader=%d\n",
                 g.matchResultMenu.IsOpen() ? 1 : 0,
                 g.matchResultMenu.CurrentMode() == Ui::MatchResultMode::MATCH ? "match" : "round",
                 g.matchResultMenu.round_, g.matchResultMenu.matchFinished_ ? 1 : 0);
    std::fprintf(out, "STATE menu: variant=%d focus=%d settings=%d gallery=%d viewer=%d month=%d card=%d\n",
                 g.startMenu.Variant(), static_cast<int>(g.startMenu.Focus()),
                 g.matchSettingsOpen, g.galleryOpen, g.startMenu.GalleryViewerOpen(),
                 g.startMenu.gallery_.selectedMonth_, g.startMenu.gallery_.selectedCard_);
    std::fprintf(out, "STATE options: master=%d sfx=%d hints=%d mode=%d aa=%d rounds=%d route=%s\n",
                 g.options.masterVolume, g.options.sfxVolume, g.options.cardHints ? 1 : 0,
                 static_cast<int>(g.options.screenMode), g.options.aaSamples,
                 g.matchSettingsMenu.SelectedRounds(), g.matchSettingsMenu.SelectedRoute());
    std::fprintf(out, "STATE radio: state=%d enabled=%d station=%d '%s' volume=%.2f muted=%d '%s'\n",
                 static_cast<int>(g.radio.state), g.radio.enabled ? 1 : 0, g.radio.stationIndex,
                 g.radio.stationName.c_str(), g.radio.volume, g.radio.muted ? 1 : 0,
                 g.radio.statusText.c_str());
}

std::string KindName(int index) { return "button" + std::to_string(index + 1); }

} // namespace

// ============================================================================
// 5. main
// ============================================================================
int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: viewer <script> [width height]\n"
                             "       run it through run.sh, which stages the assets\n");
        return 2;
    }
    const std::string scriptPath = argv[1];
    const int width = argc >= 4 ? std::atoi(argv[2]) : kCanvasWidth;
    const int height = argc >= 4 ? std::atoi(argv[3]) : kCanvasHeight;

    auto envPath = [](const char* name, const fs::path& fallback) {
        const char* value = std::getenv(name);
        return fs::absolute(value != nullptr && *value != '\0' ? fs::path(value) : fallback);
    };
    const fs::path outDir = envPath("VIEWER_OUT", "out");
    const fs::path saveDir = envPath("VIEWER_SAVES", outDir / "saves");
    fs::create_directories(outDir);
    fs::create_directories(saveDir);

    // run.sh runs the viewer inside build/run, where the client's assets sit.
    const fs::path appDirectory = envPath("VIEWER_RUN_DIR", fs::current_path());
    const fs::path optionsFile = saveDir / "options.txt";

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(width, height, "Densha de Koi (viewer)");
    if (!IsWindowReady()) {
        std::fprintf(stderr, "viewer: could not create the window\n");
        return 1;
    }
    SetExitKey(KEY_NULL);

    int result = 0;
    std::map<unsigned int, std::string> fontNames;
    try {
        const std::string background = (appDirectory / "the-city.jpg").string();
        const std::string atlas = (appDirectory / "cards_atlas.png").string();
        const std::string logo = (appDirectory / "koidenscha.png").string();
        const std::string headingFont = (appDirectory / "fonts" / "Oxanium-Bold.ttf").string();
        const std::string bodyFont = (appDirectory / "fonts" / "Rajdhani-Bold.ttf").string();
        for (const std::string& path : {background, atlas, logo, headingFont, bodyFont}) {
            if (!fs::exists(path)) {
                std::fprintf(stderr, "viewer: asset missing: %s\n", path.c_str());
                return 1;
            }
        }

        TableScene scene(background.c_str(), atlas.c_str(), logo.c_str(),
                         headingFont.c_str(), bodyFont.c_str(), appDirectory / "options");
        fontNames[0] = "default";
        // Tag the faces the text log can name.
        {
            const Font heading = LoadFontEx(headingFont.c_str(), 64, nullptr, 0);
            const Font body = LoadFontEx(bodyFont.c_str(), 48, nullptr, 0);
            fontNames[heading.texture.id] = "oxanium";
            fontNames[body.texture.id] = "rajdhani";
            // The game loads its own copies; release ours so the ids stay live
            // for the process lifetime.
        }
        Game game(appDirectory, saveDir / "start-menu-variant.txt",
                  Ui::MatchSettingsMenu(appDirectory).SelectedRounds());
        game.options = LoadOptions(optionsFile);
        game.options.aaSamples = 1; // The software backend does not supersample.

        const Screen screen = MakeScreen(width, height);
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
        std::string mouseButton = "up";
        int mouseButtonFrames = 0;
        bool mouseDown = false;
        float mouseX = -1.0f, mouseY = -1.0f;
        float wheel = 0.0f;
        std::set<int> keysDown;
        std::set<int> keysPressed;

        // ---- frame ---------------------------------------------------------
        // One client frame: update, draw, write nothing. The text recorder is
        // cleared at the start so a .text.tsv describes the frame in the PNG.
        auto frame = [&](float dt) {
            g_frameSeconds = dt;
            g_virtualSeconds += dt;
            g_text.clear();
            const auto canvas = ToCanvas(screen, mouseX, mouseY);
            const bool pressed = mouseButton == "left-down";
            const bool rightPressed = mouseButton == "right-down";
            const auto movement = GetMouseDelta();

            auto pressedKey = [&](int key) { return keysPressed.count(key) != 0; };
            auto downKey = [&](int key) { return keysDown.count(key) != 0; };
            (void)downKey;

            if (game.loading.Active()) {
                if (game.loading.Update(dt)) {
                    if (game.loading.ToMatch()) {
                        game.ResetMatch();
                        game.inStartMenu = false;
                    } else {
                        game.ReturnToTitle();
                    }
                }
                if (game.inStartMenu) {
                    BeginDrawing();
                    ClearBackground(BLACK);
                    game.startMenu.DrawBackground(screen.viewport);
                    game.loading.Draw(screen.viewport);
                    EndDrawing();
                } else {
                    game.DrawMatchFrame(scene, canvas, dt);
                }
                mouseButton = mouseDown ? (mouseButton == "right-down" ? "right-down" : "left-down") : "up";
                keysPressed.clear();
                return;
            }

            const bool escape = pressedKey(KEY_ESCAPE);
            if (!game.inStartMenu && pressedKey(KEY_BACKSPACE)) game.BeginTransition(false);
            const bool pauseKey = !game.inStartMenu && (pressedKey(KEY_ESCAPE) || pressedKey(KEY_P));
            if (pauseKey) {
                game.SetOverlay(game.overlay == TableOverlay::PAUSE ? TableOverlay::NONE
                                                                   : TableOverlay::PAUSE);
            }
            if (pressedKey(KEY_M)) game.radio.muted = !game.radio.muted;

            if (game.inStartMenu) {
                if (escape && (game.overlay != TableOverlay::NONE ||
                               (game.galleryOpen && !game.startMenu.GalleryViewerOpen()))) {
                    game.SetOverlay(TableOverlay::NONE);
                    game.galleryOpen = false;
                }
                if (game.galleryOpen) {
                    const int vertical = pressedKey(KEY_DOWN) - pressedKey(KEY_UP);
                    const int horizontal = pressedKey(KEY_RIGHT) - pressedKey(KEY_LEFT);
                    const Ui::GalleryInput galleryInput{
                        canvas, movement.x != 0.0f || movement.y != 0.0f, pressed, mouseDown,
                        rightPressed, wheel, vertical, horizontal,
                        pressedKey(KEY_ENTER), escape, game.options.aaSamples};
                    if (game.startMenu.UpdateGallery(galleryInput)) game.galleryOpen = false;
                } else if (game.overlay == TableOverlay::PAUSE) {
                    const Ui::PauseInput input{canvas, movement.x != 0 || movement.y != 0, pressed,
                                               mouseDown, pressedKey(KEY_DOWN) - pressedKey(KEY_UP),
                                               pressedKey(KEY_RIGHT) - pressedKey(KEY_LEFT),
                                               pressedKey(KEY_ENTER) || pressedKey(KEY_SPACE)};
                    if (game.startMenu.OptionsMenu().Update(game.options, input,
                                                            {width, height}, {width, height}))
                        game.SetOverlay(TableOverlay::NONE);
                } else if (game.overlay == TableOverlay::RADIO) {
                    if (pressed) {
                        if (In(canvas, TableHud::ResumeBottomButton)) game.SetOverlay(TableOverlay::NONE);
                        else if (In(canvas, TableHud::NewMatchButton)) game.BeginTransition(true);
                        else if (In(canvas, TableHud::RadioPowerButton))
                            game.radio.enabled = !game.radio.enabled;
                        else if (In(canvas, TableHud::RadioMuteButton)) game.radio.muted = !game.radio.muted;
                        else if (In(canvas, TableHud::VolumeHitBox)) game.draggingVolume = true;
                        else {
                            for (std::size_t i = 0; i < TableHud::StationSlotCount; ++i)
                                if (In(canvas, TableHud::StationButton(i))) {
                                    game.radio.stationIndex = static_cast<int>(i);
                                    break;
                                }
                        }
                    }
                    if (game.draggingVolume && mouseDown)
                        game.radio.volume = std::clamp(
                            (canvas.x - TableHud::VolumeBar.x) / TableHud::VolumeBar.width, 0.0f, 1.0f);
                    if (!mouseDown) game.draggingVolume = false;
                } else if (game.matchSettingsOpen) {
                    const int vertical = pressedKey(KEY_DOWN) - pressedKey(KEY_UP);
                    const int horizontal = pressedKey(KEY_RIGHT) - pressedKey(KEY_LEFT);
                    const auto matchAction = game.matchSettingsMenu.Update(
                        canvas, movement.x != 0.0f || movement.y != 0.0f, pressed,
                        vertical, horizontal,
                        pressedKey(KEY_ENTER) || pressedKey(KEY_SPACE), escape);
                    if (matchAction == Ui::MatchSettingsAction::BACK) {
                        game.matchSettingsOpen = false;
                    } else if (matchAction == Ui::MatchSettingsAction::START) {
                        game.BeginTransition(true);
                    }
                } else if (game.overlay == TableOverlay::NONE) {
                    const auto action = game.startMenu.Update(
                        canvas, movement.x != 0 || movement.y != 0, pressed,
                        pressedKey(KEY_DOWN) - pressedKey(KEY_UP),
                        pressedKey(KEY_ENTER) || pressedKey(KEY_SPACE));
                    switch (action) {
                        case Ui::StartMenuAction::PLAY:
                            game.matchSettingsMenu.ResetFocus();
                            game.matchSettingsOpen = true;
                            break;
                        case Ui::StartMenuAction::OPTIONS: game.SetOverlay(TableOverlay::PAUSE); break;
                        case Ui::StartMenuAction::RADIO: game.SetOverlay(TableOverlay::RADIO); break;
                        case Ui::StartMenuAction::GALLERY: game.galleryOpen = true; break;
                        case Ui::StartMenuAction::EXIT: game.quitRequested = true; break;
                        case Ui::StartMenuAction::NONE: break;
                    }
                }

                BeginDrawing();
                ClearBackground(BLACK);
                game.startMenu.DrawBackground(screen.viewport);
                if (game.matchSettingsOpen) game.matchSettingsMenu.Draw(screen.viewport, canvas);
                if (game.galleryOpen) game.startMenu.DrawGallery(screen.viewport);
                else if (game.overlay == TableOverlay::PAUSE)
                    game.startMenu.DrawOptions(screen.viewport, game.options, {width, height});
                else if (game.overlay == TableOverlay::RADIO)
                    game.startMenu.DrawRadio(screen.viewport, game.radio);
                EndDrawing();
                mouseButton = mouseDown ? (mouseButton == "right-down" ? "right-down" : "left-down") : "up";
                keysPressed.clear();
                wheel = 0.0f;
                return;
            }

            // ---- in a match ------------------------------------------------
            if (game.overlay != TableOverlay::NONE) {
                if (pressed && In(canvas, TableHud::RadioMenuButton)) {
                    game.SetOverlay(game.overlay == TableOverlay::RADIO ? TableOverlay::NONE
                                                                       : TableOverlay::RADIO);
                } else if (pressed && In(canvas, TableHud::PauseButton)) {
                    game.SetOverlay(game.overlay == TableOverlay::PAUSE ? TableOverlay::NONE
                                                                       : TableOverlay::PAUSE);
                } else if (pressed && In(canvas, TableHud::HomeButton)) {
                    game.BeginTransition(false);
                    mouseButton = "up";
                    keysPressed.clear();
                    return;
                } else if (game.overlay == TableOverlay::PAUSE) {
                    const Ui::PauseInput input{canvas, movement.x != 0 || movement.y != 0, pressed,
                                               mouseDown, pressedKey(KEY_DOWN) - pressedKey(KEY_UP),
                                               pressedKey(KEY_RIGHT) - pressedKey(KEY_LEFT),
                                               pressedKey(KEY_ENTER)};
                    if (game.pause.Update(game.options, input, {width, height}, {width, height}))
                        game.SetOverlay(TableOverlay::NONE);
                } else {
                    if (pressed) {
                        if (In(canvas, TableHud::ResumeBottomButton)) game.SetOverlay(TableOverlay::NONE);
                        else if (In(canvas, TableHud::RadioPowerButton))
                            game.radio.enabled = !game.radio.enabled;
                        else if (In(canvas, TableHud::RadioMuteButton)) game.radio.muted = !game.radio.muted;
                        else if (In(canvas, TableHud::VolumeHitBox)) game.draggingVolume = true;
                        else if (In(canvas, TableHud::NewMatchButton)) {
                            game.ResetMatch();
                            game.SetOverlay(TableOverlay::NONE);
                        } else {
                            for (std::size_t i = 0; i < TableHud::StationSlotCount; ++i)
                                if (In(canvas, TableHud::StationButton(i))) {
                                    game.radio.stationIndex = static_cast<int>(i);
                                    break;
                                }
                        }
                    }
                    if (game.draggingVolume && mouseDown)
                        game.radio.volume = std::clamp(
                            (canvas.x - TableHud::VolumeBar.x) / TableHud::VolumeBar.width, 0.0f, 1.0f);
                    if (!mouseDown) game.draggingVolume = false;
                }
            } else if (!pauseKey) {
                if (game.matchResultMenu.IsOpen()) {
                    game.inspectingCard = false;
                    if (pressed && In(canvas, TableHud::RadioMenuButton)) {
                        game.SetOverlay(TableOverlay::RADIO);
                    } else if (pressed && In(canvas, TableHud::PauseButton)) {
                        game.SetOverlay(TableOverlay::PAUSE);
                    } else if (pressed && In(canvas, TableHud::HomeButton)) {
                        game.BeginTransition(false);
                        mouseButton = "up";
                        keysPressed.clear();
                        return;
                    } else {
                        const int horizontal = pressedKey(KEY_RIGHT) - pressedKey(KEY_LEFT);
                        const auto resultAction = game.matchResultMenu.Update(
                            canvas, movement.x != 0.0f || movement.y != 0.0f, pressed,
                            horizontal, pressedKey(KEY_ENTER) || pressedKey(KEY_SPACE));
                        if (resultAction == Ui::MatchResultAction::BACK) {
                            game.BeginTransition(false);
                            mouseButton = "up";
                            keysPressed.clear();
                            return;
                        } else if (resultAction == Ui::MatchResultAction::NEXT) {
                            if (game.matchResultMenu.CurrentMode() == Ui::MatchResultMode::ROUND) {
                                if (game.game.MatchFinished()) {
                                    game.matchResultMenu.ShowMatchSummary();
                                } else {
                                    game.matchResultMenu.CloseRoundPopup();
                                    game.game.NextRound();
                                }
                            } else {
                                game.ResetMatch();
                            }
                        }
                    }
                } else {
                    if (rightPressed) {
                        if (game.game.Dragging()) game.game.CancelDrag();
                        else game.inspectingCard = game.game.HasCardAt(PlanPoint(canvas));
                    }
                    if (pressed) {
                        if (game.game.AwaitingDealerChoice() && In(canvas, TableHud::DealerCardLeft)) {
                            game.game.SelectDealerCard(0);
                        } else if (game.game.AwaitingDealerChoice() && In(canvas, TableHud::DealerCardRight)) {
                            game.game.SelectDealerCard(0);
                        } else if (In(canvas, TableHud::RadioMenuButton)) {
                            game.SetOverlay(TableOverlay::RADIO);
                        } else if (In(canvas, TableHud::PauseButton)) {
                            game.SetOverlay(TableOverlay::PAUSE);
                        } else if (In(canvas, TableHud::HomeButton)) {
                            game.BeginTransition(false);
                            mouseButton = "up";
                            keysPressed.clear();
                            return;
                        } else if (game.game.Rules().GetPhase() == Phase::AWAIT_DECISION && !game.game.Busy() &&
                                   game.game.Rules().CurrentPlayer() == Player::PLAYER &&
                                   In(canvas, TableHud::KoiKoiButton)) {
                            game.game.Choose(Decision::KOI_KOI);
                        } else if (game.game.Rules().GetPhase() == Phase::AWAIT_DECISION && !game.game.Busy() &&
                                   game.game.Rules().CurrentPlayer() == Player::PLAYER &&
                                   In(canvas, TableHud::ShobuButton)) {
                            game.game.Choose(Decision::SHOUBU);
                        } else {
                            game.game.PointerDown(PlanPoint(canvas));
                        }
                    }
                    if (game.game.Dragging()) game.game.PointerMove(PlanPoint(canvas));
                    if (mouseButton == "left-release") game.game.PointerUp(PlanPoint(canvas));
                    if (game.overlay == TableOverlay::NONE) game.game.Update(dt);
                    if (game.game.Rules().GetPhase() == Phase::ROUND_END && !game.game.Busy())
                        game.matchResultMenu.SyncRoundEnd(game.game);
                }
            }
            game.DrawMatchFrame(scene, canvas, dt);
            mouseButton = mouseDown ? (mouseButton == "right-down" ? "right-down" : "left-down") : "up";
            keysPressed.clear();
            wheel = 0.0f;
        };

        // ---- shot ----------------------------------------------------------
        // The frame is drawn into a 1360x768 render texture and letterboxed
        // into the window, so the window framebuffer is read back as-is. The
        // software rasterizer hands back BGRA; the bytes are swapped.
        auto shot = [&](const std::string& name, int cx, int cy, int cw, int ch) {
            std::vector<unsigned char> raw(static_cast<std::size_t>(width) * height * 4);
            rlCopyFramebuffer(0, 0, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, raw.data());
            cx = std::clamp(cx, 0, width - 1);
            cy = std::clamp(cy, 0, height - 1);
            cw = std::clamp(cw, 1, width - cx);
            ch = std::clamp(ch, 1, height - cy);
            std::vector<unsigned char> crop(static_cast<std::size_t>(cw) * ch * 4);
            for (int y = 0; y < ch; ++y) {
                for (int x = 0; x < cw; ++x) {
                    const unsigned char* src = &raw[(static_cast<std::size_t>(cy + y) * width + cx + x) * 4];
                    unsigned char* dst = &crop[(static_cast<std::size_t>(y) * cw + x) * 4];
                    dst[0] = src[2];
                    dst[1] = src[1];
                    dst[2] = src[0];
                    dst[3] = 255;
                }
            }
            Image image{crop.data(), cw, ch, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            const fs::path file = outDir / (name + ".png");
            if (!ExportImage(image, file.string().c_str())) {
                std::fprintf(stderr, "viewer: cannot write %s\n", file.string().c_str());
                return false;
            }
            if (g_writeTextLog) WriteTextLog(outDir, name, fontNames);
            std::printf("shot %s %dx%d (%zu text calls)\n", file.string().c_str(), cw, ch, g_text.size());
            return true;
        };

        // ---- script --------------------------------------------------------
        std::ifstream script(scriptPath);
        if (!script) {
            std::fprintf(stderr, "viewer: cannot open %s\n", scriptPath.c_str());
            CloseWindow();
            return 1;
        }
        std::string line;
        int lineNo = 0;
        int problems = 0;
        auto split = [](const std::string& text) {
            std::vector<std::string> parts;
            std::istringstream stream(text);
            std::string word;
            while (stream >> word) parts.push_back(word);
            return parts;
        };

        while (std::getline(script, line)) {
            ++lineNo;
            if (line.empty() || line[0] == '#') continue;
            const std::vector<std::string> p = split(line);
            if (p.empty()) continue;
            const std::string& c = p[0];
            auto num = [&](std::size_t i, double fallback) {
                return p.size() > i ? std::atof(p[i].c_str()) : fallback;
            };
            auto word = [&](std::size_t i, const char* fallback) {
                return p.size() > i ? p[i] : std::string(fallback);
            };
            auto fail = [&](const std::string& why) {
                std::printf("line %d: %s\n", lineNo, why.c_str());
                ++problems;
            };

            if (c == "frames") {
                const int n = static_cast<int>(num(1, 1));
                const float dt = static_cast<float>(num(2, 1000.0 / 60.0) / 1000.0);
                for (int i = 0; i < n; ++i) frame(dt);
            } else if (c == "shot" && p.size() >= 2) {
                if (p.size() >= 6) {
                    if (!shot(p[1], static_cast<int>(num(2, 0)), static_cast<int>(num(3, 0)),
                              static_cast<int>(num(4, width)), static_cast<int>(num(5, height))))
                        ++problems;
                } else if (!shot(p[1], 0, 0, width, height)) {
                    ++problems;
                }
            } else if (c == "textlog" && p.size() >= 2) {
                g_writeTextLog = p[1] == "on";
            } else if (c == "stage") {
                ReportStage(stdout);
            } else if (c == "overlaps") {
                if (p.size() >= 5) {
                    ReportOverlaps(true, static_cast<float>(num(1, 0)), static_cast<float>(num(2, 0)),
                                   static_cast<float>(num(3, 0)), static_cast<float>(num(4, 0)), stdout);
                } else {
                    ReportOverlaps(false, 0, 0, 0, 0, stdout);
                }
            } else if (c == "move" && p.size() >= 3) {
                mouseX = static_cast<float>(num(1, 0));
                mouseY = static_cast<float>(num(2, 0));
            } else if ((c == "click" || c == "rclick") && p.size() >= 3) {
                mouseX = static_cast<float>(num(1, 0));
                mouseY = static_cast<float>(num(2, 0));
                mouseButton = c == "click" ? "left-down" : "right-down";
                mouseDown = true;
                frame(g_frameSeconds);
                mouseButton = "left-release";
                mouseDown = false;
                frame(g_frameSeconds);
            } else if ((c == "press" || c == "rpress") && p.size() >= 3) {
                mouseX = static_cast<float>(num(1, 0));
                mouseY = static_cast<float>(num(2, 0));
                mouseButton = c == "press" ? "left-down" : "right-down";
                mouseDown = true;
                frame(g_frameSeconds);
            } else if (c == "release") {
                mouseButton = "left-release";
                mouseDown = false;
                frame(g_frameSeconds);
            } else if ((c == "clickhit" || c == "rclickhit" || c == "hoverhit") && p.size() >= 3) {
                // Press the middle of a hit box, by kind or by label. The boxes
                // are the ones this viewer lists with `hits`; for the shell's
                // own rectangles the script uses plain `click x y`.
                const std::string kind = p[1];
                const std::string text = p.size() > 2 ? p[2] : "";
                int fx = -1, fy = -1;
                for (const auto& hit : std::vector<std::pair<std::string, Rectangle>>{
                         {"button1", screen.viewport.Rec(Ui::StartMenu::Buttons[0])},
                         {"button2", screen.viewport.Rec(Ui::StartMenu::Buttons[1])},
                         {"button3", screen.viewport.Rec(Ui::StartMenu::Buttons[2])},
                         {"button4", screen.viewport.Rec(Ui::StartMenu::Buttons[3])},
                         {"button5", screen.viewport.Rec(Ui::StartMenu::Buttons[4])},
                     }) {
                    if (kind == "start" && (text.empty() || hit.first.find(text) != std::string::npos)) {
                        fx = static_cast<int>(hit.second.x + hit.second.width / 2);
                        fy = static_cast<int>(hit.second.y + hit.second.height / 2);
                        break;
                    }
                }
                if (fx < 0) {
                    fail("no hit box named '" + kind + "'");
                } else if (c == "hoverhit") {
                    mouseX = static_cast<float>(fx);
                    mouseY = static_cast<float>(fy);
                    frame(g_frameSeconds);
                } else {
                    mouseX = static_cast<float>(fx);
                    mouseY = static_cast<float>(fy);
                    mouseButton = c == "clickhit" ? "left-down" : "right-down";
                    mouseDown = true;
                    frame(g_frameSeconds);
                    mouseButton = "left-release";
                    mouseDown = false;
                    frame(g_frameSeconds);
                }
            } else if (c == "drag" && p.size() >= 5) {
                mouseX = static_cast<float>(num(1, 0));
                mouseY = static_cast<float>(num(2, 0));
                mouseButton = "left-down";
                mouseDown = true;
                frame(g_frameSeconds);
                mouseX = static_cast<float>(num(3, 0));
                mouseY = static_cast<float>(num(4, 0));
                frame(g_frameSeconds);
                mouseButton = "left-release";
                mouseDown = false;
                frame(g_frameSeconds);
            } else if (c == "hold" && p.size() >= 4) {
                mouseX = static_cast<float>(num(1, 0));
                mouseY = static_cast<float>(num(2, 0));
                mouseButton = "left-down";
                mouseDown = true;
                const int n = static_cast<int>(num(3, 1));
                for (int i = 0; i < n; ++i) frame(g_frameSeconds);
                mouseButton = "left-release";
                mouseDown = false;
                frame(g_frameSeconds);
            } else if (c == "scroll" && p.size() >= 4) {
                mouseX = static_cast<float>(num(1, 0));
                mouseY = static_cast<float>(num(2, 0));
                wheel = static_cast<float>(num(3, 0));
                frame(g_frameSeconds);
            } else if (c == "key" && p.size() >= 2) {
                const int key = std::atoi(p[1].c_str());
                keysDown.insert(key);
                keysPressed.insert(key);
                frame(g_frameSeconds);
                keysDown.erase(key);
            } else if (c == "keydown" && p.size() >= 2) {
                keysDown.insert(std::atoi(p[1].c_str()));
            } else if (c == "keyup" && p.size() >= 2) {
                keysDown.erase(std::atoi(p[1].c_str()));
            } else if (c == "escape") {
                keysDown.insert(KEY_ESCAPE);
                keysPressed.insert(KEY_ESCAPE);
                frame(g_frameSeconds);
                keysDown.erase(KEY_ESCAPE);
            } else if (c == "hits") {
                const std::string kind = word(1, "all");
                auto dump = [&](const char* name, TableLayout::Rect rect) {
                    const Rectangle r = screen.viewport.Rec(rect);
                    if (kind == "all" || kind == name || kind == std::string(name) + "s") {
                        std::printf("%-10s %4d %4d %4d %4d\n", name, static_cast<int>(r.x),
                                    static_cast<int>(r.y), static_cast<int>(r.width),
                                    static_cast<int>(r.height));
                    }
                    (void)rect;
                };
                if (kind == "all" || kind == "start") {
                    for (std::size_t i = 0; i < Ui::StartMenu::Buttons.size(); ++i) {
                        const std::string name = KindName(static_cast<int>(i));
                        dump(name.c_str(), Ui::StartMenu::Buttons[i]);
                    }
                    dump("gallery-back", Ui::StartMenu::GalleryBack);
                }
                if (kind == "all" || kind == "hud") {
                    dump("koikoi", TableHud::KoiKoiButton);
                    dump("shobu", TableHud::ShobuButton);
                    dump("roundend", TableHud::RoundEndButton);
                    dump("dealerleft", TableHud::DealerCardLeft);
                    dump("dealerright", TableHud::DealerCardRight);
                    dump("radio", TableHud::RadioMenuButton);
                    dump("pause", TableHud::PauseButton);
                    dump("home", TableHud::HomeButton);
                    dump("resume", TableHud::ResumeBottomButton);
                    dump("newmatch", TableHud::NewMatchButton);
                    dump("power", TableHud::RadioPowerButton);
                    dump("mute", TableHud::RadioMuteButton);
                    dump("volume", TableHud::VolumeHitBox);
                    for (std::size_t i = 0; i < TableHud::StationSlotCount; ++i) {
                        dump(("station" + std::to_string(i + 1)).c_str(), TableHud::StationButton(i));
                    }
                }
                if (kind == "all" || kind == "result") {
                    using namespace Ui::MatchResultLayout;
                    const auto rec = [&](TableLayout::Rect art) {
                        return screen.viewport.Rec({Ui::MatchResultPanel.x + art.x * Ui::MatchResultArtScale,
                                                    Ui::MatchResultPanel.y + art.y * Ui::MatchResultArtScale,
                                                    art.width * Ui::MatchResultArtScale,
                                                    art.height * Ui::MatchResultArtScale});
                    };
                    const Rectangle back = rec(BackButton);
                    const Rectangle next = rec(NextButton);
                    std::printf("%-10s %4d %4d %4d %4d\n", "resultback", static_cast<int>(back.x),
                                static_cast<int>(back.y), static_cast<int>(back.width),
                                static_cast<int>(back.height));
                    std::printf("%-10s %4d %4d %4d %4d\n", "resultnext", static_cast<int>(next.x),
                                static_cast<int>(next.y), static_cast<int>(next.width),
                                static_cast<int>(next.height));
                }
            } else if (c == "state") {
                ReportState(game, stdout);
            } else if (c == "title") {
                game.inStartMenu = true;
                game.galleryOpen = false;
                game.matchSettingsOpen = false;
                game.SetOverlay(TableOverlay::NONE);
                frame(g_frameSeconds);
            } else if (c == "variant" && p.size() >= 2) {
                const int n = std::atoi(p[1].c_str());
                if (n < 1 || n > 4) { fail("variant must be 1..4"); }
                else {
                    while (game.startMenu.Variant() != n - 1) game.startMenu.AdvanceVariant();
                    frame(g_frameSeconds);
                }
            } else if (c == "menu" && p.size() >= 2) {
                game.inStartMenu = true;
                const std::string what = p[1];
                game.galleryOpen = what == "gallery";
                game.matchSettingsOpen = what == "settings";
                game.SetOverlay(what == "options" ? TableOverlay::PAUSE
                                : what == "radio" ? TableOverlay::RADIO : TableOverlay::NONE);
                frame(g_frameSeconds);
            } else if (c == "settingsrow" && p.size() >= 2) {
                // Keyboard row selection: 0 rounds, 1 route, 2 bottom buttons.
                game.matchSettingsOpen = true;
                const int row = std::atoi(p[1].c_str());
                game.matchSettingsMenu.focusRow_ = std::clamp(row, 0, 2);
                frame(g_frameSeconds);
            } else if (c == "settingsrounds" && p.size() >= 2) {
                int index = std::atoi(p[1].c_str());
                game.matchSettingsMenu.roundIndex_ = std::clamp(index, 0, 3);
                frame(g_frameSeconds);
            } else if (c == "optionsrow" && p.size() >= 2) {
                game.inStartMenu = true;
                game.SetOverlay(TableOverlay::PAUSE);
                game.startMenu.OptionsMenu().focus_ =
                    static_cast<Ui::OptionRow>(std::clamp(std::atoi(p[1].c_str()), 0, 5));
                frame(g_frameSeconds);
            } else if (c == "option" && p.size() >= 3) {
                // Set a GameOptions field directly: master, sfx, hints, mode, aa.
                const std::string field = p[1];
                const int value = std::atoi(p[2].c_str());
                if (field == "master") game.options.masterVolume = std::clamp(value, 0, 100);
                else if (field == "sfx") game.options.sfxVolume = std::clamp(value, 0, 100);
                else if (field == "hints") game.options.cardHints = value != 0;
                else if (field == "mode") game.options.screenMode = static_cast<ScreenMode>(std::clamp(value, 0, 2));
                else if (field == "aa") game.options.aaSamples = std::clamp(value, 1, 4);
                else fail("option field must be master, sfx, hints, mode or aa");
                frame(g_frameSeconds);
            } else if (c == "radio" && p.size() >= 3) {
                const std::string field = p[1];
                if (field == "power") game.radio.enabled = std::atoi(p[2].c_str()) != 0;
                else if (field == "mute") game.radio.muted = std::atoi(p[2].c_str()) != 0;
                else if (field == "station") game.radio.stationIndex = std::atoi(p[2].c_str());
                else if (field == "state")
                    game.radio.state = static_cast<RadioState>(std::clamp(std::atoi(p[2].c_str()), 0, 3));
                else if (field == "status") game.radio.statusText = p.size() > 3 ? p[3] : "";
                else if (field == "name") game.radio.stationName = p.size() > 3 ? p[3] : "";
                else fail("radio field must be power, mute, station, state, status or name");
                if (p.size() >= 3) game.radio.volume = p.size() >= 4 && field == "volume"
                                                      ? std::clamp(static_cast<float>(std::atof(p[3].c_str())), 0.0f, 1.0f)
                                                      : game.radio.volume;
                frame(g_frameSeconds);
            } else if (c == "radiovolume" && p.size() >= 2) {
                game.radio.volume = std::clamp(static_cast<float>(num(1, 0.7)), 0.0f, 1.0f);
                frame(g_frameSeconds);
            } else if (c == "gallery" && p.size() >= 3) {
                // Month 1..12 and card 0..3, set directly through the gallery.
                game.inStartMenu = true;
                game.galleryOpen = true;
                int month = std::clamp(std::atoi(p[1].c_str()), 1, 12);
                int card = std::clamp(std::atoi(p[2].c_str()), 0, 3);
                Ui::GalleryInput input{};
                input.vertical = month - game.startMenu.gallery_.selectedMonth_;
                game.startMenu.UpdateGallery(input);
                input.vertical = 0;
                input.horizontal = card - game.startMenu.gallery_.selectedCard_;
                game.startMenu.UpdateGallery(input);
                // Let the month fade finish on the scripted clock.
                for (int i = 0; i < 12; ++i) frame(1.0f / 60.0f);
            } else if (c == "gallerymonth" && p.size() >= 2) {
                game.inStartMenu = true;
                game.galleryOpen = true;
                const int month = std::clamp(std::atoi(p[1].c_str()), 1, 12);
                Ui::GalleryInput input{};
                input.vertical = month - game.startMenu.gallery_.selectedMonth_;
                game.startMenu.UpdateGallery(input);
                for (int i = 0; i < 12; ++i) frame(1.0f / 60.0f);
            } else if (c == "viewer3d") {
                game.inStartMenu = true;
                game.galleryOpen = true;
                Ui::GalleryInput input{};
                input.enter = true;
                game.startMenu.UpdateGallery(input);
                for (int i = 0; i < 20; ++i) frame(1.0f / 60.0f);
            } else if (c == "viewerdrag" && p.size() >= 4) {
                mouseX = static_cast<float>(num(1, 0));
                mouseY = static_cast<float>(num(2, 0));
                mouseButton = "left-down";
                mouseDown = true;
                for (int i = 0; i < 3; ++i) frame(1.0f / 60.0f);
                mouseX += static_cast<float>(num(3, 0));
                mouseY += static_cast<float>(num(4, 0));
                for (int i = 0; i < 10; ++i) frame(1.0f / 60.0f);
                mouseButton = "left-release";
                mouseDown = false;
                for (int i = 0; i < 5; ++i) frame(1.0f / 60.0f);
            } else if (c == "viewerclose") {
                Ui::GalleryInput input{};
                input.escape = true;
                game.startMenu.UpdateGallery(input);
                for (int i = 0; i < 5; ++i) frame(1.0f / 60.0f);
            } else if (c == "loading" && p.size() >= 2) {
                // The loading screens are scripted directly: 1..4, with the
                // three-second hold cut to a couple of frames.
                const int which = std::clamp(std::atoi(p[1].c_str()), 1, 4) - 1;
                game.loading.Begin(true);
                game.loading.chosen_ = which;
                game.loading.elapsed_ = 2.0f * Ui::LoadingScreen::FadeSeconds; // first fade complete
                frame(g_frameSeconds);
            } else if (c == "match" || c == "matchdealer") {
                const bool dealer = c == "matchdealer";
                int rounds = game.matchSettingsMenu.SelectedRounds();
                std::uint32_t seed = 1;
                if (p.size() >= 2) rounds = std::max(1, std::atoi(p[1].c_str()));
                if (p.size() >= 3) seed = static_cast<std::uint32_t>(std::strtoul(p[2].c_str(), nullptr, 10));
                if (dealer) {
                    // The unseeded constructor opens on the dealer draw, as the
                    // client does on a new match.
                    game.game = TableGame();
                    game.matchResultMenu.ResetMatch();
                } else {
                    game.game = TableGame(seed, rounds);
                    game.matchResultMenu.ResetMatch();
                }
                game.inStartMenu = false;
                game.galleryOpen = false;
                game.matchSettingsOpen = false;
                game.SetOverlay(TableOverlay::NONE);
                frame(g_frameSeconds);
            } else if (c == "pickdealer" && p.size() >= 2) {
                game.game.SelectDealerCard(std::clamp(std::atoi(p[1].c_str()), 0, 1));
                for (int i = 0; i < 90; ++i) frame(1.0f / 60.0f);
            } else if (c == "await") {
                // Advance the real controller until the player may act.
                for (int i = 0; i < 3000; ++i) {
                    const auto& rules = game.game.Rules();
                    const bool ready = !game.game.Busy() &&
                        (rules.GetPhase() == Phase::AWAIT_HAND_PLAY ||
                         rules.GetPhase() == Phase::AWAIT_DRAW_CHOICE ||
                         rules.GetPhase() == Phase::AWAIT_DECISION ||
                         rules.GetPhase() == Phase::ROUND_END);
                    if (ready && rules.CurrentPlayer() == Player::PLAYER) break;
                    game.game.Update(1.0f / 60.0f);
                }
                frame(g_frameSeconds);
            } else if (c == "playcard" && p.size() >= 2) {
                const int index = std::atoi(p[1].c_str());
                bool played = false;
                const auto hand = game.game.Rules().Hand(Player::PLAYER);
                if (index >= 0 && index < static_cast<int>(hand.size()) && !game.game.Busy()) {
                    auto h = TableLayout::PlayerHand[static_cast<std::size_t>(index)];
                    const TableLayout::Point2 from{h.x + h.width / 2, h.y + h.height / 2};
                    game.game.PointerDown(from);
                    // Find a field slot of the same month, else drop on the field.
                    TableLayout::Point2 to{420.0f, 360.0f};
                    for (CardId field : game.game.Rules().Field()) {
                        if (DeckRegister::At(static_cast<int>(field)).month !=
                        DeckRegister::At(static_cast<int>(hand[static_cast<std::size_t>(index)])).month)
                            continue;
                        const auto f = game.game.FieldRect(field);
                        to = {f.x + f.width / 2, f.y + f.height / 2};
                        break;
                    }
                    game.game.PointerMove(to);
                    game.game.PointerUp(to);
                    played = game.game.Busy() || game.game.Rules().GetPhase() != Phase::AWAIT_HAND_PLAY;
                    for (int i = 0; i < 20; ++i) frame(1.0f / 60.0f);
                }
                if (!played) fail("card was not played");
            } else if (c == "playmatch") {
                // Play the whole match with the viewer's simple player: always
                // the first playable card, Shobu on every decision so the round
                // ends, and Next on each result panel.
                for (int step = 0; step < 4000; ++step) {
                    if (game.matchResultMenu.IsOpen()) {
                        if (game.matchResultMenu.CurrentMode() == Ui::MatchResultMode::ROUND) {
                            if (game.game.MatchFinished()) game.matchResultMenu.ShowMatchSummary();
                            else {
                                game.matchResultMenu.CloseRoundPopup();
                                game.game.NextRound();
                            }
                        } else {
                            break;
                        }
                        frame(g_frameSeconds);
                        continue;
                    }
                    if (game.game.AwaitingDealerChoice()) {
                        game.game.SelectDealerCard(0);
                        frame(g_frameSeconds);
                        continue;
                    }
                    const auto& rules = game.game.Rules();
                    if (game.game.Busy()) {
                        frame(g_frameSeconds);
                        continue;
                    }
                    if (rules.GetPhase() == Phase::AWAIT_HAND_PLAY && rules.CurrentPlayer() == Player::PLAYER) {
                        const auto hand = rules.Hand(Player::PLAYER);
                        if (hand.empty()) { frame(g_frameSeconds); continue; }
                        auto h = TableLayout::PlayerHand[0];
                        const TableLayout::Point2 from{h.x + h.width / 2, h.y + h.height / 2};
                        TableLayout::Point2 to{420.0f, 360.0f};
                        for (CardId field : rules.Field()) {
                            if (DeckRegister::At(static_cast<int>(field)).month != DeckRegister::At(static_cast<int>(hand[0])).month) continue;
                            const auto f = game.game.FieldRect(field);
                            to = {f.x + f.width / 2, f.y + f.height / 2};
                            break;
                        }
                        game.game.PointerDown(from);
                        game.game.PointerMove(to);
                        game.game.PointerUp(to);
                        frame(g_frameSeconds);
                        continue;
                    }
                    if (rules.GetPhase() == Phase::AWAIT_DECISION && rules.CurrentPlayer() == Player::PLAYER) {
                        game.game.Choose(Decision::SHOUBU);
                        frame(g_frameSeconds);
                        continue;
                    }
                    game.game.Update(1.0f / 60.0f);
                    g_frameSeconds = 1.0f / 60.0f;
                    // Draw frames directly to keep the log quiet.
                    BeginDrawing();
                    game.DrawMatchFrame(scene, {-1, -1}, 1.0f / 60.0f);
                    EndDrawing();
                }
            } else if (c == "result" && p.size() >= 2) {
                if (p[1] == "match") {
                    game.matchResultMenu.ShowMatchSummary();
                } else {
                    game.matchResultMenu.SyncRoundEnd(game.game);
                }
                frame(g_frameSeconds);
            } else if (c == "dealerroster") {
                game.inStartMenu = true;
                game.matchSettingsOpen = true;
                frame(g_frameSeconds);
            } else if (c == "theme") {
                // The client has one palette; the command exists so a script
                // that was written for SPACESHIP fails loudly, not silently.
                fail("theme does not exist in Densha de Koi");
            } else {
                fail("unknown command '" + c + "'");
            }
            std::fflush(stdout);
            if (game.quitRequested) break;
        }

        Ui::activeLoadingScreen = nullptr;
        CloseWindow();
        if (problems > 0) {
            std::printf("viewer: %d script line(s) could not be carried out\n", problems);
            result = 3;
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "viewer: %s\n", error.what());
        CloseWindow();
        result = 1;
    }
    return result;
}
