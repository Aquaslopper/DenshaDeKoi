#include "CardPresentation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
Vector3 Add(Vector3 a, Vector3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
Vector3 Sub(Vector3 a, Vector3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
Vector3 Scale(Vector3 a, float s) { return {a.x*s, a.y*s, a.z*s}; }
float Dot(Vector3 a, Vector3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vector3 Cross(Vector3 a, Vector3 b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
Vector3 Normalize(Vector3 a) {
    const float length = std::sqrt(Dot(a,a));
    return length > 1e-7f && std::isfinite(length) ? Scale(a, 1/length) : Vector3{};
}
Vector3 Rotate(Vector3 v, Vector3 axis, float angle) {
    const float s = std::sin(angle), c = std::cos(angle);
    return Add(Add(Scale(v,c), Scale(Cross(axis,v),s)), Scale(axis,Dot(axis,v)*(1-c)));
}
bool Finite(Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
TableRay CursorRay(const Camera3D& camera, Vector2 cursor) {
    return TableView::ScreenRay(camera, cursor, TableLayout::ImageWidth, TableLayout::ImageHeight);
}
} // namespace

float CardPresentation::Lift(int id) const noexcept {
    return id >= 0 && id < static_cast<int>(lift_.size()) ? lift_[static_cast<std::size_t>(id)] : 0;
}

Vector3 CardPresentation::PointOnCard(const CardPose& pose, float u, float v) noexcept {
    return Add(pose.center, Add(Scale(pose.right, (2*u-1)*pose.halfWidth),
                                Scale(pose.down, (2*v-1)*pose.halfHeight)));
}

CardPose CardPresentation::Pose(const TableCard& card, const Camera3D& camera, Vector2 cursor) const {
    const auto r = card.rect;
    const auto ground = TableLayout::OnPlane(r.x+r.width*0.5f, r.y+r.height*0.5f, card.elevation);
    const float amount = Lift(card.id);
    CardPose pose{{ground.x, ground.y + 0.35f*amount, ground.z}, {1,0,0}, {0,0,1}, {0,1,0},
                  r.width * (TableLayout::PlaneHalfWidth / TableLayout::PlanWidth) * card.flip,
                  r.height * ((TableLayout::FarDepth-TableLayout::NearDepth) / TableLayout::PlanHeight) * 0.5f};
    // The shortest rotation from the table normal to the view direction gives
    // a full local billboard, including yaw for cards away from the center.
    Vector3 desired = Normalize(Sub(camera.position, pose.center));
    if (card.zone == TableCardZone::DRAGGED && card.id == dragId_)
        desired = Scale(Normalize(CursorRay(camera, cursor).direction), -1);
    const Vector3 cross = Cross({0,1,0}, desired);
    const float axisLength = std::sqrt(Dot(cross,cross));
    if (axisLength > 1e-7f) {
        const Vector3 axis = Scale(cross, 1/axisLength);
        const float angle = std::atan2(axisLength, std::clamp(desired.y,-1.0f,1.0f))*amount;
        pose.right = Rotate(pose.right,axis,angle);
        pose.down = Rotate(pose.down,axis,angle);
        pose.normal = Normalize(Cross(pose.down,pose.right));
    }
    if (card.zone == TableCardZone::DRAGGED && card.id == dragId_) {
        // Solve the grabbed point on a horizontal plane at its actual height.
        // This avoids a feedback loop when the billboard rotates under the ray.
        const Vector3 offset = Sub(PointOnCard(pose, grabU_, grabV_), pose.center);
        Vector3 hit{};
        if (TableView::IntersectPlane(CursorRay(camera, cursor), pose.center.y + offset.y, hit)) {
            pose.center.x = hit.x - offset.x;
            pose.center.z = hit.z - offset.z;
        }
    }
    return pose;
}

bool CardPresentation::IntersectCard(const TableRay& ray, const CardPose& pose, CardHit& hit) noexcept {
    if (!Finite(ray.origin) || !Finite(ray.direction) || !Finite(pose.center) ||
        pose.halfWidth <= 0 || pose.halfHeight <= 0) return false;
    const float denominator = Dot(ray.direction, pose.normal);
    if (!std::isfinite(denominator) || std::abs(denominator) < 1e-6f) return false;
    const float t = Dot(Sub(pose.center, ray.origin), pose.normal)/denominator;
    if (!std::isfinite(t) || t < 0) return false;
    const Vector3 local = Sub(Add(ray.origin, Scale(ray.direction,t)),pose.center);
    const float u = 0.5f + Dot(local,pose.right)/(2*pose.halfWidth);
    const float v = 0.5f + Dot(local,pose.down)/(2*pose.halfHeight);
    if (!std::isfinite(u) || !std::isfinite(v) || u < 0 || u > 1 || v < 0 || v > 1) return false;
    hit.u = u; hit.v = v; hit.distance = t;
    return true;
}

std::optional<CardHit> CardPresentation::Pick(const TableCardList& cards, const Camera3D& camera, Vector2 cursor) const {
    const TableRay ray = CursorRay(camera, cursor);
    std::optional<CardHit> nearest;
    for (const TableCard& card : cards) {
        CardHit hit{};
        if (IntersectCard(ray, Pose(card,camera,cursor), hit) && (!nearest || hit.distance < nearest->distance)) {
            hit.card = card; nearest = hit;
        }
    }
    // Keep the original hand hit area active while a card rises away from it.
    // The actual rendered card always wins, and a black back can occlude it.
    if (!nearest && inspected_ >= 0) for (const TableCard& card : cards) {
        if (card.id != inspected_ || card.zone != TableCardZone::PLAYER_HAND) continue;
        CardPresentation flat;
        CardHit hit{};
        if (IntersectCard(ray, flat.Pose(card,camera,cursor), hit)) { hit.card = card; return hit; }
    }
    return nearest;
}

void CardPresentation::Update(const TableCardList& cards, const Camera3D& camera, Vector2 cursor, float dt) {
    const auto hit = Pick(cards,camera,cursor);
    inspected_ = hit && hit->card.id >= 0 ? hit->card.id : -1;
    int lifted = dragId_;
    if (lifted < 0 && hit && hit->card.id >= 0 && hit->card.zone == TableCardZone::PLAYER_HAND)
        lifted = hit->card.id;
    const float blend = std::isfinite(dt) && dt > 0 ? -std::expm1(-25.0f*dt) : 0;
    for (int i = 0; i < static_cast<int>(lift_.size()); ++i)
        lift_[static_cast<std::size_t>(i)] += ((i == lifted ? 1.0f : 0.0f) - lift_[static_cast<std::size_t>(i)]) * blend;
}

void CardPresentation::BeginDrag(const CardHit& hit) noexcept {
    dragId_ = hit.card.id; grabU_ = hit.u; grabV_ = hit.v;
}
void CardPresentation::EndDrag() noexcept { dragId_ = -1; }

std::optional<TableLayout::Rect> CardPresentation::DragVisualRect(const TableCardList& cards,
                                                                 const Camera3D& camera, Vector2 cursor) const {
    for (const auto& card : cards) if (card.zone == TableCardZone::DRAGGED && card.id == dragId_) {
        const auto pose = Pose(card,camera,cursor);
        const auto center = TableLayout::FromPlane({pose.center.x, 0, pose.center.z});
        return TableLayout::Rect{center.x-card.rect.width*0.5f, center.y-card.rect.height*0.5f,
                                 card.rect.width, card.rect.height};
    }
    return std::nullopt;
}
