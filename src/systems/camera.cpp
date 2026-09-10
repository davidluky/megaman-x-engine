// camera.cpp - updates camera tracking, bounds, rooms, locks, and shake.
// Owns: camera target smoothing, world clamps, and vertical section rules.

#include "systems/camera.h"
#include "data/settings.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace mmx {

void Camera::setWorldBounds(float w, float h) {
    worldW_ = w;
    worldH_ = h;
}

void Camera::setRoom(float rx, float ry, float rw, float rh) {
    hasRoom_ = true;
    room_ = {rx, ry, rw, rh};
}

void Camera::clearRoom() {
    hasRoom_ = false;
}

void Camera::setVerticalLock(float y) {
    verticalMode_ = VerticalMode::Lock;
    verticalLockY_ = y;
}

void Camera::setVerticalFollowDeadzone(float screenTop, float screenBottom,
                                       float lagPxPerFrame) {
    verticalMode_ = VerticalMode::DeadzoneFollow;
    deadzoneTop_ = std::min(screenTop, screenBottom);
    deadzoneBottom_ = std::max(screenTop, screenBottom);
    deadzoneLag_ = std::max(0.0f, lagPxPerFrame);
}

void Camera::clearVerticalMode() {
    verticalMode_ = VerticalMode::EasedFollow;
}

void Camera::lockTo(float x, float y) {
    locked_ = true;
    lockX_ = x;
    lockY_ = y;
}

void Camera::unlock() {
    locked_ = false;
}

void Camera::snapTo(float targetX, float targetY) {
    x_ = targetX - INTERNAL_WIDTH / 2.0f;
    if (verticalMode_ == VerticalMode::Lock) {
        y_ = verticalLockY_;
    } else if (verticalMode_ == VerticalMode::DeadzoneFollow) {
        y_ = targetY - (deadzoneTop_ + deadzoneBottom_) * 0.5f;
    } else {
        y_ = targetY - INTERNAL_HEIGHT / 2.0f;
    }
    clampToBounds();
    prevX_ = x_;
    prevY_ = y_;
}

static float approach(float current, float target, float maxStep) {
    if (maxStep <= 0.0f) return target;
    const float delta = target - current;
    if (std::fabs(delta) <= maxStep) return target;
    return current + (delta > 0.0f ? maxStep : -maxStep);
}

void Camera::update(float targetX, float targetY, bool facingRight) {
    // Screen shake tick
    if (shakeDuration_ > 0) {
        shakeDuration_--;
        // Random offset that decays with remaining duration
        float factor = static_cast<float>(shakeDuration_) / 10.0f;
        if (factor > 1.0f) factor = 1.0f;
        float intensity = shakeIntensity_ * factor;
        shakeOffsetX_ = (static_cast<float>(rand() % 200 - 100) / 100.0f) * intensity;
        shakeOffsetY_ = (static_cast<float>(rand() % 200 - 100) / 100.0f) * intensity;
    } else {
        shakeOffsetX_ = 0;
        shakeOffsetY_ = 0;
    }

    if (locked_) {
        x_ += (lockX_ - x_) * followSpeed;
        y_ += (lockY_ - y_) * followSpeed;
        clampToBounds();
        return;
    }

    // MMX1 horizontal law (oracle-measured, 900-frame walk proof — BOARD U10,
    // findings/2026-06-11-cp-grid-vs-ram-frame.md): camera_x = anchor_x - 128
    // RIGID on every frame. No directional lead, no smoothing — the real
    // game's camera is a hardware-scroll glued to X's anchor, and any easing
    // shows up as a uniform shift in the pixel gate.
    (void)facingRight;
    x_ = targetX - INTERNAL_WIDTH / 2.0f;

    if (verticalMode_ == VerticalMode::Lock) {
        y_ = verticalLockY_;
    } else if (verticalMode_ == VerticalMode::DeadzoneFollow) {
        const float screenY = targetY - y_;
        float desiredY = y_;
        if (screenY < deadzoneTop_) {
            desiredY = targetY - deadzoneTop_;
        } else if (screenY > deadzoneBottom_) {
            desiredY = targetY - deadzoneBottom_;
        }
        y_ = approach(y_, desiredY, deadzoneLag_);
    } else {
        // Vertical keeps the eased follow for now: real MMX1 vertical scrolling
        // has its own platform-snap law (unmeasured); measured rooms pin the
        // value wherever the oracle proved a lock.
        float desiredY = targetY - INTERNAL_HEIGHT / 2.0f;
        y_ += (desiredY - y_) * followSpeed;
    }

    clampToBounds();
}

void Camera::shake(float intensity, int durationFrames) {
    if (const char* disabled = std::getenv("MMX_DISABLE_SCREEN_SHAKE")) {
        if (*disabled != '\0' && *disabled != '0') return;
    }
    intensity *= Settings::screenShake;
    if (intensity <= 0.0f) return;
    if (intensity > shakeIntensity_ || shakeDuration_ <= 0) {
        shakeIntensity_ = intensity;
        shakeDuration_ = durationFrames;
    }
}

void Camera::clampToBounds() {
    // Death orbs are drawn outside normal world bounds (e.g. below a pit);
    // the gameplay scene asks for suppression while the player is in the
    // Die state and re-enables it on respawn.
    if (clampSuppressed_) return;

    // Room bounds take priority over world bounds
    if (hasRoom_) {
        float minX = room_.x;
        float minY = room_.y;
        float maxX = room_.x + room_.w - INTERNAL_WIDTH;
        float maxY = room_.y + room_.h - INTERNAL_HEIGHT;

        // If room is smaller than screen, center camera on room
        if (maxX < minX) {
            x_ = room_.x + room_.w / 2.0f - INTERNAL_WIDTH / 2.0f;
        } else {
            x_ = std::clamp(x_, minX, maxX);
        }
        if (maxY < minY) {
            y_ = room_.y + room_.h / 2.0f - INTERNAL_HEIGHT / 2.0f;
        } else {
            y_ = std::clamp(y_, minY, maxY);
        }
    }

    // Always clamp to world bounds
    float worldMaxX = worldW_ - INTERNAL_WIDTH;
    float worldMaxY = worldH_ - INTERNAL_HEIGHT;
    x_ = std::clamp(x_, 0.0f, std::max(0.0f, worldMaxX));
    y_ = std::clamp(y_, 0.0f, std::max(0.0f, worldMaxY));
}

} // namespace mmx
