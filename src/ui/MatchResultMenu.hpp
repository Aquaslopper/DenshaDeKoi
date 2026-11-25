#pragma once

#include "AtlasLayout.hpp"
#ifdef BLUE
#pragma push_macro("BLUE")
#undef BLUE
#include "DeckRegister.hpp"
#pragma pop_macro("BLUE")
#else
#include "DeckRegister.hpp" // Parse card enums before Raylib defines its BLUE color macro.
#endif
#include "KoiKoiProfile.hpp"
#include "KoiKoiRules.hpp"
#include "KoiKoiStateMachine.hpp"
#include "TableGame.hpp"
#include "ui/UiViewport.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Ui {

// Match result panel artwork is 1164x785; keep the PNG untouched and scale uniformly
// to match the Pause/Options menu footprint (~81.5%, ~949x640) centered in 1360x768.
inline constexpr float MatchResultArtWidth = 1164.0f;
inline constexpr float MatchResultArtHeight = 785.0f;
inline constexpr float MatchResultArtScale = 0.815f;
inline constexpr TableLayout::Rect MatchResultPanel{
    (TableLayout::ImageWidth - MatchResultArtWidth * MatchResultArtScale) * 0.5f,
    (TableLayout::ImageHeight - MatchResultArtHeight * MatchResultArtScale) * 0.5f,
    MatchResultArtWidth * MatchResultArtScale,
    MatchResultArtHeight * MatchResultArtScale
};

// Native 1164x785 artwork regions measured from assets/options/match-result/matchresult.png.
namespace MatchResultLayout {
using TableLayout::Rect;

inline constexpr Rect YouBody{40.0f, 176.0f, 516.0f, 308.0f};
inline constexpr Rect CpuBody{606.0f, 176.0f, 516.0f, 308.0f};
inline constexpr Rect YouTotalValue{188.0f, 492.0f, 358.0f, 58.0f};
inline constexpr Rect CpuTotalValue{754.0f, 492.0f, 358.0f, 58.0f};
inline constexpr Rect Banner{42.0f, 578.0f, 1080.0f, 126.0f};
inline constexpr Rect BackButton{440.0f, 723.0f, 154.0f, 51.0f};
inline constexpr Rect NextButton{594.0f, 723.0f, 155.0f, 51.0f};
} // namespace MatchResultLayout

enum class MatchResultAction { NONE, BACK, NEXT };
enum class MatchResultMode { ROUND, MATCH };

struct ComboDisplay {
    std::string name;
    int points = 0;
    std::vector<CardId> cards;
};

struct RoundLedgerEntry {
    int roundNumber = 1;
    EndReason reason = EndReason::EXHAUSTIVE_DRAW;
    std::optional<Player> winner;
    std::array<int, 2> points{};
};

class MatchResultMenu {
public:
    MatchResultMenu() = default;

    explicit MatchResultMenu(const std::filesystem::path& appDirectory) {
        panel_ = LoadTexture((appDirectory / "options" / "match-result" / "matchresult.png").string().c_str());
        if (panel_.id != 0) SetTextureFilter(panel_, TEXTURE_FILTER_BILINEAR);
        atlas_ = LoadTexture((appDirectory / "cards_atlas.png").string().c_str());
        if (atlas_.id != 0) SetTextureFilter(atlas_, TEXTURE_FILTER_BILINEAR);
    }

    ~MatchResultMenu() {
        if (panel_.id != 0) UnloadTexture(panel_);
        if (atlas_.id != 0) UnloadTexture(atlas_);
    }

    MatchResultMenu(const MatchResultMenu&) = delete;
    MatchResultMenu& operator=(const MatchResultMenu&) = delete;

    [[nodiscard]] bool IsOpen() const noexcept { return open_; }
    [[nodiscard]] MatchResultMode CurrentMode() const noexcept { return mode_; }
    [[nodiscard]] int FocusedButton() const noexcept { return focusedButton_; }
    [[nodiscard]] const std::vector<RoundLedgerEntry>& Ledger() const noexcept { return ledger_; }
    [[nodiscard]] const std::vector<ComboDisplay>& Combos(Player player) const {
        return combos_[PlayerIndex(player)];
    }

    void ResetMatch() {
        open_ = false;
        mode_ = MatchResultMode::ROUND;
        recordedRound_ = 0;
        round_ = 1;
        totalRounds_ = TableLayout::MatchRounds;
        matchFinished_ = false;
        focusedButton_ = 1;
        roundResult_.reset();
        matchScores_ = {};
        capturedCounts_ = {};
        koiCalls_ = {};
        combos_ = {};
        ledger_.clear();
    }

