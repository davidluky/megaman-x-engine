// gameplay_player_bounds.h - clamps player-only screen bounds without wall flags.
// Boundary: viewport limits are not terrain and must not enable wall slide/jump.

#pragma once

#include "entities/actor.h"

namespace mmx::gameplay_player_bounds {

inline bool clampLeftViewportBound(Actor& actor, float viewportLeft) {
    const AABB hitbox = actor.getHitbox();
    if (hitbox.left() >= viewportLeft) return false;

    actor.position.x += viewportLeft - hitbox.left();
    if (actor.velocity.x < 0.0f) actor.velocity.x = 0.0f;
    return true;
}

} // namespace mmx::gameplay_player_bounds
