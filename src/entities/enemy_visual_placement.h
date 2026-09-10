// enemy_visual_placement.h - pure sprite-body placement math.
//
// The offset belongs only to the textured enemy body.  Physics, hitboxes,
// authored spawn coordinates, terrain, and any separately rendered child
// composition (for example Axe Max's launcher/log stack) do not consume it.

#pragma once

namespace mmx {

inline constexpr float enemyBodyLeftX(float hitboxCenterX,
                                      int frameWidth) noexcept {
    return hitboxCenterX - static_cast<float>(frameWidth) * 0.5f;
}

inline constexpr bool enemyBodyShouldFlipX(bool facingRight,
                                           bool bodyMirrorsWithFacing,
                                           bool bodySourceFacesRight = true) noexcept {
    return bodyMirrorsWithFacing && (facingRight != bodySourceFacesRight);
}

inline constexpr float enemyBodySourceWidth(int frameWidth,
                                            bool facingRight,
                                            bool bodyMirrorsWithFacing,
                                            bool bodySourceFacesRight = true) noexcept {
    return enemyBodyShouldFlipX(
        facingRight, bodyMirrorsWithFacing, bodySourceFacesRight)
        ? -static_cast<float>(frameWidth)
        : static_cast<float>(frameWidth);
}

inline constexpr float enemyBodyTopY(float hitboxBottom,
                                     int frameHeight,
                                     float bodyVisualOffsetY) noexcept {
    return hitboxBottom - static_cast<float>(frameHeight) + bodyVisualOffsetY;
}

} // namespace mmx
