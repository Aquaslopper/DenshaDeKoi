#pragma once

#include "ui/UiViewport.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

namespace Ui {

// One selectable table backdrop. The stock placeholders all reuse the original
// station artwork; replacing these JPGs is all that is needed for new scenes.
struct MatchBackgroundEntry {
    const char* label;
    const char* fileName;
};

inline constexpr std::array<MatchBackgroundEntry, 5> MatchBackgrounds{{
    {"TOKYO LOOP", "match-background-1.jpg"},
    {"SAKURA LINE", "match-background-2.jpg"},
    {"SEASIDE LOCAL", "match-background-3.jpg"},
    {"MOONLIGHT YARD", "match-background-4.jpg"},
    {"MOUNTAIN PASS", "match-background-5.jpg"},
}};

// Round counts offered before a match. The match always plays every round.
inline constexpr std::array<int, 3> MatchRoundChoices{3, 4, 5};

struct MatchSetup {
    int rounds = MatchRoundChoices.front();
    int background = 0; // Index into MatchBackgrounds.
};

struct MatchSetupInput {
    TableLayout::Point2 pointer;
    bool moved, pressed;
    int vertical, horizontal; // Keyboard steps: -1, 0, +1.
    bool activate;            // Enter / Space.
    bool back;                // Esc.
};

enum class MatchSetupResult { NONE, START, BACK };

// Pre-match settings popup shown over the title screen. It follows the card
// gallery's visual language (traffic-light header, blue panel, footer, BACK key)
// while keeping its own hit boxes and navigation model. The preview texture is
// owned here; fonts and the viewport are borrowed from the caller.
class MatchSetupMenu {
public:
    explicit MatchSetupMenu(std::filesystem::path appDirectory);
    ~MatchSetupMenu();
    MatchSetupMenu(const MatchSetupMenu&) = delete;
    MatchSetupMenu& operator=(const MatchSetupMenu&) = delete;

    void Open(const MatchSetup& setup);
    void Close();
    [[nodiscard]] bool IsOpen() const { return open_; }
    [[nodiscard]] const MatchSetup& Setup() const { return setup_; }

    MatchSetupResult Update(const MatchSetupInput& input);
    void Draw(const Viewport& viewport, Font headingFont, Font bodyFont) const;

    // Shared layout in the fixed 1360x768 UI space, used for drawing and input.
    inline static constexpr TableLayout::Rect Panel{40, 28, 1280, 712};
    inline static constexpr TableLayout::Rect PreviewBox{64, 142, 672, 378};
    inline static constexpr TableLayout::Rect InfoBox{752, 338, 544, 182};
    inline static constexpr TableLayout::Rect StartButton{432, 682, 240, 48};
    inline static constexpr TableLayout::Rect BackButton{688, 682, 240, 48};

    [[nodiscard]] static TableLayout::Rect RoundKey(int index) {
        return {1000.0f + static_cast<float>(index) * 96.0f, 207.0f, 88.0f, 48.0f};
    }
    [[nodiscard]] static TableLayout::Rect BackgroundArrow(int direction) {
        return {direction < 0 ? 1000.0f : 1240.0f, 273.0f, 44.0f, 48.0f};
    }
    [[nodiscard]] static TableLayout::Rect BackgroundValue() { return {1052.0f, 273.0f, 180.0f, 48.0f}; }
    [[nodiscard]] static TableLayout::Rect BackgroundSlot(int index) {
        return {64.0f + static_cast<float>(index) * 136.0f, 536.0f, 128.0f, 120.0f};
    }

private:
    enum class Row { Rounds, Background, Start, Back };

    static bool Contains(TableLayout::Rect rect, TableLayout::Point2 point) {
        return point.x >= rect.x && point.x < rect.x + rect.width &&
               point.y >= rect.y && point.y < rect.y + rect.height;
    }
    static int Wrap(int value, int count) {
        value %= count;
        return value < 0 ? value + count : value;
    }

    void ClampSetup();
    void EnsurePreviewLoaded();
    void UnloadPreview();
    // Maps the pointer onto a row/key; returns true when something was hit.
    bool FocusAt(TableLayout::Point2 point);
    void StepRounds(int direction);
    void StepBackground(int direction);

    void DrawHeader(const Viewport& viewport, Font headingFont) const;
    void DrawPreview(const Viewport& viewport, Font bodyFont) const;
    void DrawSelection(const Viewport& viewport, Font headingFont, Font bodyFont) const;
    void DrawInfo(const Viewport& viewport, Font bodyFont) const;
    void DrawFooter(const Viewport& viewport, Font headingFont, Font bodyFont) const;

    std::filesystem::path directory_;
    MatchSetup setup_{};
    Row focus_ = Row::Rounds;
    int hoveredKey_ = -1; // 0..2 rounds, 3/4 arrows, 5 start, 6 back; -1 when none.
    bool open_ = false;
    Texture2D preview_{};
    int loadedBackground_ = -1;
    bool previewFailed_ = false;
};

} // namespace Ui