    void CloseRoundPopup() {
        open_ = false;
        mode_ = MatchResultMode::ROUND;
        focusedButton_ = 1;
    }

    void ShowMatchSummary() {
        if (!open_) open_ = true;
        mode_ = MatchResultMode::MATCH;
        focusedButton_ = 1;
    }

    // Snapshot a finished round once and open the popup in ROUND mode.
    void SyncRoundEnd(const TableGame& game) {
        const auto& rules = game.Rules();
        if (rules.GetPhase() != Phase::ROUND_END || !rules.Result().has_value()) return;
        if (recordedRound_ == game.Round()) return;

        recordedRound_ = game.Round();
        round_ = game.Round();
        totalRounds_ = game.TotalRounds();
        matchFinished_ = game.MatchFinished();
        roundResult_ = *rules.Result();
        matchScores_ = game.Scores();

        for (Player player : {Player::OPPONENT, Player::PLAYER}) {
            const std::size_t idx = PlayerIndex(player);
            capturedCounts_[idx] = rules.Captured(player).size();
            koiCalls_[idx] = rules.KoiCalls(player);
            combos_[idx] = BuildPlayerCombos(rules, player);
        }

        ledger_.push_back({round_, roundResult_->reason, roundResult_->winner, roundResult_->points});
        mode_ = MatchResultMode::ROUND;
        focusedButton_ = 1;
        open_ = true;
    }

    [[nodiscard]] MatchResultAction Update(TableLayout::Point2 canvasPointer, bool moved, bool pressed,
                                           int horizontal, bool activate) {
        if (!open_) return MatchResultAction::NONE;

        const TableLayout::Point2 art = ToArtPoint(canvasPointer);
        if (moved) {
            if (Contains(MatchResultLayout::BackButton, art)) focusedButton_ = 0;
            else if (Contains(MatchResultLayout::NextButton, art)) focusedButton_ = 1;
        }
        if (horizontal != 0) {
            focusedButton_ = (focusedButton_ + 2 + (horizontal > 0 ? 1 : -1)) % 2;
        }
        if (pressed) {
            if (Contains(MatchResultLayout::BackButton, art)) {
                focusedButton_ = 0;
                return MatchResultAction::BACK;
            }
            if (Contains(MatchResultLayout::NextButton, art)) {
                focusedButton_ = 1;
                return MatchResultAction::NEXT;
            }
        }
        if (activate) {
            return focusedButton_ == 0 ? MatchResultAction::BACK : MatchResultAction::NEXT;
        }
        return MatchResultAction::NONE;
    }

    void Draw(const Viewport& outer, TableLayout::Point2 canvasPointer,
              Font headingFont, Font bodyFont) const {
        if (!open_) return;

        // Subtle dark backdrop matching PauseMenu so the table remains softly visible around it.
        DrawRectangle(outer.X(0), outer.Y(0),
                      outer.W(TableLayout::ImageWidth), outer.H(TableLayout::ImageHeight),
                      Color{5, 8, 17, 142});

        const Viewport map{outer.Rec(MatchResultPanel), outer.scale * MatchResultArtScale};

        if (panel_.id != 0) {
            DrawTexturePro(panel_,
                           {0.0f, 0.0f, static_cast<float>(panel_.width), static_cast<float>(panel_.height)},
                           map.Rec({0.0f, 0.0f, MatchResultArtWidth, MatchResultArtHeight}),
                           {0.0f, 0.0f}, 0.0f, WHITE);
        } else {
            DrawRectangle(map.X(0), map.Y(0), map.W(MatchResultArtWidth), map.H(MatchResultArtHeight),
                          Color{232, 236, 240, 245});
        }

        if (mode_ == MatchResultMode::ROUND) {
            DrawRoundPanel(map, Player::PLAYER, MatchResultLayout::YouBody, headingFont, bodyFont);
            DrawRoundPanel(map, Player::OPPONENT, MatchResultLayout::CpuBody, headingFont, bodyFont);
            DrawRoundTotalBar(map, Player::PLAYER, MatchResultLayout::YouTotalValue, headingFont);
            DrawRoundTotalBar(map, Player::OPPONENT, MatchResultLayout::CpuTotalValue, headingFont);
            DrawRoundBanner(map, headingFont, bodyFont);
        } else {
            DrawMatchLedgerPanel(map, Player::PLAYER, MatchResultLayout::YouBody, headingFont, bodyFont);
            DrawMatchLedgerPanel(map, Player::OPPONENT, MatchResultLayout::CpuBody, headingFont, bodyFont);
            DrawMatchTotalBar(map, Player::PLAYER, MatchResultLayout::YouTotalValue, headingFont);
            DrawMatchTotalBar(map, Player::OPPONENT, MatchResultLayout::CpuTotalValue, headingFont);
            DrawMatchBanner(map, headingFont, bodyFont);
        }

        DrawPillHighlight(map, ToArtPoint(canvasPointer));
    }

