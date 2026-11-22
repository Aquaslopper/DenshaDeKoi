#include "AtlasManager.hpp"

#include <stdexcept>
#include <string>

AtlasManager::AtlasManager(const char* path) {
    atlas_ = LoadTexture(path);
    if (atlas_.id == 0 || atlas_.width <= 0 || atlas_.height <= 0) {
        if (atlas_.id != 0) {
            UnloadTexture(atlas_);
        }
        throw std::runtime_error(std::string("Could not load card atlas: ") + path);
    }

    cardWidth_ = static_cast<float>(atlas_.width) / MonthCount;
    cardHeight_ = static_cast<float>(atlas_.height) / CardsPerMonth;
    // Nearest-neighbor sampling avoids bleeding between adjacent atlas cells.
    SetTextureFilter(atlas_, TEXTURE_FILTER_POINT);
}

AtlasManager::~AtlasManager() {
    UnloadTexture(atlas_);
}

Rectangle AtlasManager::GetCardSourceRect(int month, int cardIndex) const {
    const auto rect = AtlasLayout::GetCardSourceRect(static_cast<float>(atlas_.width),
                                                       static_cast<float>(atlas_.height),
                                                       month, cardIndex);
    return {rect.x, rect.y, rect.width, rect.height};
}

void AtlasManager::DrawCard(int month, int cardIndex, Vector2 position, float scale) const {
    const Rectangle source = GetCardSourceRect(month, cardIndex);
    const Rectangle destination{position.x, position.y, cardWidth_ * scale, cardHeight_ * scale};
    DrawTexturePro(atlas_, source, destination, {0.0f, 0.0f}, 0.0f, WHITE);
}
