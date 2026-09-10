// pickup.cpp - runs pickup behavior, collection effects, and rendering.
// Owns: pickup animation timers, collectible identity, and grant handling.

#include "entities/pickup.h"
#include "entities/pickup_rendering.h"
#include "systems/asset_cache.h"
#include <cmath>

namespace mmx {
namespace {

constexpr const char* kArmorCapsuleSpritePath =
    "content/x1/sprites/pickups/cp_dr_light_capsule_settle.png";

} // namespace

void Pickup::init(float x, float y, PickupType ptype, const std::string& pid) {
    position = {x, y};
    prevPosition = position;
    type = ptype;
    active = true;
    onGround = false;
    persistentId = pid;
    animationTimer_ = 0;
    setSpriteSheetResource(nullptr);

    // Small bounce upward on spawn (only for enemy drops, not persistent stage items)
    vy = pid.empty() ? -2.0f : 0.0f;

    switch (ptype) {
        case PickupType::SmallHealth:
            value = 2;
            hitboxSize = {8, 8};
            lifetime = 300;
            break;
        case PickupType::LargeHealth:
            value = 8;
            hitboxSize = {10, 10};
            lifetime = 300;
            break;
        case PickupType::SmallAmmo:
            value = 2;
            hitboxSize = {8, 8};
            lifetime = 300;
            break;
        case PickupType::ExtraLife:
            value = 1;
            hitboxSize = {12, 12};
            lifetime = 480; // Stays longer
            break;
        case PickupType::HeartTank:
            value = 2;
            hitboxSize = {14, 14};
            lifetime = 999999; // Permanent until collected
            break;
        case PickupType::SubTank:
            value = 0; // Grants a slot, doesn't heal directly
            hitboxSize = {14, 14};
            lifetime = 999999;
            break;
        case PickupType::ArmorCapsule:
            value = 0;
            hitboxSize = {20, 24}; // Larger than other pickups — it's a capsule
            lifetime = 999999;
            gravity = 0; // Capsules don't fall
            if (IsWindowReady()) {
                setSpriteSheetResource(AssetCache::loadTexture(kArmorCapsuleSpritePath));
            }
            break;
    }

    hitboxOffset = {0, 0};
    if (type == PickupType::HeartTank && !persistentId.empty()) {
        // Authored Heart Tank coordinates are stable sprite centres. The
        // renderer uses a 16x16 frame, while the collection box is 14x14.
        // Keep both centred on the authored point and do not let the permanent
        // stage pickup fall away from its source-measured placement.
        hitboxOffset = {1, 1};
        gravity = 0.0f;
    }
}

void Pickup::update(float /*dt*/, const Tilemap& tilemap) {
    if (!active) return;

    prevPosition = position;
    if (type == PickupType::HeartTank) {
        animationTimer_ =
            (animationTimer_ + 1) % pickup_rendering::kHeartTankAnimationPeriod;
    }

    // Apply gravity
    if (!onGround) {
        vy += gravity;
        if (vy > 5.0f) vy = 5.0f; // Terminal velocity
    }

    // Remember where feet were before moving (for one-way platform check)
    AABB prevBox = getHitbox();
    float prevBottom = prevBox.bottom();

    // Move vertically
    position.y += vy;
    onGround = false;

    // Resolve tilemap collision (Y only)
    AABB box = getHitbox();
    int ts = tilemap.tileSize();
    int startCol = static_cast<int>(std::floor(box.left() / ts));
    int startRow = static_cast<int>(std::floor(box.top() / ts));
    int endCol   = static_cast<int>(std::floor((box.right() - 0.01f) / ts));
    int endRow   = static_cast<int>(std::floor((box.bottom() - 0.01f) / ts));

    for (int row = startRow; row <= endRow; row++) {
        for (int col = startCol; col <= endCol; col++) {
            TileType ttype = tilemap.getTileType(col, row);
            if (ttype == TileType::None) continue;

            float tileTop = static_cast<float>(row * ts);

            // One-way platforms: only block when falling from above
            if (ttype == TileType::OneWay) {
                if (vy <= 0) continue;
                if (prevBottom > tileTop + 1.0f) continue;
            }

            AABB tileBox = {
                static_cast<float>(col * ts),
                tileTop,
                static_cast<float>(ts),
                static_cast<float>(ts)
            };

            // Recheck overlap after any correction
            box = getHitbox();
            if (!box.overlaps(tileBox)) continue;

            if (vy > 0) {
                // Falling — land on top of tile
                position.y = tileTop - hitboxOffset.y - hitboxSize.y;
                vy = 0;
                onGround = true;
            } else if (vy < 0 && (ttype == TileType::Solid || ttype == TileType::Spike)) {
                // Rising — bonk head
                position.y = static_cast<float>((row + 1) * ts) - hitboxOffset.y;
                vy = 0;
            }
        }
    }

    lifetime--;
    if (lifetime <= 0) {
        active = false;
    }
}

void Pickup::render(float alpha) {
    render(alpha, {0.0f, 0.0f});
}

void Pickup::render(float alpha, Vector2 cameraOffset) {
    if (!active) return;

    float drawX = prevPosition.x + (position.x - prevPosition.x) * alpha - cameraOffset.x;
    float drawY = prevPosition.y + (position.y - prevPosition.y) * alpha - cameraOffset.y;

    // Flash when about to despawn (last 2 seconds)
    if (lifetime < 120 && (lifetime / 4) % 2 == 0) return;

    Color color;
    switch (type) {
        case PickupType::SmallHealth:
            color = {255, 80, 80, 255};    // Red orb
            break;
        case PickupType::LargeHealth:
            color = {255, 120, 120, 255};  // Bright red orb
            break;
        case PickupType::SmallAmmo:
            color = {80, 180, 255, 255};   // Blue orb
            break;
        case PickupType::ExtraLife:
            color = {255, 220, 80, 255};   // Gold
            break;
        case PickupType::HeartTank:
            color = {255, 100, 255, 255};  // Pink/Magenta
            break;
        case PickupType::SubTank:
            color = {100, 255, 255, 255};  // Light Blue/Cyan
            break;
        case PickupType::ArmorCapsule:
            color = {255, 255, 200, 255};  // Glowing gold/white
            break;
    }

    if (type == PickupType::ArmorCapsule) {
        // A capsule created before the render window was ready gets one
        // content-backed retry here. Never replace missing source art with the
        // old invented rectangle silhouette.
        if (!hasSpriteSheet() && IsWindowReady()) {
            setSpriteSheetResource(AssetCache::loadTexture(kArmorCapsuleSpritePath));
        }
        if (!hasSpriteSheet()) return;
        const TextureResource* tex = spriteSheetResource();
        DrawTexture(
            tex->get(),
            static_cast<int>(std::round(drawX)),
            static_cast<int>(std::round(drawY)),
            WHITE
        );
        return;
    } else if (type == PickupType::HeartTank) {
        // Heart tanks use the existing source-backed item atlas. Keep the old
        // colour block as a fail-soft fallback if the runtime asset is absent.
        if (!hasSpriteSheet() && IsWindowReady()) {
            setSpriteSheetResource(
                AssetCache::loadTexture(pickup_rendering::kItemsAtlasPath));
        }
        if (hasSpriteSheet()) {
            const TextureResource* tex = spriteSheetResource();
            const Rectangle source = pickup_rendering::kHeartTankFrames[
                pickup_rendering::heartTankFrameAtTick(animationTimer_)];
            const Rectangle destination{
                std::round(drawX), std::round(drawY), source.width, source.height};
            DrawTexturePro(tex->get(), source, destination, {0, 0}, 0.0f, WHITE);
            return;
        }
    }

    DrawRectangle(
        static_cast<int>(drawX), static_cast<int>(drawY),
        static_cast<int>(hitboxSize.x), static_cast<int>(hitboxSize.y),
        color
    );
}

} // namespace mmx