    // Pure helpers exposed for tests: build KK-ADD-10 combo lists with associated cards.
    [[nodiscard]] static std::vector<ComboDisplay> BuildCombosFromCaptures(
        std::span<const CardId> captured) {
        std::vector<ComboDisplay> result;
        if (captured.empty()) return result;

        const YakuScore score = ScoreYaku(captured);
        const auto has = [&captured](CardId id) {
            return std::find(captured.begin(), captured.end(), id) != captured.end();
        };

        // 1. Brights
        constexpr std::array<CardId, 5> BrightIds{0, 8, 28, 40, 44};
        if (score.bright > 0) {
            std::string name = "Sanko";
            if (score.brightYaku == BrightYaku::GOKO) name = "Goko";
            else if (score.brightYaku == BrightYaku::SHIKO) name = "Shiko";
            else if (score.brightYaku == BrightYaku::AME_SHIKO) name = "Ame-shiko";
            std::vector<CardId> cards;
            for (CardId id : BrightIds) {
                if (has(id)) cards.push_back(id);
            }
            result.push_back({std::move(name), score.bright, std::move(cards)});
        }

        // 2. Viewings
        if (score.hanami > 0) {
            result.push_back({"Hanami-zake", score.hanami, {8, 32}});
        }
        if (score.tsukimi > 0) {
            result.push_back({"Tsukimi-zake", score.tsukimi, {28, 32}});
        }

        // 3. Ino-shika-cho
        if (score.inoShikaCho > 0) {
            result.push_back({"Ino-shika-cho", score.inoShikaCho, {20, 24, 36}});
        }

        // 4. Fixed ribbon sets (mutually exclusive in KK-ADD-10)
        if (score.akatanAotan > 0) {
            result.push_back({"Akatan + Aotan", score.akatanAotan, {1, 5, 9, 21, 33, 37}});
        } else if (score.akatan > 0) {
            result.push_back({"Akatan", score.akatan, {1, 5, 9}});
        } else if (score.aotan > 0) {
            result.push_back({"Aotan", score.aotan, {21, 33, 37}});
        }

        // 5. Counted sets (Tane, Tanzaku, Kasu)
        if (score.tane > 0) {
            std::vector<CardId> cards;
            for (CardId id : captured) {
                if (DeckRegister::At(id).type == CardType::TANE) cards.push_back(id);
            }
            std::sort(cards.begin(), cards.end());
            result.push_back({"Tane (" + std::to_string(score.animals) + ")", score.tane, std::move(cards)});
        }
        if (score.tanzaku > 0) {
            std::vector<CardId> cards;
            for (CardId id : captured) {
                if (DeckRegister::At(id).type == CardType::TANZAKU) cards.push_back(id);
            }
            std::sort(cards.begin(), cards.end());
            result.push_back({"Tanzaku (" + std::to_string(score.ribbons) + ")", score.tanzaku, std::move(cards)});
        }
        if (score.kasu > 0) {
            std::vector<CardId> cards;
            for (CardId id : captured) {
                const Card& c = DeckRegister::At(id);
                if (c.type == CardType::KASU || c.isSakeCup) cards.push_back(id);
            }
            std::sort(cards.begin(), cards.end());
            result.push_back({"Kasu (" + std::to_string(score.kasuUnits) + ")", score.kasu, std::move(cards)});
        }

        return result;
    }

    [[nodiscard]] static std::vector<ComboDisplay> BuildPlayerCombos(
        const KoiKoiStateMachine& rules, Player player) {
        const auto& roundRes = rules.Result();
        if (roundRes.has_value() &&
            (roundRes->reason == EndReason::OPENING_WIN || roundRes->reason == EndReason::BOTH_OPENING_DRAW)) {
            std::vector<ComboDisplay> result;
            const bool hasLucky = (roundRes->reason == EndReason::BOTH_OPENING_DRAW) ||
                                  (roundRes->winner.has_value() && *roundRes->winner == player);
            if (!hasLucky) return result;
            const auto& hand = rules.Hand(player);
            const OpeningHand opening = hand.size() == 8 ? CheckOpeningHand(hand) : OpeningHand::NONE;
            std::string name = "Lucky Hand";
            if (opening == OpeningHand::TESHI) name = "Teshi (Lucky)";
            else if (opening == OpeningHand::KUTTSUKI) name = "Kuttsuki (Lucky)";
            std::vector<CardId> cards(hand.begin(), hand.end());
            std::sort(cards.begin(), cards.end());
            const int pts = roundRes->reason == EndReason::OPENING_WIN ? KoiKoiProfile::OpeningPoints : 0;
            result.push_back({std::move(name), pts, std::move(cards)});
            return result;
        }
        return BuildCombosFromCaptures(rules.Captured(player));
    }

private:
    struct PanelPalette {
        Color ink;
        Color accent;
        Color muted;
        Color rowFill;
        Color rowWinFill;
        Color badgeFill;
        Color badgeBorder;
        Color divider;
    };

