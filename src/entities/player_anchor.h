// player_anchor.h - explicit source/RAM and runtime hitbox anchors.
//
// WP-C pins the right-facing source player anchor at Player::position +32.
// Runtime camera follows the configured hitbox center instead. The current X
// definition makes that +33, so the exact one-pixel delta applies to the
// right-facing Chill Penguin approach only. The mirrored left-facing source
// anchor is position +38 in the 70px cell. Camera keeps the direct
// hitbox-center expression audited historically by U214.

#pragma once

namespace mmx::player_anchor {

inline constexpr float kRightFacingSourceRamOffsetX = 32.0f;

constexpr float sourceRamAnchorX(float playerPositionX, float spriteWidth,
                                 bool facingRight) noexcept {
    const float cellOffset =
        facingRight ? kRightFacingSourceRamOffsetX
                    : spriteWidth - kRightFacingSourceRamOffsetX;
    return playerPositionX + cellOffset;
}

} // namespace mmx::player_anchor
