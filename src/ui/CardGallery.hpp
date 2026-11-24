#pragma once

#include "ui/CardGalleryData.hpp"
#include "ui/CardViewer.hpp"
#include "ui/UiViewport.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Ui {

class CardGallery {
public:
    explicit CardGallery(const std::filesystem::path& appDirectory)
        : cardDirectory_(appDirectory / "options" / "card-gallery" / "cards") {}

    ~CardGallery() { UnloadMonthTextures(); }

    CardGallery(const CardGallery&) = delete;
    CardGallery& operator=(const CardGallery&) = delete;

    [[nodiscard]] bool Update(const GalleryInput& input) {
        fadeRemaining_ = std::max(0.0f, fadeRemaining_ - std::clamp(GetFrameTime(), 0.0f, 0.1f));

        if (viewer_.IsOpen()) {
            viewer_.Update(input);
            return false; // The grid underneath ignores every input.
        }

        const TableLayout::Point2 pointer = input.pointer;
        const int pointerMonth = MonthAt(pointer);
        if (input.moved || pointerMonth != hoveredMonth_) hoveredMonth_ = pointerMonth;
        hoveredThumbnail_ = ThumbnailAt(pointer);
        backHovered_ = Contains(BackButton, pointer);
        caseHovered_ = Contains(CardCase, pointer);

        if (input.pressed) {
            if (backHovered_) return true;
            if (pointerMonth != 0) {
                SelectMonth(pointerMonth);
            } else {
                const int card = ThumbnailAt(pointer);
                if (card >= 0) SelectCard(card);
            }
        }

        if (input.vertical != 0) SelectMonth(Wrap(selectedMonth_ - 1 + input.vertical, MonthCount) + 1);
        if (input.horizontal != 0) SelectCard(Wrap(selectedCard_ + input.horizontal, CardsPerMonth));

        EnsureCurrentMonthLoaded();
        OpenViewer(input);
        return false;
    }

    void Draw(const Viewport& viewport, Font headingFont, Font bodyFont) const {
        DrawRectangle(viewport.X(0), viewport.Y(0), viewport.W(1360), viewport.H(768), Backdrop);
        DrawRectangleRec(viewport.Rec({Panel.x + 8, Panel.y + 10, Panel.width, Panel.height}), Shadow);
        DrawRectangleRec(viewport.Rec(Panel), Silver);
        DrawRectangleRec(viewport.Rec({Panel.x + 4, Panel.y + 4, Panel.width - 8, Panel.height - 8}), PanelBlue);

        DrawHeader(viewport, headingFont);
        DrawMonthList(viewport, headingFont, bodyFont);
        DrawCardCase(viewport, bodyFont);
        DrawInfo(viewport, headingFont, bodyFont);
        DrawThumbnails(viewport, bodyFont);
        DrawFooter(viewport, headingFont, bodyFont);

        if (!viewer_.IsOpen()) return;
        viewer_.Draw(viewport, textures_[static_cast<std::size_t>(selectedCard_)]);
        DrawViewerOverlay(viewport, headingFont, bodyFont);
    }

    [[nodiscard]] bool ViewerOpen() const { return viewer_.IsOpen(); }

private:
    inline static constexpr int MonthCount = 12;
    inline static constexpr int CardsPerMonth = 4;
    inline static constexpr float FadeDuration = 0.12f;
    inline static constexpr Color Ink{9, 17, 32, 255};
    inline static constexpr Color Silver{196, 207, 227, 255};
    inline static constexpr Color Yellow{255, 213, 25, 255};
    inline static constexpr Color DarkKey{27, 36, 53, 255};
    inline static constexpr Color Green{0, 145, 65, 255};
    inline static constexpr Color Backdrop{2, 8, 26, 200};
    inline static constexpr Color Shadow{0, 0, 0, 190};
    inline static constexpr Color PanelBlue{8, 31, 112, 255};
    inline static constexpr Color FooterBlue{14, 44, 150, 255};
    inline static constexpr Color TileBlue{16, 48, 160, 255};
    inline static constexpr Color TitleBlock{6, 16, 64, 255};
    inline static constexpr Color StripOutline{90, 105, 140, 255};
    inline static constexpr Color White{255, 255, 255, 255};
    inline static constexpr TableLayout::Rect Panel{40, 28, 1280, 712};
    inline static constexpr TableLayout::Rect BackButton{570, 682, 220, 48};
    inline static constexpr TableLayout::Rect CardCase{376, 140, 343, 520};
    inline static constexpr float ThumbnailY = 462.0f;
    inline static constexpr float ThumbnailWidth = 128.0f;
    inline static constexpr float ThumbnailHeight = 197.0f;
    inline static constexpr float ThumbnailGap = 10.6666667f;

