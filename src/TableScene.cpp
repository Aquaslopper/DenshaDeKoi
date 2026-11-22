#include "DeckRegister.hpp"
#include "TableScene.hpp"
#include "CardInspection.hpp"

#include "TableLayout.hpp"
#include "TableGame.hpp"

#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void DrawCards(Texture2D atlas, const TableCardList& cards, const Camera3D& camera,
               const CardPresentation& presentation, Vector2 cursor) {
    for (const auto& card : cards) {
        const auto pose = presentation.Pose(card, camera, cursor);
        const bool back = card.id < 0;
        rlSetTexture(back ? rlGetTextureIdDefault() : atlas.id);
        rlBegin(RL_QUADS);
        rlColor4ub(back ? 0 : 255, back ? 0 : 255, back ? 0 : 255, 255);
        const float u0 = back ? 0.0f : static_cast<float>(card.id / 4) / 12.0f;
        const float v0 = back ? 0.0f : static_cast<float>(card.id % 4) / 4.0f;
        const float u1 = back ? 1.0f : u0 + 1.0f / 12.0f;
        const float v1 = back ? 1.0f : v0 + 1.0f / 4.0f;
        const auto vertex = [&](float u, float v, float tu, float tv) {
            const auto p = CardPresentation::PointOnCard(pose,u,v);
            rlTexCoord2f(tu,tv); rlVertex3f(p.x,p.y,p.z);
        };
        vertex(0,0,u0,v0); vertex(0,1,u0,v1);
        vertex(1,1,u1,v1); vertex(1,0,u1,v0);
        rlEnd();
    }
    rlSetTexture(0);
}

const char* TypeName(CardType type) {
    switch (type) {
        case CardType::HIKARI: return "Bright";
        case CardType::TANE: return "Animal";
        case CardType::TANZAKU: return "Ribbon";
        case CardType::KASU: return "Plain";
    }
    return "Unknown";
}

// Candidate labels refer to the actual KK-ADD card identities, not a claim
// that a yaku has been completed. General category yaku are shown as well.
void DrawYakuTags(const Card& card, int x, int y) {
    const auto line = [&](const char* text) { DrawText(text,x,y,14,LIGHTGRAY); y += 20; };
    switch (card.type) {
        case CardType::HIKARI: line("Bright-set candidate"); break;
        case CardType::TANE: line("Tane candidate"); break;
        case CardType::TANZAKU: line("Tanzaku candidate"); break;
        case CardType::KASU: line("Kasu candidate"); break;
    }
    if (card.id == 8 || card.isSakeCup) line("Hanami-zake candidate");
    if (card.id == 28 || card.isSakeCup) line("Tsukimi-zake candidate");
    if (card.id == 20 || card.id == 24 || card.id == 36) line("Ino-Shika-Cho candidate");
    if (card.id == 1 || card.id == 5 || card.id == 9) line("Akatan candidate");
    if (card.id == 21 || card.id == 33 || card.id == 37) line("Aotan candidate");
    if (card.isSakeCup) line("Also counts toward Kasu");
}

void DrawCaptureBrowser(Texture2D atlas, const TableGame& game, int left, int top) {
    const auto& player = game.Rules().Captured(Player::PLAYER);
    const auto& opponent = game.Rules().Captured(Player::OPPONENT);
    if (player.empty() && opponent.empty()) return;
    using namespace CardInspection;
    DrawRectangle(left,top,static_cast<int>(Width),static_cast<int>(Height), Color{15,20,27,220});
    DrawText("CAPTURES  YOU / CPU", left+7,top+5,14,WHITE);
    const int w = atlas.width/12, h = atlas.height/4;
    const std::size_t total = std::min(Capacity,player.size()+opponent.size());
    for (std::size_t i=0; i<total; ++i) {
        const int id = i < player.size() ? player[i] : opponent[i-player.size()];
        const float x = left + (GridX-Left) + static_cast<float>(i%Columns)*CellWidth;
        const float y = top + (GridY-Top) + static_cast<float>(i/Columns)*CellHeight;
        // Ownership marker is outside the source image, never over its border.
        DrawRectangle(static_cast<int>(x), static_cast<int>(y), 2, 37,
                      i < player.size() ? Color{93,192,165,255} : Color{224,160,116,255});
        // A separate outer mat keeps the small thumbnails visibly framed;
        // it does not cover or replace the original card-border pixels.
        DrawRectangle(static_cast<int>(x+2), static_cast<int>(y-1), 27, 39, BLACK);
        DrawTexturePro(atlas, {static_cast<float>((id/4)*w),static_cast<float>((id%4)*h),
                               static_cast<float>(w),static_cast<float>(h)},
                       {x+3,y,25,37}, {0,0}, 0, WHITE);
    }
}

