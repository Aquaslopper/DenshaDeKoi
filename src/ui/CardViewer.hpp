#pragma once

#include "ui/UiViewport.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace Ui {

// One frame of gallery input. The viewer needs the same pointer, wheel and key
// state as the gallery grid, so both share this struct instead of a long list.
struct GalleryInput {
    TableLayout::Point2 pointer{};
    bool moved = false;
    bool pressed = false; // Left button went down this frame.
    bool down = false;    // Left button is held.
    bool rightPressed = false;
    float wheel = 0.0f;
    int vertical = 0;
    int horizontal = 0;
    bool enter = false;
    bool escape = false;
    int antiAliasing = 2;
};

// The gallery's selected card as a real 3D object. The viewer owns only its
// render target; the card image stays with CardGallery, which reloads it per
// month, so no card texture is ever loaded twice.
class CardViewer {
public:
    CardViewer() = default;
    ~CardViewer() {
        if (target_.id != 0) UnloadRenderTexture(target_);
    }

    CardViewer(const CardViewer&) = delete;
    CardViewer& operator=(const CardViewer&) = delete;

    [[nodiscard]] bool IsOpen() const { return open_; }
    [[nodiscard]] float Zoom() const { return zoom_; }
    [[nodiscard]] bool BackHovered() const { return backHovered_; }

    // The anti-aliasing level is taken at open time so the first drawn frame
    // already renders into the correct supersampled target.
    void Open(int antiAliasing) {
        open_ = true;
        samples_ = std::clamp(antiAliasing, 2, 4);
        opening_ = 0.0f;
        yaw_ = 0.0f;
        pitch_ = 0.0f;
        zoom_ = 1.0f;
        zoomTarget_ = 1.0f;
        dragging_ = false;
        inertia_ = false;
        yawVelocity_ = 0.0f;
        pitchVelocity_ = 0.0f;
        allowDrag_ = false; // The click that opened the viewer must not turn it.
        sinceMove_ = 1.0f;
        previous_ = {};
        backHovered_ = false;
    }

    void Close() {
        open_ = false;
        dragging_ = false;
        inertia_ = false;
        yawVelocity_ = 0.0f;
        pitchVelocity_ = 0.0f;
        backHovered_ = false;
    }

    void Update(const GalleryInput& input) {
        if (!open_) return;

        const float dt = std::clamp(GetFrameTime(), 0.0f, 0.1f);
        opening_ = std::min(PopOutDuration, opening_ + dt);
        sinceMove_ += dt;
        backHovered_ = Contains(BackButton, input.pointer);
        samples_ = std::clamp(input.antiAliasing, 2, 4);

        if (input.escape || input.rightPressed ||
            (input.pressed && backHovered_)) {
            Close();
            return;
        }

        if (input.wheel != 0.0f) {
            zoomTarget_ = std::clamp(zoomTarget_ * std::pow(ZoomStep, input.wheel),
                                     MinZoom, MaxZoom);
        }
        const float blend = 1.0f - std::exp(-ZoomSpeed * dt);
        zoom_ += (zoomTarget_ - zoom_) * blend;

        // Turning: drag right brings the left edge toward the viewer, drag down
        // brings the top edge toward the viewer.
        if (input.pressed && allowDrag_) {
            dragging_ = true;
            inertia_ = false;
            previous_ = input.pointer;
            sinceMove_ = 0.0f;
            yawVelocity_ = 0.0f;
            pitchVelocity_ = 0.0f;
        }

        if (dragging_) {
            if (!input.down) {
                dragging_ = false;
                inertia_ = sinceMove_ <= InertiaWindow;
                if (!inertia_) {
                    yawVelocity_ = 0.0f;
                    pitchVelocity_ = 0.0f;
                }
            } else {
                const float dx = input.pointer.x - previous_.x;
                const float dy = input.pointer.y - previous_.y;
                previous_ = input.pointer;
                if (dx != 0.0f || dy != 0.0f) {
                    yaw_ += dx * Sensitivity;
                    const float wanted = pitch_ + dy * Sensitivity;
                    pitch_ = std::clamp(wanted, -PitchLimit, PitchLimit);
                    sinceMove_ = 0.0f;
                    if (dt > 0.0f) {
                        yawVelocity_ = dx * Sensitivity / dt;
                        pitchVelocity_ = pitch_ != wanted ? 0.0f : dy * Sensitivity / dt;
                    }
                }
            }
        } else if (inertia_) {
            yaw_ += yawVelocity_ * dt;
            const float wanted = pitch_ + pitchVelocity_ * dt;
            pitch_ = std::clamp(wanted, -PitchLimit, PitchLimit);
            if (pitch_ != wanted) pitchVelocity_ = 0.0f;

            const float decay = std::exp(-InertiaDecay * dt);
            yawVelocity_ *= decay;
            pitchVelocity_ *= decay;
            if (std::sqrt(yawVelocity_ * yawVelocity_ + pitchVelocity_ * pitchVelocity_) < InertiaStop) {
                inertia_ = false;
                yawVelocity_ = 0.0f;
                pitchVelocity_ = 0.0f;
            }
        }

        allowDrag_ = true; // From the frame after the opening click onward.
    }

