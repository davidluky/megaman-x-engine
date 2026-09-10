// pickup.h - declares collectible pickup types and runtime state.
// Owns: pickup identity, collection status, and type-specific metadata.

#pragma once

#include "entity.h"
#include "systems/tilemap.h"
#include "raylib.h"

// ============================================================================
// pickup.h — Collectible items (health, ammo, extra lives)
//
// Pickups are spawned by enemies on death and float briefly before settling.
// The player collects them by touching their hitbox.
//
// Types:
//   SmallHealth — restores 2 HP
//   LargeHealth — restores 8 HP
//   SmallAmmo   — restores weapon energy (future)
//   ExtraLife   — +1 life
// ============================================================================

namespace mmx {

enum class PickupType {
    SmallHealth = 1,
    LargeHealth = 2,
    SmallAmmo   = 3,
    ExtraLife   = 4,
    HeartTank   = 5,
    SubTank     = 6,
    ArmorCapsule = 7  // Dr. Light capsule — grants an armor upgrade
};

class Pickup : public Entity {
public:
    void init(float x, float y, PickupType ptype, const std::string& persistentId = "");

    // Pickups need the tilemap to settle on the ground, so they take a wider
    // update signature than the base Entity. Keep the inherited Entity::update
    // visible so the override set is unambiguous (silences -Woverloaded-virtual).
    using Entity::update;
    void update(float dt, const Tilemap& tilemap);
    void render(float alpha) override;
    void render(float alpha, Vector2 cameraOffset);

    PickupType type = PickupType::SmallHealth;
    int value = 2;         // How much to restore
    int lifetime = 300;    // 5 seconds at 60Hz before despawning
    float vy = 0;          // Vertical velocity (for drop arc)
    float gravity = 0.15f;
    bool onGround = false;
    std::string persistentId;
    std::string armorPart;  // For ArmorCapsule: "boots", "helmet", "body", "buster"

private:
    int flashTimer_ = 0;  // Flashes before despawning
    int animationTimer_ = 0;
};

} // namespace mmx