    static bool Contains(TableLayout::Rect rect, TableLayout::Point2 point) {
        return point.x >= rect.x && point.x < rect.x + rect.width &&
               point.y >= rect.y && point.y < rect.y + rect.height;
    }

    static int Wrap(int value, int count) {
        value %= count;
        return value < 0 ? value + count : value;
    }

    static TableLayout::Rect MonthStrip(int month) {
        return {64, 142.0f + 43.5f * static_cast<float>(month - 1), 296, 38};
    }

    static TableLayout::Rect ThumbnailRect(int index) {
        return {752.0f + static_cast<float>(index) * (ThumbnailWidth + ThumbnailGap),
                ThumbnailY, ThumbnailWidth, ThumbnailHeight};
    }

    static int MonthAt(TableLayout::Point2 point) {
        for (int month = 1; month <= MonthCount; ++month) {
            if (Contains(MonthStrip(month), point)) return month;
        }
        return 0;
    }

    static int ThumbnailAt(TableLayout::Point2 point) {
        for (int index = 0; index < CardsPerMonth; ++index) {
            if (Contains(ThumbnailRect(index), point)) return index;
        }
        return -1;
    }

    static std::size_t DataIndex(int month, int card) {
        return static_cast<std::size_t>((month - 1) * CardsPerMonth + card);
    }

    const CardGalleryEntry& SelectedEntry() const {
        return CardGalleryData[DataIndex(selectedMonth_, selectedCard_)];
    }

    void SelectMonth(int month) {
        if (month == selectedMonth_) return;
        selectedMonth_ = month;
        fadeRemaining_ = FadeDuration;
    }

    void SelectCard(int card) {
        if (card == selectedCard_) return;
        selectedCard_ = card;
        fadeRemaining_ = FadeDuration;
    }

    void UnloadMonthTextures() {
        for (Texture2D& texture : textures_) {
            if (texture.id != 0) UnloadTexture(texture);
            texture = {};
        }
        failed_.fill(false);
        loadedMonth_ = 0;
    }

    void EnsureCurrentMonthLoaded() {
        if (loadedMonth_ == selectedMonth_) return;
        UnloadMonthTextures();
        loadedMonth_ = selectedMonth_;

        for (int index = 0; index < CardsPerMonth; ++index) {
            const auto& entry = CardGalleryData[DataIndex(selectedMonth_, index)];
            const std::filesystem::path imagePath = cardDirectory_ / (std::string(entry.id) + ".jpg");
            const std::string pathText = imagePath.string();
            Texture2D texture = LoadTexture(pathText.c_str());
            if (texture.id == 0 || texture.width <= 0 || texture.height <= 0) {
                if (texture.id != 0) UnloadTexture(texture);
                failed_[static_cast<std::size_t>(index)] = true;
                TraceLog(LOG_WARNING, "Card gallery image missing or failed to load: %s", pathText.c_str());
                continue;
            }

            GenTextureMipmaps(&texture);
            SetTextureFilter(texture, texture.mipmaps > 1 ? TEXTURE_FILTER_TRILINEAR : TEXTURE_FILTER_BILINEAR);
            textures_[static_cast<std::size_t>(index)] = texture;
        }
    }