void DrawInspection(Texture2D atlas, int id, int left, int top) {
    if (id < 0 || id >= 48) return; // Never reveal a stock or opponent back.
    const Card& card = DeckRegister::At(id);
    DrawRectangle(left, top, 386, 282, Color{15,20,27,240});
    const int cellWidth = atlas.width / 12, cellHeight = atlas.height / 4;
    DrawTexturePro(atlas, {static_cast<float>((id / 4)*cellWidth), static_cast<float>((id % 4)*cellHeight),
                           static_cast<float>(cellWidth), static_cast<float>(cellHeight)},
                   {static_cast<float>(left+8), static_cast<float>(top+8),
                    static_cast<float>(cellWidth), static_cast<float>(cellHeight)}, {0,0}, 0, WHITE);
    const int x = left+196;
    DrawText("TRAIN TELEMETRY", x, top+10, 15, WHITE);
    // Names are static register strings; wrap on a word boundary, without heap allocations.
    const std::string_view name(card.name);
    const auto split = name.size() > 21 ? name.rfind(' ', 21) : std::string_view::npos;
    char first[32]{}, second[32]{};
    const std::size_t count = split == std::string_view::npos ? name.size() : split;
    std::snprintf(first, sizeof(first), "%.*s", static_cast<int>(count), name.data());
    if (split != std::string_view::npos)
        std::snprintf(second, sizeof(second), "%.*s", static_cast<int>(name.size()-split-1), name.data()+split+1);
    DrawText(first, x, top+39, 15, WHITE);
    if (second[0]) DrawText(second, x, top+58, 15, WHITE);
    char line[64];
    std::snprintf(line, sizeof(line), "Month: %d", card.month);
    DrawText(line, x, top+86, 15, WHITE);
    std::snprintf(line, sizeof(line), "Type: %s", TypeName(card.type));
    DrawText(line, x, top+108, 15, WHITE);
    DrawText("YAKU CANDIDATES", x, top+143, 14, WHITE);
    DrawYakuTags(card, x, top+165);
}

Texture2D LoadRequiredTexture(const char* path) {
    Texture2D texture = LoadTexture(path);
    if (texture.id == 0 || texture.width <= 0 || texture.height <= 0) {
        if (texture.id != 0) UnloadTexture(texture);
        throw std::runtime_error(std::string("Could not load texture: ") + path);
    }
    return texture;
}
} // namespace

TableScene::TableScene(const char* backgroundPath, const char* atlasPath) {
    try {
        background_ = LoadRequiredTexture(backgroundPath);
        atlas_ = LoadRequiredTexture(atlasPath);
        if (atlas_.width % 12 != 0 || atlas_.height % 4 != 0)
            throw std::runtime_error("Card atlas must be a tightly packed 12x4 grid");
        SetTextureFilter(atlas_, TEXTURE_FILTER_POINT);
        canvas_ = LoadRenderTexture(TableLayout::ImageWidth, TableLayout::ImageHeight);
        if (canvas_.id == 0 || canvas_.texture.id == 0)
            throw std::runtime_error("Could not create table render texture");
        SetTextureFilter(canvas_.texture, TEXTURE_FILTER_BILINEAR);
    } catch (...) {
        if (canvas_.id != 0) UnloadRenderTexture(canvas_);
        if (atlas_.id != 0) UnloadTexture(atlas_);
        if (background_.id != 0) UnloadTexture(background_);
        throw;
    }
}

