// camera.h - declares camera room metadata and scroll controller state.
// Owns: viewport position, bounds, room locks, deadzones, and shake state.

#pragma once

#include "app/constants.h"

// ============================================================================
// camera.h — 2D camera with the measured MMX1 behavior
//
// Features:
//   Rigid horizontal follow: camera_x = target anchor - 128 EVERY frame
//     (oracle-measured law, 900-frame proof — BOARD U10 findings; the old
//     eased follow + directional lead measured as a 19-43px gate residual)
//   Vertical eased follow (real vertical law unmeasured; rooms pin locks)
//   Room boundaries: camera is confined within the current room section
//   Screen shake: offset by random amounts for impact effects
//   Boss lock-in: camera locks to a specific area during boss fights
//
// The camera operates in world-space pixels. The gameplay scene subtracts
// camera position from entity positions when rendering to get screen coords.
//
// Design note: MMX uses hard room boundaries. When the player crosses a
// room boundary, the camera slides smoothly to the next room. During the
// transition, the player's input is locked. This is handled by the gameplay
// scene, not the camera itself — the camera just knows its target and bounds.
// ============================================================================

namespace mmx {

struct CameraRoom {
    float x, y, w, h; // Bounding rectangle in world pixels
};

class Camera {
public:
    // Set the world bounds (full stage size). Camera never shows outside this.
    void setWorldBounds(float w, float h);

    // Set the current room boundaries. Camera is confined to this area.
    // If room is smaller than the screen, camera centers on the room.
    void setRoom(float rx, float ry, float rw, float rh);

    // Clear room bounds — camera can move freely within world bounds.
    void clearRoom();

    // U39 camera_sections vertical modes. These affect only camera_y; the
    // measured MMX1 horizontal law remains rigid anchor-128.
    void setVerticalLock(float y);
    void setVerticalFollowDeadzone(float screenTop, float screenBottom,
                                   float lagPxPerFrame);
    void clearVerticalMode();

    // Lock camera to a fixed position (boss fight).
    void lockTo(float x, float y);
    void unlock();

    // Suppress world/room bounds clamping. Used during player death so the
    // camera can follow X into the pit-death zone (below world bounds) and
    // keep the orb burst on-screen. Reset to false after respawn.
    void setClampSuppressed(bool s) { clampSuppressed_ = s; }
    bool clampSuppressed() const    { return clampSuppressed_; }

    // CP-B1C-T1-M2: the boss-entry cinematic drives base X/Y exactly (the
    // measured 2 px/tick pan into the arena lock) while clamping is
    // suppressed; vertical holds the pre-pan clamped value. Interpolation
    // prev still comes from the scene's savePosition() cadence.
    void setBaseXY(float x, float y) { x_ = x; y_ = y; }

    // Update camera position to follow target.
    // targetX/Y: world position of what to follow (usually player center)
    // facingRight: player's facing direction (for directional lead)
    void update(float targetX, float targetY, bool facingRight);

    // Apply screen shake. Intensity decreases over duration.
    void shake(float intensity, int durationFrames);

    // Get final camera position (includes shake offset)
    float x() const { return x_ + shakeOffsetX_; }
    float y() const { return y_ + shakeOffsetY_; }

    // Get camera position without shake (for interpolation base)
    float baseX() const { return x_; }
    float baseY() const { return y_; }

    // Previous frame position for render interpolation
    float prevX() const { return prevX_ + prevShakeX_; }
    float prevY() const { return prevY_ + prevShakeY_; }

    void savePosition() {
        prevX_ = x_;
        prevY_ = y_;
        prevShakeX_ = shakeOffsetX_;
        prevShakeY_ = shakeOffsetY_;
    }

    // Snap camera instantly (no smoothing) — used for initial placement
    void snapTo(float targetX, float targetY);

    // Camera config. Horizontal follow is RIGID (anchor - 128, the measured
    // MMX1 law — see camera.cpp); followSpeed only eases the vertical axis
    // and boss locks.
    float followSpeed = 0.12f;   // Lerp factor (0=frozen, 1=instant)

private:
    float x_ = 0, y_ = 0;
    float prevX_ = 0, prevY_ = 0;

    // World limits
    float worldW_ = 0, worldH_ = 0;

    // Room bounds (if active)
    bool hasRoom_ = false;
    CameraRoom room_ = {};

    // Boss lock
    bool locked_ = false;
    float lockX_ = 0, lockY_ = 0;

    // Screen shake
    float shakeIntensity_ = 0;
    int shakeDuration_ = 0;
    float shakeOffsetX_ = 0, shakeOffsetY_ = 0;
    float prevShakeX_ = 0, prevShakeY_ = 0;

    // Death-state override — disables world/room bounds clamping so orbs
    // stay visible even when X is outside the normal playfield.
    bool clampSuppressed_ = false;

    enum class VerticalMode {
        EasedFollow,
        Lock,
        DeadzoneFollow,
    };
    VerticalMode verticalMode_ = VerticalMode::EasedFollow;
    float verticalLockY_ = 0.0f;
    float deadzoneTop_ = 0.0f;
    float deadzoneBottom_ = 0.0f;
    float deadzoneLag_ = 0.0f;

    void clampToBounds();
};

} // namespace mmx
