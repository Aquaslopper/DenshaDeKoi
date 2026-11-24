#include "ui/MatchSetupMenu.hpp"

#include <algorithm>
#include <string>

namespace Ui {
namespace {
constexpr int RoundCount = static_cast<int>(MatchRoundChoices.size());
constexpr int BackgroundCount = static_cast<int>(MatchBackgrounds.size());

// Pointer keys: 0..2 round presets, 3/4 background arrows, 5 start, 6 back.
constexpr int LeftArrowKey = RoundCount;
constexpr int RightArrowKey = RoundCount + 1;
constexpr int StartKey = RoundCount + 2;
constexpr int BackKey = RoundCount + 3;

constexpr Color Ink{9, 17, 32, 255};
constexpr Color Silver{196, 207, 227, 255};
constexpr Color Yellow{255, 213, 25, 255};
constexpr Color DarkKey{27, 36, 53, 255};
constexpr Color Green{0, 145, 65, 255};
constexpr Color Backdrop{2, 8, 26, 200};
constexpr Color Shadow{0, 0, 0, 190};
constexpr Color PanelBlue{8, 31, 112, 255};
constexpr Color FooterBlue{14, 44, 150, 255};
constexpr Color TileBlue{16, 48, 160, 255};
constexpr Color TitleBlock{6, 16, 64, 255};
constexpr Color StripOutline{90, 105, 140, 255};
constexpr Color White{255, 255, 255, 255};

void Label(const Viewport& viewport, Font font, const char* text, TableLayout::Rect box,
           float typeSize, Color color, bool leftAligned = false, bool rightAligned = false) {
    const Rectangle rect = viewport.Rec(box);
    const float size = static_cast<float>(viewport.Font(typeSize));
    const float spacing = 0.5f * viewport.scale;
    float width = MeasureTextEx(font, text, size, spacing).x;
    float fitted = size;
    if (width > rect.width && width > 0.0f) {
        fitted *= rect.width / width;
        width = MeasureTextEx(font, text, fitted, spacing).x;
    }
    float x = rect.x + (rect.width - width) / 2.0f;
    if (leftAligned) x = rect.x;
    else if (rightAligned) x = rect.x + rect.width - width;
    DrawTextEx(font, text, {x, rect.y + (rect.height - fitted) / 2.0f}, fitted, spacing, color);
}

void KeyBox(const Viewport& viewport, const char* text, Font font, TableLayout::Rect rect,
            bool selected, bool focused, float typeSize, bool hovered) {
    const Color fill = selected ? Yellow : focused ? DarkKey : Ink;
    const Color border = selected ? Yellow : (focused || hovered) ? Silver : StripOutline;
    DrawRectangleRec(viewport.Rec(rect), fill);
    DrawRectangleLinesEx(viewport.Rec(rect), std::max(1.0f, viewport.scale * 2.0f), border);
    Label(viewport, font, text, rect, typeSize, selected ? Ink : White);
}

void ArrowKey(const Viewport& viewport, TableLayout::Rect rect, int direction, bool focused, bool hovered) {
    DrawRectangleRec(viewport.Rec(rect), focused ? DarkKey : Ink);
    DrawRectangleLinesEx(viewport.Rec(rect), std::max(1.0f, viewport.scale),
                         (focused || hovered) ? Silver : StripOutline);
    const float cx = rect.x + rect.width / 2.0f, cy = rect.y + rect.height / 2.0f;
    const float dx = 10.0f * static_cast<float>(direction);
    DrawTriangle({static_cast<float>(viewport.X(cx + dx)), static_cast<float>(viewport.Y(cy))},
                 {static_cast<float>(viewport.X(cx - dx)), static_cast<float>(viewport.Y(cy - 12.0f))},
                 {static_cast<float>(viewport.X(cx - dx)), static_cast<float>(viewport.Y(cy + 12.0f))},
                 Yellow);
}

std::string Number(int value) { return std::to_string(value); }
} // namespace

MatchSetupMenu::MatchSetupMenu(std::filesystem::path appDirectory)
    : directory_(std::move(appDirectory)) {}

MatchSetupMenu::~MatchSetupMenu() { UnloadPreview(); }

void MatchSetupMenu::Open(const MatchSetup& setup) {
    setup_ = setup;
    ClampSetup();
    focus_ = Row::Rounds;
    hoveredKey_ = -1;
    open_ = true;
    EnsurePreviewLoaded();
}

void MatchSetupMenu::Close() {
    open_ = false;
    hoveredKey_ = -1;
    UnloadPreview();
}

void MatchSetupMenu::ClampSetup() {
    if (std::find(MatchRoundChoices.begin(), MatchRoundChoices.end(), setup_.rounds) ==
        MatchRoundChoices.end()) {
        setup_.rounds = MatchRoundChoices.front();
    }
    setup_.background = Wrap(setup_.background, BackgroundCount);
}

void MatchSetupMenu::UnloadPreview() {
    if (preview_.id != 0) UnloadTexture(preview_);
    preview_ = {};
    loadedBackground_ = -1;
    previewFailed_ = false;
}

void MatchSetupMenu::EnsurePreviewLoaded() {
    if (loadedBackground_ == setup_.background && (preview_.id != 0 || previewFailed_)) return;
    UnloadPreview();
    loadedBackground_ = setup_.background;
    const std::string path = (directory_ / "options" / "match-backgrounds" /
                              MatchBackgrounds[static_cast<std::size_t>(setup_.background)].fileName).string();
    preview_ = LoadTexture(path.c_str());
    if (preview_.id == 0 || preview_.width <= 0 || preview_.height <= 0) {
        if (preview_.id != 0) UnloadTexture(preview_);
        preview_ = {};
        previewFailed_ = true;
        TraceLog(LOG_WARNING, "Match background missing or failed to load: %s", path.c_str());
        return;
    }
    previewFailed_ = false;
    GenTextureMipmaps(&preview_);
    SetTextureFilter(preview_, preview_.mipmaps > 1 ? TEXTURE_FILTER_TRILINEAR : TEXTURE_FILTER_BILINEAR);
}

void MatchSetupMenu::StepRounds(int direction) {
    const auto it = std::find(MatchRoundChoices.begin(), MatchRoundChoices.end(), setup_.rounds);
    const int index = it == MatchRoundChoices.end() ? 0 : static_cast<int>(it - MatchRoundChoices.begin());
    setup_.rounds = MatchRoundChoices[static_cast<std::size_t>(Wrap(index + direction, RoundCount))];
}

void MatchSetupMenu::StepBackground(int direction) {
    setup_.background = Wrap(setup_.background + direction, BackgroundCount);
    EnsurePreviewLoaded();
}

bool MatchSetupMenu::FocusAt(TableLayout::Point2 point) {
    for (int index = 0; index < RoundCount; ++index) {
        if (Contains(RoundKey(index), point)) { hoveredKey_ = index; focus_ = Row::Rounds; return true; }
    }
    if (Contains(BackgroundArrow(-1), point)) { hoveredKey_ = LeftArrowKey; focus_ = Row::Background; return true; }
    if (Contains(BackgroundArrow(1), point)) { hoveredKey_ = RightArrowKey; focus_ = Row::Background; return true; }
    if (Contains(BackgroundValue(), point)) { hoveredKey_ = RightArrowKey; focus_ = Row::Background; return true; }
    for (int index = 0; index < BackgroundCount; ++index) {
        if (Contains(BackgroundSlot(index), point)) { hoveredKey_ = RightArrowKey; focus_ = Row::Background; return true; }
    }
    if (Contains(StartButton, point)) { hoveredKey_ = StartKey; focus_ = Row::Start; return true; }
    if (Contains(BackButton, point)) { hoveredKey_ = BackKey; focus_ = Row::Back; return true; }
    hoveredKey_ = -1;
    return false;
}

MatchSetupResult MatchSetupMenu::Update(const MatchSetupInput& input) {
    if (!open_) return MatchSetupResult::NONE;
    if (input.back) return MatchSetupResult::BACK;

    // Hover moves focus; a press is committed against the key under the pointer.
    if (input.moved || input.pressed) FocusAt(input.pointer);

    if (input.activate) {
        if (focus_ == Row::Start) return MatchSetupResult::START;
        if (focus_ == Row::Back) return MatchSetupResult::BACK;
    }

    if (input.pressed) {
        for (int index = 0; index < RoundCount; ++index) {
            if (Contains(RoundKey(index), input.pointer)) {
                setup_.rounds = MatchRoundChoices[static_cast<std::size_t>(index)];
                return MatchSetupResult::NONE;
            }
        }
        bool slotHit = false;
        for (int index = 0; index < BackgroundCount; ++index) {
            if (!Contains(BackgroundSlot(index), input.pointer)) continue;
            slotHit = true;
            if (setup_.background != index) { setup_.background = index; EnsurePreviewLoaded(); }
            break;
        }
        if (slotHit) return MatchSetupResult::NONE;
        if (Contains(BackgroundArrow(-1), input.pointer)) StepBackground(-1);
        else if (Contains(BackgroundArrow(1), input.pointer)) StepBackground(1);
        else if (Contains(BackgroundValue(), input.pointer)) StepBackground(1);
        else if (Contains(StartButton, input.pointer)) return MatchSetupResult::START;
        else if (Contains(BackButton, input.pointer)) return MatchSetupResult::BACK;
        return MatchSetupResult::NONE;
    }

    if (input.vertical != 0) {
        focus_ = static_cast<Row>(Wrap(static_cast<int>(focus_) + input.vertical, 4));
        hoveredKey_ = -1;
    }
    if (input.horizontal != 0) {
        if (focus_ == Row::Rounds) StepRounds(input.horizontal);
        else if (focus_ == Row::Background) StepBackground(input.horizontal);
    }
    return MatchSetupResult::NONE;
}

void MatchSetupMenu::Draw(const Viewport& viewport, Font headingFont, Font bodyFont) const {
    DrawRectangle(viewport.X(0), viewport.Y(0), viewport.W(TableLayout::ImageWidth),
                  viewport.H(TableLayout::ImageHeight), Backdrop);
    DrawRectangleRec(viewport.Rec({Panel.x + 8, Panel.y + 10, Panel.width, Panel.height}), Shadow);
    DrawRectangleRec(viewport.Rec(Panel), Silver);
    DrawRectangleRec(viewport.Rec({Panel.x + 4, Panel.y + 4, Panel.width - 8, Panel.height - 8}), PanelBlue);

    DrawHeader(viewport, headingFont);
    DrawPreview(viewport, bodyFont);
    DrawSelection(viewport, headingFont, bodyFont);
    DrawInfo(viewport, bodyFont);
    DrawFooter(viewport, headingFont, bodyFont);
}

void MatchSetupMenu::DrawHeader(const Viewport& viewport, Font headingFont) const {
    DrawRectangleRec(viewport.Rec({44, 32, 1272, 92}), Green);
    DrawRectangleRec(viewport.Rec({58, 38, 50, 80}), Ink);
    DrawRectangleLinesEx(viewport.Rec({58, 38, 50, 80}), std::max(1.0f, viewport.scale), Silver);

    const Color dimLamp{68, 74, 83, 255};
    DrawCircle(viewport.X(83), viewport.Y(58), static_cast<float>(viewport.W(10)), dimLamp);
    DrawCircle(viewport.X(83), viewport.Y(80), static_cast<float>(viewport.W(10)), dimLamp);
    DrawCircle(viewport.X(83), viewport.Y(102), static_cast<float>(viewport.W(17)), Color{0, 220, 90, 32});
    DrawCircle(viewport.X(83), viewport.Y(102), static_cast<float>(viewport.W(13)), Color{0, 220, 90, 76});
    DrawCircle(viewport.X(83), viewport.Y(102), static_cast<float>(viewport.W(10)), Color{42, 238, 112, 255});

    DrawRectangleRec(viewport.Rec({124, 38, 1184, 74}), TitleBlock);
    Label(viewport, headingFont, "MATCH SETUP", {148, 38, 900, 74}, 54, Yellow, true);
    Label(viewport, headingFont, "START", {1170, 38, 122, 74}, 28, Silver, false, true);
}

void MatchSetupMenu::DrawPreview(const Viewport& viewport, Font bodyFont) const {
    DrawRectangleRec(viewport.Rec(PreviewBox), Color{9, 17, 32, 235});
    DrawRectangleLinesEx(viewport.Rec(PreviewBox), std::max(1.0f, viewport.scale * 2.0f), Yellow);

    if (preview_.id != 0) {
        // Cover-fit the artwork inside the preview so its 16:9 frame stays intact.
        const float sourceAspect = static_cast<float>(preview_.width) / static_cast<float>(preview_.height);
        const float boxAspect = PreviewBox.width / PreviewBox.height;
        Rectangle source{0, 0, static_cast<float>(preview_.width), static_cast<float>(preview_.height)};
        if (sourceAspect > boxAspect) {
            source.width = static_cast<float>(preview_.height) * boxAspect;
            source.x = (static_cast<float>(preview_.width) - source.width) / 2.0f;
        } else if (sourceAspect < boxAspect) {
            source.height = static_cast<float>(preview_.width) / boxAspect;
            source.y = (static_cast<float>(preview_.height) - source.height) / 2.0f;
        }
        DrawTexturePro(preview_, source, viewport.Rec(PreviewBox), {0, 0}, 0.0f, WHITE);
    } else {
        Label(viewport, bodyFont, previewFailed_ ? "PREVIEW MISSING" : "LOADING PREVIEW",
              PreviewBox, 22, Silver);
    }

    // The stock art is a placeholder, so the panel says so explicitly.
    DrawRectangleRec(viewport.Rec({64, 488, 672, 32}), Color{9, 17, 32, 210});
    Label(viewport, bodyFont,
          previewFailed_ ? "REPLACE OPTIONS/MATCH-BACKGROUNDS ART" : "PLACEHOLDER ART - DROP-IN READY",
          {76, 488, 648, 32}, 16, previewFailed_ ? Color{255, 138, 128, 255} : Silver, true);

    // Five drop-in slots, so the placeholders are obvious at a glance.
    for (int index = 0; index < BackgroundCount; ++index) {
        const TableLayout::Rect slot = BackgroundSlot(index);
        const bool selected = index == setup_.background;
        DrawRectangleRec(viewport.Rec(slot), selected ? TileBlue : Ink);
        DrawRectangleLinesEx(viewport.Rec(slot), std::max(1.0f, viewport.scale * (selected ? 3.0f : 1.5f)),
                             selected ? Yellow : StripOutline);
        Label(viewport, bodyFont, Number(index + 1).c_str(), slot, 30, selected ? White : Silver);
    }
}

void MatchSetupMenu::DrawSelection(const Viewport& viewport, Font headingFont, Font bodyFont) const {
    const bool roundsFocus = focus_ == Row::Rounds;
    const bool backgroundFocus = focus_ == Row::Background;

    // Row 1: rounds.
    DrawRectangleRec(viewport.Rec({752, 204, 96, 54}), TileBlue);
    DrawRectangleLinesEx(viewport.Rec({752, 204, 96, 54}), std::max(1.0f, viewport.scale * 2.0f), White);
    Label(viewport, headingFont, "1", {752, 204, 96, 54}, 30, White);
    DrawRectangleRec(viewport.Rec({848, 204, 448, 54}), roundsFocus ? DarkKey : Ink);
    DrawRectangleLinesEx(viewport.Rec({848, 204, 448, 54}), std::max(1.0f, viewport.scale), StripOutline);
    Label(viewport, bodyFont, "ROUNDS", {864, 204, 136, 54}, 24, roundsFocus ? Yellow : White, true);
    for (int index = 0; index < RoundCount; ++index) {
        const bool selected = MatchRoundChoices[static_cast<std::size_t>(index)] == setup_.rounds;
        KeyBox(viewport, Number(MatchRoundChoices[static_cast<std::size_t>(index)]).c_str(), headingFont,
               RoundKey(index), selected, roundsFocus && selected, 28, hoveredKey_ == index);
    }

    // Row 2: background.
    DrawRectangleRec(viewport.Rec({752, 270, 96, 54}), TileBlue);
    DrawRectangleLinesEx(viewport.Rec({752, 270, 96, 54}), std::max(1.0f, viewport.scale * 2.0f), White);
    Label(viewport, headingFont, "2", {752, 270, 96, 54}, 30, White);
    DrawRectangleRec(viewport.Rec({848, 270, 448, 54}), backgroundFocus ? DarkKey : Ink);
    DrawRectangleLinesEx(viewport.Rec({848, 270, 448, 54}), std::max(1.0f, viewport.scale), StripOutline);
    Label(viewport, bodyFont, "BACKGROUND", {864, 270, 136, 54}, 24, backgroundFocus ? Yellow : White, true);
    ArrowKey(viewport, BackgroundArrow(-1), -1, backgroundFocus, hoveredKey_ == LeftArrowKey);
    ArrowKey(viewport, BackgroundArrow(1), 1, backgroundFocus, hoveredKey_ == RightArrowKey);
    DrawRectangleRec(viewport.Rec(BackgroundValue()), backgroundFocus ? Yellow : Ink);
    DrawRectangleLinesEx(viewport.Rec(BackgroundValue()), std::max(1.0f, viewport.scale),
                         backgroundFocus ? Yellow : StripOutline);
    Label(viewport, headingFont, Number(setup_.background + 1).c_str(), BackgroundValue(), 28,
          backgroundFocus ? Ink : White);
}

void MatchSetupMenu::DrawInfo(const Viewport& viewport, Font bodyFont) const {
    DrawRectangleRec(viewport.Rec(InfoBox), Color{9, 17, 32, 235});
    DrawRectangleLinesEx(viewport.Rec(InfoBox), std::max(1.0f, viewport.scale), StripOutline);
    const auto& background = MatchBackgrounds[static_cast<std::size_t>(setup_.background)];
    Label(viewport, bodyFont, background.label, {768, 344, 512, 38}, 24, Yellow, true);

    const std::string rounds = "BEST OF " + Number(setup_.rounds) + " ROUNDS";
    Label(viewport, bodyFont, rounds.c_str(), {768, 388, 512, 28}, 19, White, true);
    Label(viewport, bodyFont, "WHO WINS MORE ROUNDS TAKES THE MATCH",
          {768, 416, 512, 26}, 16, Silver, true);
    Label(viewport, bodyFont, "THE LOGO BEAT AND DEALER PICK STAY AS BEFORE",
          {768, 442, 512, 26}, 16, Silver, true);
    Label(viewport, bodyFont, "UP/DOWN AND ENTER WORK AS WELL",
          {768, 468, 512, 26}, 16, Silver, true);
}

void MatchSetupMenu::DrawFooter(const Viewport& viewport, Font headingFont, Font bodyFont) const {
    DrawRectangleRec(viewport.Rec({44, 676, 1272, 60}), FooterBlue);
    Label(viewport, bodyFont, "CLICK OR UP/DOWN AND ENTER", {64, 676, 300, 60}, 18, Silver, true);
    Label(viewport, bodyFont, "ESC  BACK", {1170, 676, 126, 60}, 18, Silver, false, true);

    KeyBox(viewport, "START MATCH", headingFont, StartButton, focus_ == Row::Start,
           focus_ == Row::Start, 28, hoveredKey_ == StartKey);
    KeyBox(viewport, "BACK", headingFont, BackButton, focus_ == Row::Back,
           focus_ == Row::Back, 28, hoveredKey_ == BackKey);
    // A white inner edge separates both keys from the footer band behind them.
    DrawRectangleLinesEx(viewport.Rec(StartButton), std::max(1.0f, viewport.scale), White);
    DrawRectangleLinesEx(viewport.Rec(BackButton), std::max(1.0f, viewport.scale), White);
}

} // namespace Ui
