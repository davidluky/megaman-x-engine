#pragma once
#include "systems/tilemap.h"
#include "entities/player.h"
#include "entities/player_anchor.h"
#include <cmath>
#include <optional>
namespace mmx::player_source_terrain {
// cp_dashjump_wall_contact_2026-09-17.json; A552 bytes5..9 = 0,-1,7,17,8.
// Query after both coordinate integrations. Empty means this measured lane
// cannot resolve the terrain; never substitute it for body/damage geometry.
inline std::optional<float> horizontalAnchor(const Tilemap& map, float x,
                                              float y, bool right,
                                              SourceContactProfile profile=SourceContactProfile::NormalA552) {
    if (!right || map.tileSize()!=16 || x<0 || y<18) return std::nullopt;
    // dash_flat_floor_2026-09-17.json: BB38 widens the same three probes.
    const int probeX=static_cast<int>(std::floor(x))+
        (profile==SourceContactProfile::ActiveDashBB38 ? 9 : 7);
    const int ramY=static_cast<int>(std::floor(y));
    // Source order: center, upper inset, lower inset, return on first contact.
    for (int offset : {-1,-10,8}) {
        const int col=probeX/16, row=(ramY+offset)/16;
        if (col>=map.width() || row>=map.height()) return std::nullopt;
        const auto type=map.getTileType(col,row);
        if (type==TileType::Solid)
            return x-static_cast<float>((probeX&15)+1);
        // horizontal_dispatch_table_2026-09-17.json: source table935D
        // sends these attributes to unconditional no-hit handler977A.
        const auto attr=map.getRawAttr(col,row)&0x3f;
        if ((type==TileType::SlopeR || type==TileType::SlopeL) &&
            attr<=50 && attr!=17) continue;
        if (type!=TileType::None) return std::nullopt;
    }
    return x;
}
// flat_ground_continuation_2026-09-17.json: same-height raw3B floor only.
inline bool tryMoveGroundedFlat(Player& player, const Tilemap& map) {
    const auto profile=player.sourceContactProfile();
    if (!player.sourceGroundMovementEnabled() || !player.onGround ||
        !player.facingRight || player.velocity.x<0 ||
        (player.state()!=PlayerState::Idle && player.state()!=PlayerState::Run &&
         player.state()!=PlayerState::Dash) || !profile) return false;
    const float intendedX=player.position.x+player.velocity.x;
    const float ramY=player.position.y+40;
    const float anchor=player_anchor::sourceRamAnchorX(intendedX,player.spriteWidth,true);
    const auto resolved=horizontalAnchor(map,anchor,ramY,true,*profile);
    if (!resolved) return false;
    const int x=static_cast<int>(std::floor(*resolved));
    const int foot=static_cast<int>(std::floor(ramY))+16+6;
    if (x<0 || foot<0 || x/16>=map.width() || foot/16>=map.height() ||
        map.getTileType(x/16,foot/16)!=TileType::Solid ||
        (map.getRawAttr(x/16,foot/16)&0x3f)!=0x3B ||
        6-(foot&15)-1!=0) return false; // slope/ledge handoffs keep their owner
    player.savePosition();
    player.position.x=intendedX+*resolved-anchor;
    player.velocity.y=0;
    player.onCeiling=false;
    player.touchingWallLeft=false;
    player.touchingWallRight=*resolved<anchor;
    if (player.touchingWallRight) player.velocity.x=0;
    return true;
}
inline bool tryMoveAirborne(Player& player, const Tilemap& map) {
    if (!player.sourceGroundMovementEnabled() || !player.facingRight ||
        (player.state()!=PlayerState::DashJump && player.state()!=PlayerState::Jump &&
         player.state()!=PlayerState::Fall) ||
        player.velocity.x<=0 ||
        player.sourceContactProfile()!=SourceContactProfile::NormalA552) return false;
    Actor step = static_cast<const Actor&>(player);
    step.savePosition();
    step.applyGravity();
    const float intendedX=step.position.x+step.velocity.x;
    const float intendedY=step.position.y+step.velocity.y;
    const float anchor=player_anchor::sourceRamAnchorX(intendedX,player.spriteWidth,true);
    const auto resolved=horizontalAnchor(map,anchor,intendedY+40,true);
    if (!resolved) return false;
    // Executed ceiling lane: center then +/-7, RAM Y-18. Solid/other ceiling
    // responses are unmeasured here; decline without mutating the player.
    // terrain_probe_dispatch_2026-09-17.json: the descending lane issues ONE
    // probe, at the anchor column (profile byte +05 = 0); the vertical routine
    // adds the reach in 8495C4 and calls the lookup once, with no loop over
    // columns. Empty terrain dispatches table84:961C entry0 to handler 84:969C
    // (LDA #$00, RTS) and returns through 849616 without touching the Y word or
    // the support bit, so the fall simply continues.
    // flat_floor_landing_2026-09-17.json: raw3B changes integer Y only.
    std::optional<float> groundedY;
    const int row=static_cast<int>(std::floor(intendedY+40))+
        (step.velocity.y<0 ? -18 : 16);
    if (row<0 || row/16>=map.height()) return false;
    if (step.velocity.y<0) {
        for (int offset : {0,-7,7}) {
            const int x=static_cast<int>(std::floor(*resolved))+offset;
            if (x<0 || x/16>=map.width() ||
                map.getTileType(x/16,row/16)!=TileType::None) return false;
        }
    } else {
        const int x=static_cast<int>(std::floor(*resolved));
        if (x<0 || x/16>=map.width()) return false;
        const auto type=map.getTileType(x/16,row/16);
        if (type==TileType::Solid && (map.getRawAttr(x/16,row/16)&0x3f)==0x3B) {
            groundedY=intendedY-static_cast<float>((row&15)+1);
        } else if (type!=TileType::None) {
            return false; // other foot attributes keep their existing owner
        }
    }
    player.savePosition();
    player.gravityStepSpent=step.gravityStepSpent;
    player.velocity=step.velocity;
    player.position={intendedX+*resolved-anchor,groundedY.value_or(intendedY)};
    player.onGround=groundedY.has_value();
    if (player.onGround) player.velocity.y=0;
    player.onCeiling=false;
    player.touchingWallLeft=false;
    player.touchingWallRight=*resolved<anchor;
    if (player.touchingWallRight) player.velocity.x=0;
    return true;
}
// hurt_terrain_2026-09-19: leftward Hurt descent on raw0B/0C quarter slopes.
// The source keeps the Y fraction and landing-row velocity; the next supported
// step clears vertical speed before gravity. Other terrain keeps its owner.
inline bool tryMoveHurtQuarter(Player& player, const Tilemap& map) {
    if (!player.sourceGroundMovementEnabled() || !player.facingRight ||
        player.state()!=PlayerState::Hurt || player.velocity.x>=0 ||
        player.sourceContactProfile()!=SourceContactProfile::NormalA552 ||
        map.tileSize()!=16) return false;
    Actor step=static_cast<const Actor&>(player);
    if (step.onGround && step.velocity.y>=0) step.velocity.y=0;
    step.applyGravity();
    if (step.velocity.y<0) return false;
    const float x=step.position.x+step.velocity.x;
    const float y=step.position.y+step.velocity.y;
    const int anchorX=static_cast<int>(std::floor(x+32));
    const int anchorY=static_cast<int>(std::floor(y+40));
    if (anchorX<7 || anchorY<10 || anchorX+7>=map.width()*16 ||
        anchorY+22>=map.height()*16) return false;
    // No measured wall response in this lane: require clear side probes.
    for (int dx : {-7,7}) for (int dy : {-1,-10,8})
        if (map.getTileType((anchorX+dx)/16,(anchorY+dy)/16)!=TileType::None)
            return false;
    const int footY=anchorY+16+(player.onGround ? 6 : 0);
    const int col=anchorX/16, row=footY/16;
    const int attr=map.getRawAttr(col,row)&0x3f;
    const auto slope=map.getSlope(col,row);
    if ((attr!=0x0B && attr!=0x0C) ||
        slope.leftY!=(12-attr)*4 || slope.rightY!=slope.leftY+4)
        return false;
    const int height=slope.leftY+((anchorX&15)+1)/4;
    const int floorY=row*16+height-17;
    const bool hit=(footY&15)+1>height; // 84:97EA/97EC rejects equality.
    if (!hit) {
        // Secondary probes have reach bit7 clear: these quarter handlers
        // return no hit. Decline unknown attributes instead of ignoring them.
        for (int dx : {-7,7}) {
            const int raw=map.getRawAttr((anchorX+dx)/16,footY/16)&0x3f;
            if (raw!=0 && raw!=0x0B && raw!=0x0C) return false;
        }
    }
    player.savePosition();
    player.position={x,y+(hit ? floorY-anchorY : 0)};
    player.velocity=step.velocity;
    player.gravityStepSpent=step.gravityStepSpent;
    player.onGround=hit;
    player.onCeiling=false;
    player.touchingWallLeft=false;
    player.touchingWallRight=false;
    return true;
}

}