TableScene::~TableScene() {
    UnloadRenderTexture(canvas_);
    UnloadTexture(atlas_);
    UnloadTexture(background_);
}

void TableScene::Draw(const TableGame& game, const Camera3D& camera,
                      const CardPresentation& presentation, Vector2 cursor) const {
    BeginTextureMode(canvas_);
    ClearBackground(BLACK);
    // The original image is drawn unchanged. No camera-dependent backdrop warp.
    DrawTexturePro(background_, {0,0,static_cast<float>(background_.width),static_cast<float>(background_.height)},
                   {0,0,static_cast<float>(TableLayout::ImageWidth),static_cast<float>(TableLayout::ImageHeight)},
                   {0,0}, 0, WHITE);
    BeginMode3D(camera);
    DrawCards(atlas_, game.CardVisuals(), camera, presentation, cursor);
    EndMode3D();
    DrawRectangle(8, 8, 540, 126, Color{15, 20, 27, 220});
    char label[128], status[128];
    std::snprintf(label, sizeof(label), "ROUND %d / %d    YOU %d  -  CPU %d", game.Round(), TableLayout::MatchRounds,
                  game.Scores()[PlayerIndex(Player::PLAYER)], game.Scores()[PlayerIndex(Player::OPPONENT)]);
    game.FormatStatus(status, sizeof(status));
    DrawText(label, 20, 18, 22, WHITE);
    DrawText(status, 20, 51, 19, WHITE);
    DrawText("Drag a card to a match; right-click to cancel", 20, 80, 15, LIGHTGRAY);
    DrawText("Hover a card to inspect", 20, 108, 15, LIGHTGRAY);
    if (game.Rules().GetPhase() == Phase::AWAIT_DECISION && game.Rules().CurrentPlayer() == Player::PLAYER && !game.Busy()) {
        DrawRectangle(1010, 20, 145, 40, Color{31, 75, 58, 245});
        DrawRectangle(1165, 20, 145, 40, Color{100, 50, 38, 245});
        DrawText("KOI-KOI", 1030, 30, 20, WHITE);
        DrawText("SHOBU", 1190, 30, 20, WHITE);
    }
    if (game.Rules().GetPhase() == Phase::ROUND_END) {
        DrawRectangle(1000, 20, 310, 66, Color{15, 20, 27, 235});
        DrawText(game.MatchFinished() ? "MATCH OVER" : "NEXT ROUND", 1020, 27, 20, WHITE);
        DrawText(game.MatchFinished() ? "Click to restart" : "Click to continue", 1020, 55, 16, LIGHTGRAY);
    }
    EndTextureMode();

    const Rectangle destination = TableView::CanvasViewport(static_cast<float>(GetScreenWidth()),
                                                                          static_cast<float>(GetScreenHeight()));
    BeginDrawing();
    ClearBackground(BLACK);
    // A render texture is vertically inverted in Raylib.
    DrawTexturePro(canvas_.texture, {0, 0, static_cast<float>(canvas_.texture.width),
                                    -static_cast<float>(canvas_.texture.height)}, destination, {0, 0}, 0, WHITE);
    // Draw the source cell at native logical screen pixels even when the
    // canvas is resized; do not downsample the inspection image with it.
    const int left = CardInspection::PreviewLeft(destination.x, destination.width);
    const int top = CardInspection::PanelTop(destination.y, destination.width);
    DrawCaptureBrowser(atlas_, game, CardInspection::BrowserLeft(destination.x,destination.width), top);
    DrawInspection(atlas_, presentation.InspectedCard(), left, top);
    EndDrawing();
}
