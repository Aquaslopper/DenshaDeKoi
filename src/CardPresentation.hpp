#pragma once

#include "TableGame.hpp" // Card enums must be parsed before Raylib color macros.
#include "TableView.hpp"
#include <array>
#include <optional>

// Presentation state only. The game continues to use flat table coordinates.
struct CardPose {
    Vector3 center{}, right{}, down{}, normal{};
    float halfWidth = 0, halfHeight = 0;
};
struct CardHit {
    TableCard card{};
    float u = 0, v = 0, distance = 0;
};

class CardPresentation {
public:
    // The cursor is in canvas coordinates; no camera state is changed.
    void Update(const TableCardList& cards, const Camera3D& camera, Vector2 cursor, float dt);
    [[nodiscard]] std::optional<CardHit> Pick(const TableCardList& cards, const Camera3D& camera, Vector2 cursor) const;
    [[nodiscard]] CardPose Pose(const TableCard& card, const Camera3D& camera, Vector2 cursor) const;
    [[nodiscard]] std::optional<TableLayout::Rect> DragVisualRect(const TableCardList& cards,
                                                                   const Camera3D& camera, Vector2 cursor) const;
    void BeginDrag(const CardHit& hit) noexcept;
    void EndDrag() noexcept;
    [[nodiscard]] int InspectedCard() const noexcept { return inspected_; }
    // Used by the screen-space capture browser, whose hit boxes are 2D.
    void InspectCaptured(int id) noexcept { inspected_ = id >= 0 && id < 48 ? id : -1; }
    [[nodiscard]] float Lift(int id) const noexcept;
    [[nodiscard]] static Vector3 PointOnCard(const CardPose& pose, float u, float v) noexcept;
    [[nodiscard]] static bool IntersectCard(const TableRay& ray, const CardPose& pose, CardHit& hit) noexcept;
private:
    std::array<float, 48> lift_{};
    int inspected_ = -1;
    int dragId_ = -1;
    float grabU_ = 0.5f, grabV_ = 0.5f;
};