    static PanelPalette PaletteFor(Player player) {
        if (player == Player::PLAYER) {
            return {
                Color{10, 46, 26, 255},
                Color{0, 102, 46, 255},
                Color{68, 114, 88, 255},
                Color{0, 62, 30, 18},
                Color{0, 120, 56, 36},
                Color{6, 86, 44, 34},
                Color{6, 98, 50, 115},
                Color{16, 110, 62, 90}
            };
        }
        return {
            Color{10, 34, 72, 255},
            Color{12, 74, 152, 255},
            Color{72, 102, 138, 255},
            Color{0, 42, 96, 18},
            Color{12, 88, 186, 36},
            Color{10, 58, 128, 34},
            Color{14, 76, 160, 115},
            Color{20, 82, 158, 90}
        };
    }

    static TableLayout::Point2 ToArtPoint(TableLayout::Point2 canvas) {
        return {
            (canvas.x - MatchResultPanel.x) / MatchResultArtScale,
            (canvas.y - MatchResultPanel.y) / MatchResultArtScale
        };
    }

    static bool Contains(TableLayout::Rect rect, TableLayout::Point2 p) {
        return p.x >= rect.x && p.x <= rect.x + rect.width &&
               p.y >= rect.y && p.y <= rect.y + rect.height;
    }

    static std::string PointsText(int pts) {
        return std::to_string(pts) + (pts == 1 ? " pt" : " pts");
    }

    enum class Align { LEFT, CENTER, RIGHT };

    static void DrawFittedText(const Viewport& map, Font font, const char* text,
                               TableLayout::Rect box, float baseSize, Color color,
                               Align align = Align::LEFT, bool shadow = false) {
        const Rectangle r = map.Rec(box);
        float size = baseSize * map.scale;
        const float spacing = 0.6f * map.scale;
        Vector2 measured = MeasureTextEx(font, text, size, spacing);
        if (measured.x > r.width && measured.x > 0.0f) {
            size *= r.width / measured.x;
            measured = MeasureTextEx(font, text, size, spacing);
        }
        float x = r.x;
        if (align == Align::CENTER) x = r.x + (r.width - measured.x) * 0.5f;
        else if (align == Align::RIGHT) x = r.x + (r.width - measured.x);
        const float y = r.y + (r.height - measured.y) * 0.5f;
        if (shadow) {
            const float off = std::max(1.0f, 2.0f * map.scale);
            DrawTextEx(font, text, {x + off, y + off}, size, spacing, Color{0, 0, 0, 155});
        }
        DrawTextEx(font, text, {x, y}, size, spacing, color);
    }

    void DrawMiniCards(const Viewport& map, const std::vector<CardId>& cards,
                       float x, float y, float maxWidth, float cardHeight, bool dimmed) const {
        if (cards.empty()) return;
        constexpr float CardAspect = 179.0f / 264.0f;
        const float cardW = cardHeight * CardAspect;
        const std::size_t count = cards.size();
        const float step = count > 1
            ? std::min(cardW + 4.0f, std::max(4.0f, (maxWidth - cardW) / static_cast<float>(count - 1)))
            : 0.0f;
        const Color tint = dimmed ? Color{255, 255, 255, 190} : WHITE;
        const Color shadowColor = dimmed ? Color{0, 0, 0, 45} : Color{0, 0, 0, 82};
        const Color borderColor = dimmed ? Color{25, 32, 40, 165} : Color{18, 22, 28, 235};

        for (std::size_t i = 0; i < count; ++i) {
            const TableLayout::Rect dst{x + static_cast<float>(i) * step, y, cardW, cardHeight};
            DrawRectangle(map.X(dst.x + 1.5f), map.Y(dst.y + 1.5f), map.W(dst.width), map.H(dst.height), shadowColor);
            if (atlas_.id != 0 && atlas_.width > 0 && atlas_.height > 0) {
                const Card& card = DeckRegister::At(cards[i]);
                const auto src = AtlasLayout::GetCardSourceRect(
                    static_cast<float>(atlas_.width), static_cast<float>(atlas_.height),
                    card.month, card.cardIndex);
                DrawTexturePro(atlas_, {src.x, src.y, src.width, src.height},
                               map.Rec(dst), {0.0f, 0.0f}, 0.0f, tint);
            } else {
                DrawRectangle(map.X(dst.x), map.Y(dst.y), map.W(dst.width), map.H(dst.height),
                              Color{240, 234, 218, tint.a});
            }
            DrawRectangleLinesEx(map.Rec(dst), std::max(1.0f, 1.0f * map.scale), borderColor);
        }
    }