    static Vector2 Point(const Viewport& viewport, float x, float y) {
        return {static_cast<float>(viewport.X(x)), static_cast<float>(viewport.Y(y))};
    }

    static void Label(const Viewport& viewport, Font font, std::string_view text,
                      TableLayout::Rect box, float typeSize, Color color,
                      bool leftAligned = false, bool rightAligned = false) {
        const Rectangle rect = viewport.Rec(box);
        const std::string value(text);
        float size = static_cast<float>(viewport.Font(typeSize));
        const float spacing = 0.5f * viewport.scale;
        float width = MeasureTextEx(font, value.c_str(), size, spacing).x;
        if (width > rect.width && width > 0.0f) {
            size *= rect.width / width;
            width = MeasureTextEx(font, value.c_str(), size, spacing).x;
        }

        float x = rect.x + (rect.width - width) / 2.0f;
        if (leftAligned) x = rect.x;
        else if (rightAligned) x = rect.x + rect.width - width;
        DrawTextEx(font, value.c_str(), {x, rect.y + (rect.height - size) / 2.0f}, size, spacing, color);
    }

    static float TextWidth(Font font, std::string_view text, float size, float spacing) {
        const std::string value(text);
        return MeasureTextEx(font, value.c_str(), size, spacing).x;
    }

    static void Ellipsize(std::string& line, Font font, float size, float spacing, float width) {
        while (!line.empty() && TextWidth(font, line + "...", size, spacing) > width) {
            const std::size_t lastSpace = line.find_last_of(' ');
            if (lastSpace != std::string::npos) line.erase(lastSpace);
            else line.pop_back();
        }
        line.append("...");
    }

    static std::vector<std::string> WrapLines(Font font, std::string_view text,
                                               float size, float spacing, float width,
                                               std::size_t maximumLines) {
        std::vector<std::string> lines;
        std::string current;
        std::size_t position = 0;
        while (position < text.size()) {
            while (position < text.size() && text[position] == ' ') ++position;
            if (position == text.size()) break;
            std::size_t end = text.find(' ', position);
            if (end == std::string_view::npos) end = text.size();
            const std::string word(text.substr(position, end - position));
            const std::string candidate = current.empty() ? word : current + " " + word;
            if (current.empty() || TextWidth(font, candidate, size, spacing) <= width) {
                current = candidate;
            } else {
                lines.push_back(current);
                current = word;
            }
            position = end;
        }
        if (!current.empty()) lines.push_back(current);

        if (lines.size() > maximumLines) {
            lines.resize(maximumLines);
            if (!lines.empty()) Ellipsize(lines.back(), font, size, spacing, width);
        }
        return lines;
    }

    static std::size_t DrawWrappedText(const Viewport& viewport, Font font, std::string_view text,
                                       float x, float y, float width, float typeSize,
                                       float lineHeight, std::size_t maximumLines, Color color) {
        const float size = static_cast<float>(viewport.Font(typeSize));
        const float spacing = 0.5f * viewport.scale;
        const float maxWidth = viewport.W(width);
        const auto lines = WrapLines(font, text, size, spacing, maxWidth, maximumLines);
        for (std::size_t index = 0; index < lines.size(); ++index) {
            std::string line = lines[index];
            const float lineWidth = MeasureTextEx(font, line.c_str(), size, spacing).x;
            float fittedSize = size;
            if (lineWidth > maxWidth && lineWidth > 0.0f) fittedSize *= maxWidth / lineWidth;
            DrawTextEx(font, line.c_str(),
                       {static_cast<float>(viewport.X(x)),
                        static_cast<float>(viewport.Y(y + static_cast<float>(index) * lineHeight))},
                       fittedSize, spacing, color);
        }
        return lines.size();
    }