    // Draws the faded backdrop and the 3D card only; the gallery adds its
    // shared strip and footer on top, so the two screens stay in one family.
    void Draw(const Viewport& viewport, Texture2D texture) const {
        if (!open_) return;

        const float progress = Eased();
        DrawRectangleRec(viewport.box,
                         Color{2, 8, 26, static_cast<unsigned char>(std::lround(225.0f * progress))});
        if (texture.id == 0) return;

        EnsureTarget(samples_);
        if (target_.id == 0) return;

        RenderCard(texture);

        const float scale = PopOutStartScale + (1.0f - PopOutStartScale) * progress;
        const float cx = PopOutStartX + (CentreX - PopOutStartX) * progress;
        const float cy = PopOutStartY + (CentreY - PopOutStartY) * progress;
        const TableLayout::Rect destination{
            cx - CentreX * scale, cy - CentreY * scale,
            static_cast<float>(TableLayout::ImageWidth) * scale,
            static_cast<float>(TableLayout::ImageHeight) * scale};
        // Render textures are stored upside down, hence the negative height.
        const Rectangle source{0, 0, static_cast<float>(target_.texture.width),
                               -static_cast<float>(target_.texture.height)};
        DrawTexturePro(target_.texture, source, viewport.Rec(destination), {0, 0}, 0.0f, WHITE);
    }

private:
    inline static constexpr float PopOutDuration = 0.18f;
    inline static constexpr float PopOutStartScale = 0.869f; // 504 / 580
    inline static constexpr float PopOutStartX = 547.5f;     // Card centre in the case.
    inline static constexpr float PopOutStartY = 400.0f;
    inline static constexpr float CentreX = static_cast<float>(TableLayout::ImageWidth) * 0.5f;
    inline static constexpr float CentreY = static_cast<float>(TableLayout::ImageHeight) * 0.5f;
    inline static constexpr float MinZoom = 0.6f;
    inline static constexpr float MaxZoom = 3.0f;
    inline static constexpr float ZoomStep = 1.12f;
    inline static constexpr float ZoomSpeed = 12.0f;
    inline static constexpr float Sensitivity = 0.4f; // Degrees per design pixel.
    inline static constexpr float PitchLimit = 70.0f;
    inline static constexpr float InertiaWindow = 0.08f;
    inline static constexpr float InertiaDecay = 6.0f;
    inline static constexpr float InertiaStop = 2.0f; // Degrees per second.
    inline static constexpr float CameraDistance = 2.47f;
    inline static constexpr float FovY = 30.0f;
    inline static constexpr float HalfWidth = 602.0f / 928.0f * 0.5f;
    inline static constexpr float HalfHeight = 0.5f;
    inline static constexpr float HalfThickness = 0.006f;
    inline static constexpr int MaxTargetSide = 4096;
    inline static constexpr TableLayout::Rect BackButton{570, 682, 220, 48};
    inline static constexpr Color EdgeCream{214, 209, 194, 255};

    static bool Contains(TableLayout::Rect rect, TableLayout::Point2 point) {
        return point.x >= rect.x && point.x < rect.x + rect.width &&
               point.y >= rect.y && point.y < rect.y + rect.height;
    }

    float Eased() const {
        const float t = std::clamp(opening_ / PopOutDuration, 0.0f, 1.0f);
        const float inverse = 1.0f - t;
        return 1.0f - inverse * inverse * inverse;
    }

    // Supersampled target for the whole 1360x768 UI box. Its size depends only
    // on the anti-aliasing setting, so it is rebuilt when that changes.
    void EnsureTarget(int samples) const {
        int factor = std::clamp(samples, 2, 4);
        constexpr int longest = TableLayout::ImageWidth > TableLayout::ImageHeight
                                    ? TableLayout::ImageWidth : TableLayout::ImageHeight;
        while (factor > 1 && longest * factor > MaxTargetSide) --factor;
        const int width = TableLayout::ImageWidth * factor;
        const int height = TableLayout::ImageHeight * factor;
        if (target_.id != 0 &&
            target_.texture.width == width && target_.texture.height == height) return;

        if (target_.id != 0) UnloadRenderTexture(target_);
        RenderTexture2D created = LoadRenderTexture(width, height);
        if (created.id == 0 || created.texture.id == 0) {
            if (created.id != 0) UnloadRenderTexture(created);
            return;
        }
        SetTextureFilter(created.texture, TEXTURE_FILTER_BILINEAR);
        target_ = created;
    }

