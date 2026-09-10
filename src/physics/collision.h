// collision.h - declares actor-vs-tilemap movement and collision resolution.
// Boundary: physics sets actor contact flags; gameplay decides responses.

#pragma once

#include "entities/actor.h"
#include "systems/tilemap.h"

#include <functional>
#include <optional>

// ============================================================================
// collision.h — Tilemap collision detection and resolution
//
// Collision between actors and the tilemap uses the separate-axis method:
//   1. Move the actor horizontally (add velocity.x to position.x)
//   2. Check for overlapping solid tiles and push the actor out
//   3. Move the actor vertically (add velocity.y to position.y)
//   4. Check for overlapping solid tiles and push the actor out
//
// Why separate axes? If you move diagonally and resolve both at once, the
// actor can "catch" on tile corners and get stuck or teleport through walls.
// By resolving one axis at a time, each resolution is unambiguous — we know
// exactly which direction to push because we only moved in one direction.
//
// One-way platforms get special handling: they only block downward movement,
// and only when the actor's feet were above the platform before moving.
// ============================================================================

namespace mmx {

namespace physics {

using TilePassablePredicate = std::function<bool(int, int)>;

// Optional owner-supplied exclusive AABB-bottom coordinate. Returning empty
// preserves generic slope interpolation; ordinary Actors use that default.
using SlopeSurfacePolicy = std::function<std::optional<float>(
    const Actor&, const Tilemap&, int tileX, int tileY, int rowOffset,
    float feetX, float linearSurfaceY)>;

// Move an actor horizontally and resolve any tilemap collisions.
// Sets touchingWallLeft/Right flags on the actor.
void moveAndResolveX(Actor& actor, const Tilemap& tilemap,
                     const TilePassablePredicate& passable = {},
                     bool allowSlopeStepUp = true);

// R96C platform-support seam. Save the pre-step position, clear stale tile
// flags, then retain normal horizontal wall resolution while scene-owned
// support supplies the vertical result.
void moveAndResolveXOnly(Actor& actor, const Tilemap& tilemap,
                         const TilePassablePredicate& passable = {});

// Move an actor vertically and resolve any tilemap collisions.
// Sets onGround and onCeiling flags on the actor.
// Handles one-way platforms (only block when falling from above).
void moveAndResolveY(Actor& actor, const Tilemap& tilemap,
                     const TilePassablePredicate& passable = {});

// Full movement step: save position, apply gravity, resolve X, resolve Y.
// Call this once per physics tick for each actor.
void moveAndCollide(Actor& actor, const Tilemap& tilemap,
                    const TilePassablePredicate& passable = {},
                    const SlopeSurfacePolicy& slopePolicy = {},
                    bool allowSlopeStepUp = true);

} // namespace physics
} // namespace mmx