    void DrawHeader(const Viewport& viewport, Font headingFont) const {
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
        Label(viewport, headingFont, "CARD GALLERY", {148, 38, 900, 74}, 54, Yellow, true);
        Label(viewport, headingFont, CounterText(), {1170, 38, 122, 74}, 28, Silver, false, true);
    }

    std::string CounterText() const {
        const int number = static_cast<int>(DataIndex(selectedMonth_, selectedCard_)) + 1;
        std::string value = std::to_string(number);
        if (number < 10) value.insert(value.begin(), '0');
        return value + " / 48";
    }

    void DrawMonthList(const Viewport& viewport, Font headingFont, Font bodyFont) const {
        for (int month = 1; month <= MonthCount; ++month) {
            const auto strip = MonthStrip(month);
            const bool selected = month == selectedMonth_;
            const bool hovered = month == hoveredMonth_;
            const Color fill = selected ? Yellow : hovered ? DarkKey : Ink;
            const auto& entry = CardGalleryData[DataIndex(month, 0)];
            const Color text = selected ? Ink : White;
            const Color plant = selected ? Ink : Silver;

            DrawRectangleRec(viewport.Rec({108, strip.y, 252, strip.height}), fill);
            DrawRectangleLinesEx(viewport.Rec({108, strip.y, 252, strip.height}),
                                 std::max(1.0f, viewport.scale), StripOutline);
            DrawRectangleRec(viewport.Rec({64, strip.y, 44, strip.height}), TileBlue);
            DrawRectangleLinesEx(viewport.Rec({64, strip.y, 44, strip.height}),
                                 std::max(1.0f, viewport.scale * 2.0f), White);

            Label(viewport, headingFont, std::to_string(month), {64, strip.y, 44, strip.height}, 22, White);
            Label(viewport, bodyFont, entry.monthName, {120, strip.y, 132, strip.height}, 22, text, true);
            Label(viewport, bodyFont, entry.plant, {240, strip.y, 110, strip.height}, 15, plant, false, true);
        }
    }

    unsigned char FadeAlpha() const {
        const float amount = 1.0f - std::clamp(fadeRemaining_ / FadeDuration, 0.0f, 1.0f);
        return static_cast<unsigned char>(amount * 255.0f);
    }