    void RenderCard(Texture2D texture) const {
        BeginTextureMode(target_);
        ClearBackground(BLANK);
        const Camera3D camera{{0.0f, 0.0f, CameraDistance}, {0.0f, 0.0f, 0.0f},
                              {0.0f, 1.0f, 0.0f}, FovY, CAMERA_PERSPECTIVE};
        BeginMode3D(camera);
        // rlgl submits the vertices when the batch is flushed, which happens at
        // EndMode3D/EndTextureMode. Culling therefore has to stay off until then,
        // not just around the vertex loops, and is restored right afterwards.
        rlDisableBackfaceCulling();
        rlPushMatrix();
        rlLoadIdentity(); // rlPushMatrix keeps the previous transform; start clean.
        rlRotatef(pitch_, 1.0f, 0.0f, 0.0f);
        rlRotatef(yaw_, 0.0f, 1.0f, 0.0f);
        rlScalef(zoom_, zoom_, zoom_);
        DrawCardModel(texture);
        rlPopMatrix();
        EndMode3D();
        EndTextureMode();
        rlEnableBackfaceCulling();
    }

    static void Vertex(float x, float y, float z, float u, float v) {
        rlTexCoord2f(u, v);
        rlVertex3f(x, y, z);
    }

    static void DrawCardModel(Texture2D texture) {
        constexpr float hw = HalfWidth;
        constexpr float hh = HalfHeight;
        constexpr float ht = HalfThickness;

        // Front (+Z): the artwork, upright and unmirrored, full 0..1 texture.
        // Wound counter-clockwise when seen from the camera.
        rlSetTexture(texture.id);
        rlBegin(RL_QUADS);
        rlColor4ub(255, 255, 255, 255);
        Vertex(-hw, -hh, ht, 0.0f, 1.0f);
        Vertex(hw, -hh, ht, 1.0f, 1.0f);
        Vertex(hw, hh, ht, 1.0f, 0.0f);
        Vertex(-hw, hh, ht, 0.0f, 0.0f);
        rlEnd();

        // Back (-Z): solid black, like the table's card backs.
        rlSetTexture(rlGetTextureIdDefault());
        rlBegin(RL_QUADS);
        rlColor4ub(0, 0, 0, 255);
        Vertex(hw, hh, -ht, 0.0f, 0.0f);
        Vertex(hw, -hh, -ht, 0.0f, 1.0f);
        Vertex(-hw, -hh, -ht, 1.0f, 1.0f);
        Vertex(-hw, hh, -ht, 1.0f, 0.0f);
        rlEnd();

        // Four cream edge strips keep the card edge visible on the dark backdrop.
        // They use the default white texture so the cream is not tinted by artwork.
        rlSetTexture(rlGetTextureIdDefault());
        rlBegin(RL_QUADS);
        rlColor4ub(EdgeCream.r, EdgeCream.g, EdgeCream.b, EdgeCream.a);
        Vertex(hw, hh, ht, 0.0f, 0.0f);
        Vertex(hw, hh, -ht, 0.0f, 0.0f);
        Vertex(hw, -hh, -ht, 0.0f, 0.0f);
        Vertex(hw, -hh, ht, 0.0f, 0.0f);

        Vertex(-hw, hh, -ht, 0.0f, 0.0f);
        Vertex(-hw, hh, ht, 0.0f, 0.0f);
        Vertex(-hw, -hh, ht, 0.0f, 0.0f);
        Vertex(-hw, -hh, -ht, 0.0f, 0.0f);

        Vertex(-hw, hh, -ht, 0.0f, 0.0f);
        Vertex(hw, hh, -ht, 0.0f, 0.0f);
        Vertex(hw, hh, ht, 0.0f, 0.0f);
        Vertex(-hw, hh, ht, 0.0f, 0.0f);

        Vertex(-hw, -hh, ht, 0.0f, 0.0f);
        Vertex(hw, -hh, ht, 0.0f, 0.0f);
        Vertex(hw, -hh, -ht, 0.0f, 0.0f);
        Vertex(-hw, -hh, -ht, 0.0f, 0.0f);
        rlEnd();
        rlSetTexture(0);
    }

    mutable RenderTexture2D target_{};
    bool open_ = false;
    bool dragging_ = false;
    bool inertia_ = false;
    bool allowDrag_ = false;
    bool backHovered_ = false;
    float opening_ = 0.0f;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float yawVelocity_ = 0.0f;
    float pitchVelocity_ = 0.0f;
    float sinceMove_ = 0.0f;
    float zoom_ = 1.0f;
    float zoomTarget_ = 1.0f;
    TableLayout::Point2 previous_{};
    int samples_ = 2;
};

} // namespace Ui
