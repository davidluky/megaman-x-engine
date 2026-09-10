// entity.h - declares the base entity shape and axis-aligned bounds helper.
// Owns: common position, size, active/visible flags, and update/render API.

#pragma once

#include "systems/raylib_resource.h"

// ============================================================================
// entity.h — Base class for all game objects
//
// Every object in the game world (player, enemies, projectiles, pickups,
// effects) inherits from Entity. It provides the minimal shared state:
// position, hitbox, and whether the entity is active.
//
// The hitbox is an axis-aligned bounding box (AABB) defined as an offset
// from the entity's position plus a size. This allows the collision box
// to be smaller than the visual sprite — critical for fair gameplay.
//
// Example: A 32x32 sprite might have a 14x24 hitbox centered within it.
//   position = top-left of the entity in world space
//   hitboxOffset = (9, 4) — shifts the collision box right and down
//   hitboxSize = (14, 24) — the actual collision rectangle
// ============================================================================

namespace mmx {

struct AABB {
    float x, y, w, h;

    float left()   const { return x; }
    float right()  const { return x + w; }
    float top()    const { return y; }
    float bottom() const { return y + h; }

    bool overlaps(const AABB& other) const {
        return x < other.x + other.w && x + w > other.x &&
               y < other.y + other.h && y + h > other.y;
    }
};

class Entity {
public:
    virtual ~Entity() = default;

    virtual void handleInput() {}
    virtual void update(float /*dt*/) {}
    virtual void render(float /*alpha*/) {}

    // World-space hitbox (position + offset)
    AABB getHitbox() const {
        return {
            position.x + hitboxOffset.x,
            position.y + hitboxOffset.y,
            hitboxSize.x,
            hitboxSize.y
        };
    }

    // Position in world space (top-left of the entity's logical area)
    Vector2 position = {0, 0};
    Vector2 prevPosition = {0, 0}; // For render interpolation

    // Hitbox definition (relative to position)
    Vector2 hitboxOffset = {0, 0};
    Vector2 hitboxSize = {16, 16};

    const TextureResource* spriteSheetResource() const { return spriteSheetResource_; }
    bool hasSpriteSheet() const {
        return spriteSheetResource_ && spriteSheetResource_->valid();
    }

    bool active = true;
    bool facingRight = true;

protected:
    void setSpriteSheetResource(const TextureResource* resource) {
        spriteSheetResource_ = resource;
    }

private:
    const TextureResource* spriteSheetResource_ = nullptr; // Borrowed render resource.
};

} // namespace mmx