    void DrawRoundPanel(const Viewport& map, Player player, TableLayout::Rect body,
                        Font headingFont, Font bodyFont) const {
        const PanelPalette pal = PaletteFor(player);
        const std::size_t idx = PlayerIndex(player);
        const auto& combos = combos_[idx];
        const bool isWinner = roundResult_.has_value() && roundResult_->winner.has_value() &&
                              *roundResult_->winner == player;
        const bool isShoubuWin = isWinner && roundResult_->reason == EndReason::SHOUBU;
        const bool hasMultipliers = isShoubuWin && roundResult_->settlement.award > roundResult_->settlement.base;
        const bool isUnbanked = !combos.empty() && !isWinner &&
                                roundResult_.has_value() && roundResult_->winner.has_value();

        if (combos.empty()) {
            const float centerY = body.y + body.height * 0.5f;
            DrawFittedText(map, headingFont, "No combos",
                           {body.x + 16.0f, centerY - 34.0f, body.width - 32.0f, 34.0f},
                           30.0f, pal.muted, Align::CENTER);
            const std::size_t cap = capturedCounts_[idx];
            const std::string sub = "Captured: " + std::to_string(cap) + (cap == 1 ? " card" : " cards");
            DrawFittedText(map, bodyFont, sub.c_str(),
                           {body.x + 16.0f, centerY + 6.0f, body.width - 32.0f, 28.0f},
                           24.0f, pal.muted, Align::CENTER);
            return;
        }

        const float footerReserve = hasMultipliers ? 50.0f : (isUnbanked ? 34.0f : 4.0f);
        const float listTop = body.y + 4.0f;
        const float availHeight = std::max(60.0f, body.height - 8.0f - footerReserve);
        const std::size_t n = combos.size();
        const float rowStep = std::min(56.0f, availHeight / static_cast<float>(n));
        const float rowHeight = std::max(28.0f, rowStep - 4.0f);
        const float cardH = std::clamp(rowHeight - 6.0f, 24.0f, 44.0f);
        const float nameFontSize = std::clamp(rowHeight * 0.50f, 18.0f, 25.0f);
        const float ptsFontSize = std::clamp(rowHeight * 0.46f, 17.0f, 23.0f);

        for (std::size_t i = 0; i < n; ++i) {
            const auto& combo = combos[i];
            const float ry = listTop + static_cast<float>(i) * rowStep;
            const TableLayout::Rect rowRect{body.x, ry, body.width, rowHeight};

            const Color fill = isUnbanked
                ? Color{pal.rowFill.r, pal.rowFill.g, pal.rowFill.b, 10}
                : pal.rowFill;
            DrawRectangleRounded(map.Rec(rowRect), 0.16f, 4, fill);

            const TableLayout::Rect nameRect{rowRect.x + 10.0f, rowRect.y, 174.0f, rowRect.height};
            DrawFittedText(map, bodyFont, combo.name.c_str(), nameRect, nameFontSize,
                           isUnbanked ? pal.muted : pal.ink, Align::LEFT);

            const float cardY = rowRect.y + (rowRect.height - cardH) * 0.5f;
            DrawMiniCards(map, combo.cards, rowRect.x + 190.0f, cardY, 238.0f, cardH, isUnbanked);

            const TableLayout::Rect ptsRect{rowRect.x + 432.0f, rowRect.y, 74.0f, rowRect.height};
            const std::string ptsStr = PointsText(combo.points);
            DrawFittedText(map, headingFont, ptsStr.c_str(), ptsRect, ptsFontSize,
                           isUnbanked ? pal.muted : pal.accent, Align::RIGHT);
        }

        if (hasMultipliers) {
            const Settlement& s = roundResult_->settlement;
            std::string multText = "Base " + std::to_string(s.base);
            if (s.sevenFactor > 1) multText += "  x" + std::to_string(s.sevenFactor) + " (7+)";
            if (s.counterFactor > 1) multText += "  x" + std::to_string(s.counterFactor) + " (Opp Koi)";
            if (s.callerFactor > 1) {
                multText += "  x" + std::to_string(s.callerFactor) + " (Koi";
                if (koiCalls_[idx] > 0) multText += " x" + std::to_string(koiCalls_[idx]);
                multText += ")";
            }
            multText += "  =  " + std::to_string(s.award) + " pts";

            const TableLayout::Rect badge{body.x, body.y + body.height - 44.0f, body.width, 40.0f};
            DrawRectangleRounded(map.Rec(badge), 0.22f, 6, pal.badgeFill);
            DrawRectangleLinesEx(map.Rec(badge), std::max(1.0f, 1.5f * map.scale), pal.badgeBorder);
            DrawFittedText(map, bodyFont, multText.c_str(),
                           {badge.x + 10.0f, badge.y, badge.width - 20.0f, badge.height},
                           23.0f, pal.ink, Align::CENTER);
        } else if (isUnbanked) {
            const TableLayout::Rect note{body.x + 8.0f, body.y + body.height - 30.0f, body.width - 16.0f, 26.0f};
            DrawFittedText(map, bodyFont, "Unbanked - 0 pts awarded", note, 21.0f, pal.muted, Align::CENTER);
        }
    }

