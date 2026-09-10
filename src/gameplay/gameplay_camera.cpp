// gameplay_camera.cpp - updates GameplayScene camera and section state.
// Owns: scene camera anchors, section transitions, and render override hooks.

#include "gameplay_scene.h"

#include "gameplay/gameplay_camera_sections.h"
#include "entities/player_anchor.h"

namespace mmx {

// B1: the source's camera law is camera_x = RAM anchor - 128, and the RAM
// anchor is Player::position +32 in the right-facing 70px cell
// (knowledge_base/_shotgun_ice_runs/anchor_idle/anchor_offset.json). Following
// the configured hitbox center instead put the camera one pixel right on Chill
// Penguin, where setupCpEntryHitboxRollout() swaps X's box for the legacy
// {21,22,24,35} rollout and the center lands at +33. Measured 2026-09-05 in
// build/b1: X drawn at screen (113,142) against the source's (114,141).
float GameplayScene::playerCameraAnchorX() const {
    return player_anchor::sourceRamAnchorX(player_.position.x,
                                           player_.spriteWidth,
                                           player_.facingRight);
}

void GameplayScene::updateCameraRoom() {
    updateActiveVisualSection();

    // While a boss arena is locked, that fight owns the camera room - don't
    // override it.
    if (bossLocked_) {
        activeCameraSectionId_.clear();
        return;
    }

    // The Sting body-capsule arena has a source-static camera: every
    // committed hardware still registers at the same window, so the scene
    // owns the camera while it (or its proven post-release tail) is active.
    if (stingChameleonCapsule_.cameraWindowActive()) {
        activeCameraSectionId_.clear();
        camera_.clearVerticalMode();
        camera_.setRoom(
            static_cast<float>(StingChameleonCapsuleCutscene::kArenaCameraX),
            static_cast<float>(StingChameleonCapsuleCutscene::kArenaCameraY),
            static_cast<float>(StingChameleonCapsuleCutscene::kArenaCameraW),
            static_cast<float>(StingChameleonCapsuleCutscene::kArenaCameraH));
        return;
    }

    if (applyCameraSection()) {
        return;
    }
    activeCameraSectionId_.clear();
    camera_.clearVerticalMode();
    gameplay_camera_sections::applyMeasuredStageVerticalDefault(
        activeStageId_.str(), camera_);

    // Unnamed rooms act as camera bounds: they lock the vertical view and cap
    // horizontal scroll, reproducing the real MMX per-room camera (e.g. Chill
    // Penguin's flat, vertically-locked opening). Named rooms are boss-fight
    // triggers handled in update(), so they're skipped here. A tall unnamed
    // room supplies bounds while the selected vertical mode remains active.
    //
    // Membership = HITBOX overlap, not sprite center (U54): X pressed against
    // a room-edge wall keeps his body inside the room while his 30px-wide
    // sprite center crosses the edge - a center test released the clamp there
    // and let the camera expose the uncaptured strip past the wall.
    const AABB hb = player_.getHitbox();
    for (const auto& room : tilemap_.rooms()) {
        if (!room.name.empty()) continue;
        if (hb.right() > room.x && hb.left() < room.x + room.w) {
            camera_.setRoom(room.x, room.y, room.w, room.h);
            return;
        }
    }
    // No unnamed room contains the player: free camera within world bounds.
    camera_.clearRoom();
}

bool GameplayScene::applyCameraSection() {
    const AABB hb = player_.getHitbox();
    for (const auto& section : tilemap_.cameraSections()) {
        const auto& r = section.rect;
        const bool overlaps = hb.right() > r.x && hb.left() < r.x + r.w
            && hb.bottom() > r.y && hb.top() < r.y + r.h;
        if (!overlaps) continue;

        activeCameraSectionId_ = section.id;
        camera_.clearRoom();
        if (section.mode == CameraSectionMode::Lock) {
            camera_.setVerticalLock(section.lockCamY);
        } else {
            camera_.setVerticalFollowDeadzone(section.deadzoneTop,
                                              section.deadzoneBottom,
                                              section.lagPxPerFrame);
        }
        return true;
    }
    return false;
}

void GameplayScene::updateActiveVisualSection() {
    const AABB hb = player_.getHitbox();
    activeVisualSectionId_ = tilemap_.visualSectionIdForRect(
        hb.x, hb.y, hb.w, hb.h, bossLocked_);
}

} // namespace mmx
