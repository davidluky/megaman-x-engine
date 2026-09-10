// collision.cpp - resolves actor movement against tilemap collision geometry.
// Owns: solid, one-way, conveyor, slope, and wall/ground contact resolution.

#include "physics/collision.h"
#include "entities/actor.h"
#include "app/constants.h"
#include <algorithm>
#include <cmath>

namespace mmx {
namespace physics {

// Tile types that behave as solid floor/wall material: they block movement,
// catch falling feet, and are valid ground-snap targets. Conveyor belts are
// solid floors — the horizontal push (U62/M3) layers on top in the player
// physics, it does not change their solidity.
static bool isSolidLike(TileType t) {
    return t == TileType::Solid || t == TileType::Breakable ||
           t == TileType::Conveyor;
}

static TileType collisionTypeAt(const Tilemap& tilemap, int col, int row,
                                const TilePassablePredicate& passable) {
    return passable && passable(col, row)
        ? TileType::None
        : tilemap.getTileType(col, row);
}

// Internal: get the range of tiles that a hitbox overlaps
static void getTileRange(const AABB& box, int tileSize,
                         int& startCol, int& startRow, int& endCol, int& endRow) {
    startCol = static_cast<int>(std::floor(box.left() / tileSize));
    startRow = static_cast<int>(std::floor(box.top() / tileSize));
    endCol   = static_cast<int>(std::floor((box.right() - 0.01f) / tileSize));
    endRow   = static_cast<int>(std::floor((box.bottom() - 0.01f) / tileSize));
}

// Compute slope surface Y (world coords) at a given world X for the slope tile
// at (tileX, tileY). Returns false if the tile is not a slope.
//
// Slope surface is a straight line from (tileX*ts, ty*ts+leftY) to
// ((tileX+1)*ts, ty*ts+rightY). leftY/rightY come from Tilemap::getSlope and
// are measured in pixels from the tile's top edge.
static bool getSlopeSurfaceY(const Tilemap& tm, int tileX, int tileY,
                             float worldX, float& outSurfaceY) {
    TileType t = tm.getTileType(tileX, tileY);
    if (t != TileType::SlopeL && t != TileType::SlopeR) return false;
    int ts = tm.tileSize();
    SlopeHeight h = tm.getSlope(tileX, tileY);
    float localX = worldX - static_cast<float>(tileX * ts);
    if (localX < 0) localX = 0;
    if (localX > ts) localX = static_cast<float>(ts);
    float surfLocal = static_cast<float>(h.leftY) +
                      (static_cast<float>(h.rightY) - static_cast<float>(h.leftY)) *
                      localX / static_cast<float>(ts);
    outSurfaceY = static_cast<float>(tileY * ts) + surfLocal;
    return true;
}

// True if a slope tile sits at (or directly above/below) the actor's feet —
// i.e. the actor is currently climbing/descending a slope. Used by
// moveAndResolveX to allow a small step-up over the solid "fill" under a ramp
// (and the flat tile a ramp feeds onto) instead of treating it as a wall.
static bool onSlopeContext(const Actor& actor, const Tilemap& tilemap) {
    AABB box = actor.getHitbox();
    int ts = tilemap.tileSize();
    float feetX = box.left() + box.w * 0.5f;
    float feetY = box.bottom();
    int feetCol = static_cast<int>(std::floor(feetX / ts));
    int feetRow = static_cast<int>(std::floor(feetY / ts));
    // Scan the feet column and its immediate neighbours: at a slope→flat junction
    // the feet centre sits one column away from the slope tile its leading edge is
    // climbing onto, so a single-column check flickers and the step-up stalls.
    for (int dc = -1; dc <= 1; dc++) {
        for (int dr = -1; dr <= 1; dr++) {
            TileType t = tilemap.getTileType(feetCol + dc, feetRow + dr);
            if (t == TileType::SlopeL || t == TileType::SlopeR) return true;
        }
    }
    return false;
}

// Snap an actor to a slope surface under its feet if one is close enough.
// Runs AFTER the normal X and Y collision resolution so feet stick to the slope
// both when climbing (surface rises beneath moving feet) and descending
// (surface falls, actor would otherwise lose ground contact and float out).
static void snapToSlope(Actor& actor, const Tilemap& tilemap,
                        const SlopeSurfacePolicy& slopePolicy) {
    AABB box = actor.getHitbox();
    int ts = tilemap.tileSize();
    float feetX = box.left() + box.w * 0.5f;  // feet center
    float feetY = box.bottom();

    int feetCol = static_cast<int>(std::floor(feetX / ts));
    int feetRow = static_cast<int>(std::floor(feetY / ts));

    // Check the tile at feet level, the tile just above (for when feet sit
    // above a slope that rises into them), and the tile just below (for when
    // walking off the top of a slope onto its surface).
    for (int dr = -1; dr <= 1; dr++) {
        int row = feetRow + dr;
        float surfY;
        if (!getSlopeSurfaceY(tilemap, feetCol, row, feetX, surfY)) continue;
        if (slopePolicy) {
            if (const auto sourceSurface = slopePolicy(
                    actor, tilemap, feetCol, row, dr, feetX, surfY)) {
                surfY = *sourceSurface;
            }
        }

        float diff = surfY - feetY;  // positive = surface below feet, negative = feet inside slope

        // Snap UP: feet are inside or at the slope surface and actor isn't jumping.
        // Tolerance <= ts because in the worst case feet straddle two slope tiles.
        if (diff <= 0.5f && diff >= -static_cast<float>(ts) && actor.velocity.y >= 0) {
            actor.position.y = surfY - actor.hitboxOffset.y - actor.hitboxSize.y;
            actor.velocity.y = 0;
            actor.onGround = true;
            return;
        }
        // Snap DOWN: previously grounded, feet just above a slope surface that
        // dropped under them this frame (walking or dashing downhill). Without
        // this, each frame of downhill motion detaches feet, re-applies gravity
        // next frame, lands, repeat — a visible jitter.
        // Guard against upward velocity: if the actor is jumping, do not snap
        // back — the same guard that snapToGround applies.
        if (diff > 0 && diff <= static_cast<float>(ts) && actor.wasOnGround && !actor.onGround
            && actor.velocity.y >= 0) {
            actor.position.y = surfY - actor.hitboxOffset.y - actor.hitboxSize.y;
            actor.velocity.y = 0;
            actor.onGround = true;
            return;
        }
        // Snap DOWN onto the diagonal the feet are standing INSIDE (plan task
        // R4.2). The ordinary Y resolution uses the whole 24 px hitbox, so the
        // flat floor a ramp feeds into lifts a grounded actor to the tile top
        // as soon as his leading edge crosses that column -- 2.8 px before the
        // source leaves the ramp, measured on the Chill Penguin movie at tile
        // (21, 74). The source's feet ride the diagonal under the feet CENTRE:
        // over 1,636 grounded Chill Penguin slope frames and 114 Storm Eagle
        // ones they are within one pixel of it on 96.1 % and 93.9 %. Only the
        // tile the feet are actually inside (dr == 0) qualifies, so walking off
        // a ramp's top onto higher ground is never pulled back down.
        if (dr == 0 && diff > 0.5f && diff <= static_cast<float>(ts) && actor.onGround
            && actor.velocity.y >= 0) {
            actor.position.y = surfY - actor.hitboxOffset.y - actor.hitboxSize.y;
            actor.velocity.y = 0;
            return;
        }
    }
}

// Ground-snap across a slope→flat seam. When a grounded actor walks off the last
// slope tile a sub-pixel ABOVE the flat floor the ramp feeds into, snapToSlope
// can't catch the feet (it only tracks slope tiles) and the flat floor sits just
// below them — so without this the actor free-falls for 1-2 frames, which flips
// the player to the airborne "fall" pose at the ramp bottom (the "falls onto the
// flat" bug). If the actor was grounded last frame, isn't rising, and a solid
// floor sits within a small tolerance directly below the feet, snap down to it.
// kSnap is far under a tile so genuine ledges (≥1 tile drop) still fall.
static void snapToGround(Actor& actor, const Tilemap& tilemap,
                         const TilePassablePredicate& passable) {
    if (actor.onGround || !actor.wasOnGround || actor.velocity.y < 0) return;
    const float kSnap = 2.0f;
    AABB box = actor.getHitbox();
    int ts = tilemap.tileSize();
    float feetY = box.bottom();
    int probeRow = static_cast<int>(std::floor((feetY + kSnap) / ts));
    float tileTop = static_cast<float>(probeRow * ts);
    float gap = tileTop - feetY;  // >0 = floor below the feet
    if (gap < 0.0f || gap > kSnap) return;
    int startCol = static_cast<int>(std::floor(box.left() / ts));
    int endCol = static_cast<int>(std::floor((box.right() - 0.01f) / ts));
    for (int col = startCol; col <= endCol; ++col) {
        TileType t = collisionTypeAt(tilemap, col, probeRow, passable);
        if (!isSolidLike(t)) continue;
        actor.position.y = tileTop - actor.hitboxOffset.y - actor.hitboxSize.y;
        actor.velocity.y = 0;
        actor.onGround = true;
        return;
    }
}

void moveAndResolveX(Actor& actor, const Tilemap& tilemap,
                     const TilePassablePredicate& passable,
                     bool allowSlopeStepUp) {
    // Move horizontally
    actor.position.x += actor.velocity.x;

    actor.touchingWallLeft = false;
    actor.touchingWallRight = false;

    AABB box = actor.getHitbox();
    int ts = tilemap.tileSize();
    int startCol, startRow, endCol, endRow;
    getTileRange(box, ts, startCol, startRow, endCol, endRow);

    for (int row = startRow; row <= endRow; row++) {
        for (int col = startCol; col <= endCol; col++) {
            TileType type = collisionTypeAt(tilemap, col, row, passable);
            // Only solid tiles, spikes, and breakable tiles block horizontal movement.
            // One-way platforms are passable from the sides.
            if (!isSolidLike(type) && type != TileType::Spike) continue;

            AABB tileBox = {
                static_cast<float>(col * ts),
                static_cast<float>(row * ts),
                static_cast<float>(ts),
                static_cast<float>(ts)
            };

            // Recheck overlap after any previous correction this frame
            box = actor.getHitbox();
            if (!box.overlaps(tileBox)) continue;

            // Slope step-up: a grounded actor climbing a slope must not be walled
            // by the solid "fill" beneath the ramp, nor by the flat tile the ramp
            // feeds onto, when that solid's top sits only a few pixels above its
            // feet. Lift the actor onto the tile top and keep moving so snapToSlope
            // can finish placing it on the surface. Tolerance stays well under a
            // full tile so real ledges/walls still block.
            // Slope handling while a grounded actor is on/at a slope:
            if (actor.onGround && onSlopeContext(actor, tilemap)) {
                TileType above = tilemap.getTileType(col, row - 1);
                bool substrate = (above == TileType::SlopeL || above == TileType::SlopeR);
                if (substrate) {
                    // Ramp body — don't block and don't lift. snapToSlope carries
                    // the feet up/down the diagonal smoothly (lifting to the tile
                    // top here is what made X "teleport" up the ramp).
                    continue;
                }
                // Small step-up onto the flat tile a ramp feeds onto (its top sits
                // a few px above the feet as the ramp apex meets it).
                float diff = box.bottom() - tileBox.top();
                if (diff > 0.0f && diff <= 6.0f) {
                    if (allowSlopeStepUp) {
                        actor.position.y = tileBox.top() - actor.hitboxOffset.y - actor.hitboxSize.y;
                    }
                    continue;
                }
            }

            if (actor.velocity.x > 0) {
                // Moving right — push left edge of actor to left edge of tile
                actor.position.x = tileBox.left() - actor.hitboxOffset.x - actor.hitboxSize.x;
                actor.velocity.x = 0;
                actor.touchingWallRight = true;
            } else if (actor.velocity.x < 0) {
                // Moving left — push right edge of actor to right edge of tile
                actor.position.x = tileBox.right() - actor.hitboxOffset.x;
                actor.velocity.x = 0;
                actor.touchingWallLeft = true;
            }
        }
    }
}

void moveAndResolveXOnly(Actor& actor, const Tilemap& tilemap,
                         const TilePassablePredicate& passable) {
    // Platform support supplies the vertical result. Save the pre-step
    // position, clear stale tile flags, and retain normal horizontal wall
    // resolution.
    actor.savePosition();
    actor.onGround = false;
    actor.onCeiling = false;
    moveAndResolveX(actor, tilemap, passable);
}

void moveAndResolveY(Actor& actor, const Tilemap& tilemap,
                     const TilePassablePredicate& passable) {
    // Remember where feet were before moving (for one-way platform check)
    AABB prevBox = actor.getHitbox();
    float prevBottom = prevBox.bottom();

    // Move vertically
    actor.position.y += actor.velocity.y;

    actor.onGround = false;
    actor.onCeiling = false;

    AABB box = actor.getHitbox();
    int ts = tilemap.tileSize();
    int startCol, startRow, endCol, endRow;
    getTileRange(box, ts, startCol, startRow, endCol, endRow);

    for (int row = startRow; row <= endRow; row++) {
        for (int col = startCol; col <= endCol; col++) {
            TileType type = collisionTypeAt(tilemap, col, row, passable);
            if (type == TileType::None || type == TileType::Ladder) continue;

            // Slopes are resolved by snapToSlope() after Y collision — skip
            // them here so a 45° slope tile doesn't behave as a full 16×16
            // solid block, which is how this path would otherwise treat it.
            if (type == TileType::SlopeL || type == TileType::SlopeR) continue;

            // Ramp substrate: a solid sitting directly under a slope tile. While
            // the actor is riding a slope, skip it so the wide AABB doesn't snap
            // the actor up to the substrate's quantised tile-top (the visible
            // "teleport"). snapToSlope places the feet on the smooth diagonal
            // surface via the centre point instead.
            if (type == TileType::Solid &&
                (actor.wasOnGround || onSlopeContext(actor, tilemap))) {
                TileType above = tilemap.getTileType(col, row - 1);
                if (above == TileType::SlopeL || above == TileType::SlopeR) continue;
            }

            float tileTop = static_cast<float>(row * ts);
            float tileBottom = static_cast<float>((row + 1) * ts);

            // One-way platforms: only block when falling from above.
            // The actor must have been above the platform top before moving,
            // and must now overlap it. This lets actors jump through from below.
            if (type == TileType::OneWay) {
                if (actor.velocity.y <= 0) continue;      // Moving up — pass through
                if (prevBottom > tileTop + 1.0f) continue; // Was already below top — pass
            }

            AABB tileBox = {
                static_cast<float>(col * ts),
                tileTop,
                static_cast<float>(ts),
                static_cast<float>(ts)
            };

            // Recheck overlap
            box = actor.getHitbox();
            if (!box.overlaps(tileBox)) continue;

            if (actor.velocity.y > 0) {
                // Falling — land on top of tile
                actor.position.y = tileTop - actor.hitboxOffset.y - actor.hitboxSize.y;
                actor.velocity.y = 0;
                actor.onGround = true;
            } else if (actor.velocity.y < 0 &&
                       (isSolidLike(type) || type == TileType::Spike)) {
                // Rising — bonk head on bottom of tile
                actor.position.y = tileBottom - actor.hitboxOffset.y;
                actor.velocity.y = 0;
                actor.onCeiling = true;
            }
        }
    }
}

static float conveyorPushForAttr(int attr) {
    switch (attr & 0x3F) {
        case 0x37:
            return -0.5f;
        case 0x38:
            return 0.5f;
        default:
            return 0.0f;
    }
}

static float conveyorPushAtFeet(const Actor& actor, const Tilemap& tilemap) {
    if (!actor.onGround) return 0.0f;

    AABB box = actor.getHitbox();
    int ts = tilemap.tileSize();
    int row = static_cast<int>(std::floor(box.bottom() / static_cast<float>(ts)));

    auto pushAt = [&](int col) {
        if (tilemap.getTileType(col, row) != TileType::Conveyor) return 0.0f;
        return conveyorPushForAttr(tilemap.getRawAttr(col, row));
    };

    int centerCol = static_cast<int>(std::floor((box.left() + box.w * 0.5f) /
                                                static_cast<float>(ts)));
    float push = pushAt(centerCol);
    if (push != 0.0f) return push;

    int startCol = static_cast<int>(std::floor(box.left() / static_cast<float>(ts)));
    int endCol = static_cast<int>(std::floor((box.right() - 0.01f) /
                                             static_cast<float>(ts)));
    for (int col = startCol; col <= endCol; ++col) {
        if (col == centerCol) continue;
        push = pushAt(col);
        if (push != 0.0f) return push;
    }
    return 0.0f;
}

// Returns true when a push was applied, because it re-runs the X resolution
// and so can lift a grounded actor off a slope (see moveAndCollide).
static bool applyConveyorPush(Actor& actor, const Tilemap& tilemap) {
    float push = conveyorPushAtFeet(actor, tilemap);
    if (push == 0.0f) return false;

    bool wasTouchingLeft = actor.touchingWallLeft;
    bool wasTouchingRight = actor.touchingWallRight;
    float velocityX = actor.velocity.x;

    actor.velocity.x = push;
    moveAndResolveX(actor, tilemap);

    actor.touchingWallLeft = wasTouchingLeft || actor.touchingWallLeft;
    actor.touchingWallRight = wasTouchingRight || actor.touchingWallRight;
    actor.velocity.x = velocityX;
    return true;
}

void moveAndCollide(Actor& actor, const Tilemap& tilemap,
                    const TilePassablePredicate& passable,
                    const SlopeSurfacePolicy& slopePolicy,
                    bool allowSlopeStepUp) {
    actor.savePosition();
    actor.applyGravity();
    moveAndResolveX(actor, tilemap, passable, allowSlopeStepUp);
    moveAndResolveY(actor, tilemap, passable);
    snapToSlope(actor, tilemap, slopePolicy);
    snapToGround(actor, tilemap, passable);
    if (applyConveyorPush(actor, tilemap)) {
        // The push re-runs the X resolution, and its slope step-up lifts the
        // feet onto the tile top again, so the diagonal has to be re-applied
        // (plan task R4.2; the Flame Mammoth ramp sits on a belt).
        snapToSlope(actor, tilemap, slopePolicy);
    }
}

} // namespace physics
} // namespace mmx