    void DrawMatchLedgerPanel(const Viewport& map, Player player, TableLayout::Rect body,
                              Font headingFont, Font bodyFont) const {
        const PanelPalette pal = PaletteFor(player);
        const std::size_t idx = PlayerIndex(player);

        int wins = 0;
        for (const auto& entry : ledger_) {
            if (entry.winner.has_value() && *entry.winner == player) ++wins;
        }

        const TableLayout::Rect headerLeft{body.x + 8.0f, body.y + 4.0f, 260.0f, 28.0f};
        const TableLayout::Rect headerRight{body.x + body.width - 210.0f, body.y + 4.0f, 202.0f, 28.0f};
        DrawFittedText(map, headingFont, "ROUND LEDGER", headerLeft, 21.0f, pal.ink, Align::LEFT);
        const std::string winsLabel = "WINS: " + std::to_string(wins) + " / " + std::to_string(ledger_.size());
        DrawFittedText(map, headingFont, winsLabel.c_str(), headerRight, 21.0f, pal.accent, Align::RIGHT);

        const float divY = body.y + 36.0f;
        DrawLineEx({map.box.x + body.x * map.scale, map.box.y + divY * map.scale},
                   {map.box.x + (body.x + body.width) * map.scale, map.box.y + divY * map.scale},
                   std::max(1.0f, 1.5f * map.scale), pal.divider);

        if (ledger_.empty()) return;

        const float gridTop = divY + 8.0f;
        const float gridHeight = body.y + body.height - gridTop - 4.0f;
        const std::size_t count = ledger_.size();
        const bool twoCol = count > 6;
        const std::size_t rowsPerCol = twoCol ? (count + 1) / 2 : count;
        const float colGap = twoCol ? 12.0f : 0.0f;
        const float colWidth = twoCol ? (body.width - colGap) * 0.5f : body.width;
        const float rowStep = std::min(43.0f, gridHeight / static_cast<float>(std::max<std::size_t>(1, rowsPerCol)));
        const float rowHeight = std::max(26.0f, rowStep - 4.0f);

        for (std::size_t i = 0; i < count; ++i) {
            const auto& entry = ledger_[i];
            const std::size_t col = twoCol ? (i / rowsPerCol) : 0;
            const std::size_t row = twoCol ? (i % rowsPerCol) : i;
            const float rx = body.x + static_cast<float>(col) * (colWidth + colGap);
            const float ry = gridTop + static_cast<float>(row) * rowStep;
            const TableLayout::Rect cell{rx, ry, colWidth, rowHeight};

            const bool won = entry.winner.has_value() && *entry.winner == player;
            const bool draw = !entry.winner.has_value();
            DrawRectangleRounded(map.Rec(cell), 0.16f, 4, won ? pal.rowWinFill : pal.rowFill);

            const int pts = entry.points[idx];
            const std::string roundLbl = twoCol
                ? ("R" + std::to_string(entry.roundNumber))
                : ("Round " + std::to_string(entry.roundNumber));
            const char* outcome = won ? "WIN" : (draw ? "DRAW" : "-");
            const std::string ptsLbl = pts > 0
                ? ("+" + std::to_string(pts) + (twoCol ? "" : " pts"))
                : (twoCol ? "0" : "0 pts");

            const float fontSize = twoCol ? 21.0f : 24.0f;
            DrawFittedText(map, bodyFont, roundLbl.c_str(),
                           {cell.x + 10.0f, cell.y, twoCol ? 64.0f : 140.0f, cell.height},
                           fontSize, pal.ink, Align::LEFT);
            DrawFittedText(map, bodyFont, outcome,
                           {cell.x + (twoCol ? 78.0f : 180.0f), cell.y, twoCol ? 78.0f : 130.0f, cell.height},
                           twoCol ? 19.0f : 22.0f, won ? pal.accent : pal.muted, Align::CENTER);
            DrawFittedText(map, headingFont, ptsLbl.c_str(),
                           {cell.x + cell.width - (twoCol ? 88.0f : 150.0f), cell.y,
                            twoCol ? 78.0f : 140.0f, cell.height},
                           twoCol ? 20.0f : 24.0f, won ? pal.accent : pal.muted, Align::RIGHT);
        }
    }

