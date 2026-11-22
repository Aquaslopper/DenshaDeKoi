#pragma once

#include "AtlasLayout.hpp"

#include <raylib.h>

class AtlasManager {
public:
    static constexpr int MonthCount = AtlasLayout::MonthCount;
    static constexpr int CardsPerMonth = AtlasLayout::CardsPerMonth;

    // A Raylib window/context must exist before construction and until destruction.
    explicit AtlasManager(const char* path = "cards_atlas.png");
    ~AtlasManager();

    AtlasManager(const AtlasManager&) = delete;
    AtlasManager& operator=(const AtlasManager&) = delete;
    AtlasManager(AtlasManager&&) = delete;
    AtlasManager& operator=(AtlasManager&&) = delete;

    [[nodiscard]] Rectangle GetCardSourceRect(int month, int cardIndex) const;
    void DrawCard(int month, int cardIndex, Vector2 position, float scale) const;

    [[nodiscard]] float GetCardWidth() const { return cardWidth_; }
    [[nodiscard]] float GetCardHeight() const { return cardHeight_; }

private:
    Texture2D atlas_{};
    float cardWidth_ = 0.0f;
    float cardHeight_ = 0.0f;
};
