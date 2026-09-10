// Resolution against the existing Axe Max launcher/log envelope.
// The scene adapter owns stage/enemy selection; this header keeps the
// movement contract headless and directly testable.

#pragma once

#include "entities/entity.h"

#include <optional>

namespace mmx::axemax_stack_collision {

struct HorizontalResolution {
    float playerPositionX = 0.0f;
    bool touchingWallLeft = false;
    bool touchingWallRight = false;
};

struct VerticalResolution {
    float playerPositionY;
    bool onGround;
    bool onCeiling;
};

inline std::optional<VerticalResolution> resolveVertical(
    const AABB& previousBox, const AABB& playerBox, const AABB& blocker,
    float playerHitboxOffsetY, float velocityY) {
    if (blocker.w <= 0 || blocker.h <= 0 ||
        playerBox.right() <= blocker.left() || playerBox.left() >= blocker.right()) {
        return std::nullopt;
    }
    // R246: choose the crossed face at the time of contact. Requiring
    // previous overlap rejects valid corner landings; testing only the end
    // position incorrectly lifts a late side entry onto the magazine.
    const auto supportedAt = [&](float t) {
        const float x = previousBox.x + (playerBox.x - previousBox.x) * t;
        return x + playerBox.w > blocker.left() && x < blocker.right();
    };
    if (velocityY >= 0 && previousBox.bottom() <= blocker.top() &&
        playerBox.bottom() >= blocker.top()) {
        const float dy = playerBox.bottom() - previousBox.bottom();
        if (!supportedAt(dy > 0 ? (blocker.top() - previousBox.bottom()) / dy : 1.0f)) {
            return std::nullopt;
        }
        return VerticalResolution{
            blocker.top() - playerHitboxOffsetY - playerBox.h, true, false};
    }
    if (velocityY < 0 && previousBox.top() >= blocker.bottom() &&
        playerBox.top() <= blocker.bottom()) {
        const float dy = playerBox.top() - previousBox.top();
        if (!supportedAt(dy < 0 ? (blocker.bottom() - previousBox.top()) / dy : 1.0f)) {
            return std::nullopt;
        }
        return VerticalResolution{blocker.bottom() - playerHitboxOffsetY, false, true};
    }
    return std::nullopt;
}

inline std::optional<HorizontalResolution> resolveHorizontal(
    const AABB& playerBox, const AABB& blocker, float playerHitboxOffsetX) {
    if (blocker.w <= 0.0f || blocker.h <= 0.0f ||
        !playerBox.overlaps(blocker)) {
        return std::nullopt;
    }

    const float playerCenter = playerBox.x + playerBox.w * 0.5f;
    const float blockerCenter = blocker.x + blocker.w * 0.5f;
    if (playerCenter <= blockerCenter) {
        return HorizontalResolution{
            blocker.left() - playerHitboxOffsetX - playerBox.w,
            false,
            true,
        };
    }
    return HorizontalResolution{
        blocker.right() - playerHitboxOffsetX,
        true,
        false,
    };
}

} // namespace mmx::axemax_stack_collision