    void DrawRoundTotalBar(const Viewport& map, Player player, TableLayout::Rect barRect,
                           Font headingFont) const {
        const std::size_t idx = PlayerIndex(player);
        const int roundPts = roundResult_.has_value() ? roundResult_->points[idx] : 0;
        const int matchPts = matchScores_[idx];

        const std::string roundPart =
            (roundPts > 0 ? ("+" + std::to_string(roundPts)) : std::string("0")) + " pts";
        const std::string matchPart = "  |  Match: " + std::to_string(matchPts);
        const std::string combined = roundPart + matchPart;

        const Rectangle r = map.Rec(barRect);
        float size = 30.0f * map.scale;
        const float spacing = 0.6f * map.scale;
        Vector2 totalMeasure = MeasureTextEx(headingFont, combined.c_str(), size, spacing);
        if (totalMeasure.x > r.width && totalMeasure.x > 0.0f) {
            size *= r.width / totalMeasure.x;
            totalMeasure = MeasureTextEx(headingFont, combined.c_str(), size, spacing);
        }
        const Vector2 roundMeasure = MeasureTextEx(headingFont, roundPart.c_str(), size, spacing);
        const float x = r.x + (r.width - totalMeasure.x);
        const float y = r.y + (r.height - totalMeasure.y) * 0.5f;

        const Color roundColor = roundPts > 0 ? Color{255, 224, 96, 255} : Color{186, 206, 204, 255};
        DrawTextEx(headingFont, roundPart.c_str(), {x, y}, size, spacing, roundColor);
        DrawTextEx(headingFont, matchPart.c_str(), {x + roundMeasure.x + spacing, y}, size, spacing, WHITE);
    }

    void DrawMatchTotalBar(const Viewport& map, Player player, TableLayout::Rect barRect,
                           Font headingFont) const {
        const int youPts = matchScores_[PlayerIndex(Player::PLAYER)];
        const int cpuPts = matchScores_[PlayerIndex(Player::OPPONENT)];
        const int ownPts = matchScores_[PlayerIndex(player)];
        const bool isMatchWinner = (player == Player::PLAYER && youPts > cpuPts) ||
                                   (player == Player::OPPONENT && cpuPts > youPts);
        const Color color = isMatchWinner ? Color{255, 224, 96, 255} : WHITE;
        const std::string text = PointsText(ownPts);
        DrawFittedText(map, headingFont, text.c_str(), barRect, 36.0f, color, Align::RIGHT);
    }

