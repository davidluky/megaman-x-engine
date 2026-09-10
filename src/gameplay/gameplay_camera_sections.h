// gameplay_camera_sections.h - applies stage camera-section rules to gameplay.
// Boundary: reads tilemap sections; camera math itself stays in systems/camera.

#pragma once

#include "entities/entity.h"
#include "systems/camera.h"
#include "systems/tilemap.h"

#include <string>
#include <string_view>

namespace mmx::gameplay_camera_sections {

// R2.vertical: the B8 RAM-to-scene calibration converts the fitted source
// screen band [96,128] into the live Camera::update target band [90,122].
// Keep this default stage-scoped until the other eleven stages have measured
// vertical laws of their own.
inline constexpr float kMeasuredStageVerticalTop = 90.0f;
inline constexpr float kMeasuredStageVerticalBottom = 122.0f;
inline constexpr float kMeasuredStageVerticalLag = 0.0f;

inline bool applyMeasuredStageVerticalDefault(std::string_view stageId,
                                              Camera& camera) {
    if (stageId != "chill-penguin" && stageId != "storm-eagle") {
        return false;
    }
    camera.setVerticalFollowDeadzone(kMeasuredStageVerticalTop,
                                     kMeasuredStageVerticalBottom,
                                     kMeasuredStageVerticalLag);
    return true;
}

inline bool hitboxOverlapsRect(const AABB& hitbox,
                               const CameraSectionRect& rect) {
    return hitbox.right() > rect.x &&
           hitbox.left() < rect.x + rect.w &&
           hitbox.bottom() > rect.y &&
           hitbox.top() < rect.y + rect.h;
}

inline std::string activeVisualSectionIdForHitbox(const Tilemap& tilemap,
                                                  const AABB& hitbox,
                                                  bool bossLocked) {
    return tilemap.visualSectionIdForRect(
        hitbox.x, hitbox.y, hitbox.w, hitbox.h, bossLocked);
}

inline void clearCameraSection(std::string& activeCameraSectionId,
                               Camera& camera) {
    activeCameraSectionId.clear();
    camera.clearVerticalMode();
}

inline bool applyCameraSectionForHitbox(const Tilemap& tilemap,
                                        const AABB& hitbox,
                                        Camera& camera,
                                        std::string& activeCameraSectionId) {
    for (const auto& section : tilemap.cameraSections()) {
        if (!hitboxOverlapsRect(hitbox, section.rect)) continue;

        activeCameraSectionId = section.id;
        camera.clearRoom();
        if (section.mode == CameraSectionMode::Lock) {
            camera.setVerticalLock(section.lockCamY);
        } else {
            camera.setVerticalFollowDeadzone(section.deadzoneTop,
                                             section.deadzoneBottom,
                                             section.lagPxPerFrame);
        }
        return true;
    }
    return false;
}

inline const Room* unnamedRoomForHorizontalHitboxOverlap(const Tilemap& tilemap,
                                                         const AABB& hitbox) {
    for (const auto& room : tilemap.rooms()) {
        if (!room.name.empty()) continue;
        if (hitbox.right() > room.x && hitbox.left() < room.x + room.w) {
            return &room;
        }
    }
    return nullptr;
}

inline void applyPlayerCameraRoom(const Tilemap& tilemap,
                                  const AABB& hitbox,
                                  bool bossLocked,
                                  Camera& camera,
                                  std::string& activeCameraSectionId,
                                  std::string& activeVisualSectionId) {
    activeVisualSectionId =
        activeVisualSectionIdForHitbox(tilemap, hitbox, bossLocked);

    // While a boss arena is locked, that fight owns the camera room.
    if (bossLocked) {
        activeCameraSectionId.clear();
        return;
    }

    if (applyCameraSectionForHitbox(
            tilemap, hitbox, camera, activeCameraSectionId)) {
        return;
    }
    clearCameraSection(activeCameraSectionId, camera);

    // Unnamed rooms act as camera bounds: vertical lock plus horizontal cap.
    // Membership is HITBOX overlap, not sprite center, so wall-edge clamps do
    // not release while X's body is still inside the room.
    if (const Room* room =
            unnamedRoomForHorizontalHitboxOverlap(tilemap, hitbox)) {
        camera.setRoom(room->x, room->y, room->w, room->h);
        return;
    }

    camera.clearRoom();
}

inline const Room* namedBossRoomForPlayerX(const Tilemap& tilemap,
                                           float playerX) {
    for (const auto& room : tilemap.rooms()) {
        if (!room.name.empty() && playerX >= room.x) {
            return &room;
        }
    }
    return nullptr;
}

} // namespace mmx::gameplay_camera_sections
