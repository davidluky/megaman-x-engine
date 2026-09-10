// actor.h - declares the physics-aware base type for active entities.
// Owns: shared velocity, collision flags, and health-facing actor state.

#pragma once

#include "entity.h"

// ============================================================================
// actor.h — Base class for entities that move and have physics
//
// Actors are entities that obey gravity, have velocity, can collide with the
// tilemap, and have health. Player, enemies, and bosses all inherit from Actor.
//
// Physics units: velocities and gravity are in pixels per physics frame
// (at 60Hz). This matches how SNES games defined movement — directly in
// pixels per frame. Since our physics runs at a fixed 60Hz, the values
// are identical to the original game.
//
// The update method in this class applies gravity and updates position.
// Collision resolution is handled separately after movement, using the
// separate-axis method (move X → resolve X → move Y → resolve Y).
// ============================================================================

namespace mmx {

class Actor : public Entity {
public:
    // Physics state
    Vector2 velocity = {0, 0};

    // Physics constants (loaded from JSON per character)
    float gravity = 0.22f;      // Pixels per frame per frame (downward)
    float maxFallSpeed = 5.0f;  // Terminal velocity (pixels per frame)

    // Ground state — set by collision resolution
    bool onGround = false;
    bool wasOnGround = false; // Previous frame's ground state (for edge detection)
    bool onCeiling = false;
    bool touchingWallLeft = false;
    bool touchingWallRight = false;

    // Health
    int health = 1;
    int maxHealth = 1;
    bool alive = true;

    // Apply gravity to vertical velocity
    void applyGravity() {
        velocity.y += gravity;
        if (velocity.y > maxFallSpeed) {
            velocity.y = maxFallSpeed;
        }
    }

    // Save current position for interpolation before moving
    void savePosition() {
        prevPosition = position;
        wasOnGround = onGround;
    }
};

} // namespace mmx