    static void DrawTexture(const Viewport& viewport, Texture2D texture,
                            TableLayout::Rect destination, unsigned char alpha) {
        const Rectangle source{0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)};
        DrawTexturePro(texture, source, viewport.Rec(destination), {0, 0}, 0.0f,
                       Color{255, 255, 255, alpha});
    }

    void DrawMissing(const Viewport& viewport, Font bodyFont, TableLayout::Rect destination,
                     float typeSize, unsigned char alpha) const {
        DrawRectangleRec(viewport.Rec(destination), Color{9, 17, 32, alpha});
        DrawRectangleLinesEx(viewport.Rec(destination), std::max(1.0f, viewport.scale), StripOutline);
        Label(viewport, bodyFont, "IMAGE MISSING", destination, typeSize, Color{255, 255, 255, alpha});
    }

    void DrawCardCase(const Viewport& viewport, Font bodyFont) const {
        DrawRectangleRec(viewport.Rec(CardCase), Color{9, 17, 32, 235});
        DrawRectangleLinesEx(viewport.Rec(CardCase),
                             std::max(1.0f, viewport.scale * 2.0f),
                             caseHovered_ && !viewer_.IsOpen() ? Yellow : Silver);
        DrawRectangleRec(viewport.Rec({386, 150, 327, 504}), Color{0, 0, 0, 64});
        DrawRectangleRec(viewport.Rec({388, 152, 327, 504}), Color{0, 0, 0, 96});

        const auto& texture = textures_[static_cast<std::size_t>(selectedCard_)];
        const TableLayout::Rect cardRect{384, 148, 327, 504};
        if (texture.id != 0) {
            DrawTexture(viewport, texture, cardRect, FadeAlpha());
        } else if (failed_[static_cast<std::size_t>(selectedCard_)]) {
            DrawMissing(viewport, bodyFont, cardRect, 18, FadeAlpha());
        }
    }

    static std::string Uppercase(std::string_view text) {
        std::string value(text);
        for (char& character : value) {
            if (character >= 'a' && character <= 'z')
                character = static_cast<char>(character - 'a' + 'A');
        }
        return value;
    }

    void DrawInfo(const Viewport& viewport, Font headingFont, Font bodyFont) const {
        const auto& entry = SelectedEntry();
        const int cardNumber = selectedCard_ + 1;

        DrawRectangleRec(viewport.Rec({752, 140, 112, 56}), TileBlue);
        DrawRectangleLinesEx(viewport.Rec({752, 140, 112, 56}),
                             std::max(1.0f, viewport.scale), White);
        Label(viewport, headingFont, entry.id, {752, 140, 112, 56}, 30, White);
        DrawRectangleRec(viewport.Rec({864, 140, 432, 56}), Ink);
        Label(viewport, headingFont, Uppercase(entry.kind), {880, 140, 304, 56}, 30, Yellow, true);
        Label(viewport, bodyFont, std::to_string(cardNumber) + " / 4", {1190, 140, 96, 56}, 22, Silver, false, true);

        DrawRectangleRec(viewport.Rec({752, 204, 112, 48}), TileBlue);
        DrawRectangleLinesEx(viewport.Rec({752, 204, 112, 48}),
                             std::max(1.0f, viewport.scale), White);
        Label(viewport, bodyFont, "TRAIN", {752, 204, 112, 48}, 20, White);
        DrawRectangleRec(viewport.Rec({864, 204, 432, 48}), Ink);
        Label(viewport, bodyFont, entry.trainModel, {878, 204, 408, 48}, 20, White, true);

        DrawRectangleRec(viewport.Rec({752, 260, 544, 184}), Color{9, 17, 32, 235});
        DrawRectangleLinesEx(viewport.Rec({752, 260, 544, 184}),
                             std::max(1.0f, viewport.scale), StripOutline);
        const std::size_t descriptionLines = DrawWrappedText(
            viewport, bodyFont, entry.description, 768, 276, 512, 21, 26, 3, White);
        const float dividerY = 276.0f + static_cast<float>(descriptionLines) * 26.0f + 8.0f;
        DrawRectangleRec(viewport.Rec({768, dividerY, 512, 1}), StripOutline);
        const float motifY = dividerY + 9.0f;
        DrawWrappedText(viewport, bodyFont, entry.motifTwist, 768, motifY, 512, 18, 22, 2, Silver);
    }

    void DrawThumbnails(const Viewport& viewport, Font bodyFont) const {
        for (int index = 0; index < CardsPerMonth; ++index) {
            const auto rect = ThumbnailRect(index);
            const auto& texture = textures_[static_cast<std::size_t>(index)];
            if (texture.id != 0) DrawTexture(viewport, texture, rect, 255);
            else if (failed_[static_cast<std::size_t>(index)]) DrawMissing(viewport, bodyFont, rect, 13, 255);
            else DrawRectangleRec(viewport.Rec(rect), Ink);

            if (index != selectedCard_) {
                const bool hovered = index == hoveredThumbnail_;
                DrawRectangleRec(viewport.Rec(rect), Color{0, 0, 0, static_cast<unsigned char>(hovered ? 60 : 120)});
            } else {
                const TableLayout::Rect frame{rect.x - 4, rect.y - 4, rect.width + 8, rect.height + 8};
                DrawRectangleLinesEx(viewport.Rec(frame), std::max(1.0f, viewport.scale * 4.0f), Yellow);
            }
        }
    }

    void DrawFooter(const Viewport& viewport, Font headingFont, Font bodyFont) const {
        // The extra hint always fits once Label() shrinks it, and the BACK
        // button keeps its exact place so it never jumps between screens.
        DrawRectangleRec(viewport.Rec({44, 676, 1272, 60}), FooterBlue);
        Label(viewport, bodyFont, "UP/DOWN  MONTH      LEFT/RIGHT  CARD      CLICK  3D VIEW",
              {64, 676, 490, 60}, 18, Silver, true);
        Label(viewport, bodyFont, "ESC  BACK", {1170, 676, 126, 60}, 18, Silver, false, true);

        DrawRectangleRec(viewport.Rec(BackButton), backHovered_ ? TileBlue : Ink);
        DrawRectangleLinesEx(viewport.Rec(BackButton), std::max(1.0f, viewport.scale * 2.0f), White);
        DrawTriangle(Point(viewport, 594, 706), Point(viewport, 608, 696),
                     Point(viewport, 608, 716), White);
        Label(viewport, headingFont, "BACK", BackButton, 28, White);
    }

    void OpenViewer(const GalleryInput& input) {
        const bool loaded = textures_[static_cast<std::size_t>(selectedCard_)].id != 0;
        if (!loaded) return; // A missing image must not open an empty viewer.
        const bool onCase = Contains(CardCase, input.pointer) && input.pressed;
        if (!onCase && !input.enter) return;
        viewer_.Open(input.antiAliasing);
        caseHovered_ = false;
    }

    void DrawViewerOverlay(const Viewport& viewport, Font headingFont, Font bodyFont) const {
        const auto& entry = SelectedEntry();
        DrawRectangleRec(viewport.Rec({64, 40, 112, 56}), TileBlue);
        DrawRectangleLinesEx(viewport.Rec({64, 40, 112, 56}),
                             std::max(1.0f, viewport.scale * 2.0f), White);
        Label(viewport, headingFont, entry.id, {64, 40, 112, 56}, 30, White);

        DrawRectangleRec(viewport.Rec({176, 40, 330, 56}), Ink);
        DrawRectangleLinesEx(viewport.Rec({176, 40, 330, 56}),
                             std::max(1.0f, viewport.scale), StripOutline);
        // 15 px left padding, ending where the right-aligned zoom readout starts.
        Label(viewport, headingFont, Uppercase(entry.kind), {191, 40, 172, 56}, 30, Yellow, true);

        char zoomText[16]{};
        std::snprintf(zoomText, sizeof(zoomText), "ZOOM %.1fX", static_cast<double>(viewer_.Zoom()));
        Label(viewport, bodyFont, zoomText, {363, 40, 127, 56}, 20, Silver, false, true);

        DrawRectangleRec(viewport.Rec({44, 676, 1272, 60}), FooterBlue);
        Label(viewport, bodyFont, "DRAG  ROTATE      WHEEL  ZOOM",
              {64, 676, 490, 60}, 18, Silver, true);
        Label(viewport, bodyFont, "ESC  BACK", {1170, 676, 126, 60}, 18, Silver, false, true);

        DrawRectangleRec(viewport.Rec(BackButton), viewer_.BackHovered() ? TileBlue : Ink);
        DrawRectangleLinesEx(viewport.Rec(BackButton), std::max(1.0f, viewport.scale * 2.0f), White);
        DrawTriangle(Point(viewport, 594, 706), Point(viewport, 608, 696),
                     Point(viewport, 608, 716), White);
        Label(viewport, headingFont, "BACK", BackButton, 28, White);
    }

    std::filesystem::path cardDirectory_;
    std::array<Texture2D, CardsPerMonth> textures_{};
    std::array<bool, CardsPerMonth> failed_{};
    int loadedMonth_ = 0;
    int selectedMonth_ = 1;
    int selectedCard_ = 0;
    int hoveredMonth_ = 0;
    int hoveredThumbnail_ = -1;
    float fadeRemaining_ = 0.0f;
    bool backHovered_ = false;
    bool caseHovered_ = false;
    CardViewer viewer_;
};

} // namespace Ui