    void DrawRoundBanner(const Viewport& map, Font headingFont, Font bodyFont) const {
        const TableLayout::Rect b = MatchResultLayout::Banner;
        std::string headline = "ROUND " + std::to_string(round_) + " DRAW";
        Color headColor{226, 236, 248, 255};

        if (roundResult_.has_value() && roundResult_->winner.has_value()) {
            if (*roundResult_->winner == Player::PLAYER) {
                headline = "YOU WIN ROUND " + std::to_string(round_) + "!";
                headColor = Color{110, 255, 175, 255};
            } else {
                headline = "CPU WINS ROUND " + std::to_string(round_) + "!";
                headColor = Color{115, 210, 255, 255};
            }
        }

        std::string reasonText;
        if (roundResult_.has_value()) {
            const int winPts = roundResult_->winner.has_value()
                ? roundResult_->points[PlayerIndex(*roundResult_->winner)]
                : 0;
            switch (roundResult_->reason) {
                case EndReason::SHOUBU:
                    reasonText = "SHOBU +" + std::to_string(winPts) + " PTS";
                    break;
                case EndReason::OPENING_WIN:
                    reasonText = "LUCKY HAND +" + std::to_string(winPts) + " PTS";
                    break;
                case EndReason::EXHAUSTIVE_DRAW:
                    reasonText = "EXHAUSTION - NO SCORE";
                    break;
                case EndReason::BOTH_OPENING_DRAW:
                    reasonText = "BOTH PLAYERS LUCKY HAND";
                    break;
            }
        }

        const int youPts = matchScores_[PlayerIndex(Player::PLAYER)];
        const int cpuPts = matchScores_[PlayerIndex(Player::OPPONENT)];
        std::string subtitle = reasonText;
        if (matchFinished_) {
            subtitle += "  |  FINAL ROUND (" + std::to_string(round_) + " / " +
                        std::to_string(totalRounds_) + ")  |  NEXT: MATCH RESULT";
        } else {
            subtitle += "  |  ROUND " + std::to_string(round_) + " / " + std::to_string(totalRounds_) +
                        "  |  MATCH: YOU " + std::to_string(youPts) + " - " + std::to_string(cpuPts) + " CPU";
        }

        DrawFittedText(map, headingFont, headline.c_str(),
                       {b.x, b.y + 8.0f, b.width, 62.0f}, 48.0f, headColor, Align::CENTER, true);
        DrawFittedText(map, bodyFont, subtitle.c_str(),
                       {b.x, b.y + 76.0f, b.width, 40.0f}, 27.0f, Color{216, 230, 246, 255}, Align::CENTER);
    }

    void DrawMatchBanner(const Viewport& map, Font headingFont, Font bodyFont) const {
        const TableLayout::Rect b = MatchResultLayout::Banner;
        const int youPts = matchScores_[PlayerIndex(Player::PLAYER)];
        const int cpuPts = matchScores_[PlayerIndex(Player::OPPONENT)];

        std::string headline = "MATCH DRAW!";
        Color headColor{226, 236, 248, 255};
        if (youPts > cpuPts) {
            headline = "YOU WIN THE MATCH!";
            headColor = Color{255, 220, 76, 255};
        } else if (cpuPts > youPts) {
            headline = "CPU WINS THE MATCH!";
            headColor = Color{115, 210, 255, 255};
        }

        const std::string subtitle = "FINAL SCORE:  YOU " + std::to_string(youPts) + " - " +
            std::to_string(cpuPts) + " CPU   |   " + std::to_string(totalRounds_) +
            (totalRounds_ == 1 ? " ROUND" : " ROUNDS") + "   |   NEXT: NEW MATCH";

        DrawFittedText(map, headingFont, headline.c_str(),
                       {b.x, b.y + 6.0f, b.width, 66.0f}, 52.0f, headColor, Align::CENTER, true);
        DrawFittedText(map, bodyFont, subtitle.c_str(),
                       {b.x, b.y + 78.0f, b.width, 40.0f}, 27.0f, Color{216, 230, 246, 255}, Align::CENTER);
    }

    void DrawPillHighlight(const Viewport& map, TableLayout::Point2 artPointer) const {
        const int hovered = Contains(MatchResultLayout::BackButton, artPointer) ? 0
                          : (Contains(MatchResultLayout::NextButton, artPointer) ? 1 : -1);
        const int active = hovered >= 0 ? hovered : focusedButton_;

        if (active == 0) {
            DrawRectangleRounded(map.Rec({445.0f, 728.0f, 145.0f, 41.0f}), 0.75f, 8, Color{25, 110, 220, 38});
            DrawRectangle(map.X(477.0f), map.Y(764.5f), map.W(84.0f), map.H(3.5f), Color{25, 110, 220, 225});
        } else if (active == 1) {
            DrawRectangleRounded(map.Rec({598.0f, 728.0f, 146.0f, 41.0f}), 0.75f, 8, Color{0, 155, 78, 42});
            DrawRectangle(map.X(626.0f), map.Y(764.5f), map.W(84.0f), map.H(3.5f), Color{0, 155, 78, 235});
        }
    }

    Texture2D panel_{};
    Texture2D atlas_{};
    bool open_ = false;
    MatchResultMode mode_ = MatchResultMode::ROUND;
    int recordedRound_ = 0;
    int round_ = 1;
    int totalRounds_ = TableLayout::MatchRounds;
    bool matchFinished_ = false;
    int focusedButton_ = 1; // 0 = Back, 1 = Next
    std::optional<RoundResult> roundResult_;
    std::array<int, 2> matchScores_{};
    std::array<std::size_t, 2> capturedCounts_{};
    std::array<int, 2> koiCalls_{};
    std::array<std::vector<ComboDisplay>, 2> combos_{};
    std::vector<RoundLedgerEntry> ledger_;
};

} // namespace Ui
