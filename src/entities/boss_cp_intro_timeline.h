// boss_cp_intro_timeline.h - pure source-clock CP boss-entry choreography.
// Oracle: B1 boss.csv and mailbox_writes.csv, door-touch-local t84..409.

#pragma once

#include <algorithm>

namespace mmx::boss_cp_intro_timeline {

inline constexpr int kSourceSpawnTick = 84;
inline constexpr int kFallTick = 264;
// The object begins its fall state at t264, but the source raster remains
// empty at t264/t266; the tucked body first enters the viewport at t268.
inline constexpr int kFirstVisibleBodyTick = 268;
inline constexpr int kLandTick = 300;
// CP-B1C-H1-O1: the boss HP gauge is absent through t312 and first visible at
// exactly t313, measured gaplessly from the every-frame source capture. t312 is
// this timeline's sub_state->4 event, so the gauge appears on the next tick.
inline constexpr int kHudRevealTick = 313;
inline constexpr int kFillTick = 340;
inline constexpr int kFullTick = 402;
inline constexpr int kLastFillApuTick = 400;
inline constexpr int kFightTick = 409;

struct Frame {
    float y = 0.0f;
    int displayHealth = 0;
    bool emitFillApu = false;
    bool fightOn = false;
    bool hudVisible = false;
};

// CP-B1C-T1-M2 entry-motion law. $source:
// knowledge_base/mmx1/bosses/chill-penguin/entry_horizontal_law.json
// (SHA-256 D94847F268AAEB99D0FA6A5E72B5B0A910993EB4D0447842F420C4BB34427BCC):
// the player source/RAM anchor advances 116 fp per tick through t117 into its
// settle value (1969141 -> 1972969 fp over t84..t117) and camera X advances
// 2 px per tick through t116 into its lock value (7680), both held through
// t409. Expressed RELATIVE — offsets into the engine's own settle/lock — per
// the M2 row: absolute placement belongs to CP-B1C-T1-X1. 116/256 is dyadic,
// so the ladder is exact in float.
inline constexpr int kEntryWalkStepFp = 116;
inline constexpr int kEntryWalkLastAdvanceTick = 117;
inline constexpr int kEntrySettleTick = 133;
// Source player.csv: animation byte $D7 begins at t118, three ticks before
// the vertical descent, and holds through the landing tick t133. The
// source-authenticated x_0035 sprite is byte-identical to runtime sheet cell
// 36; normal idle ($78) resumes at t134.
inline constexpr int kEntryPlayerFallPoseFirstTick = 118;
inline constexpr int kEntryPlayerFallPoseLastTick = 133;
inline constexpr int kEntryPlayerFallPoseFrame = 36;
inline constexpr int kEntrySettleAnchorFp = 1972969;
inline constexpr int kPlayerRamAnchorOffsetX = 32;
inline constexpr int kPlayerRamAnchorOffsetY = 37;
inline constexpr int kEntryCameraStepPx = 2;
inline constexpr int kEntryCameraLockTick = 116;
inline constexpr int kShutterSealCueTick = 117;
// Source PNGs sf0015324..sf0015336 plus door_boundary.json: the leaf is absent
// through t134, then advances through five source-raster checkpoints on even
// ticks t136..t144 and holds the final state thereafter.
inline constexpr int kShutterFirstVisibleTick = 136;
inline constexpr int kShutterFrameCount = 5;
inline constexpr int kShutterHeightPx = 48;
// CP-ENTRY-V2, source player.csv t84..t409: $7E1E51 is bottom-anchored at
// 513 for all 326 rows. The authenticated source-to-engine camera bridge uses
// a 256-pixel origin bias (ms3_intro_highway_route_anchor_scout.py).
inline constexpr int kSourceCameraBottomY = 513;
inline constexpr int kSourceToEngineCameraBiasPx = 256;
// Boss slot Y is already in the source object's bottom-camera basis. With the
// engine's top-camera basis, its sheet-cell top needs this render-only bridge:
// source screenY = slotY - 513 + 224 = slotY - (257 + 32).
inline constexpr int kBossRenderYBiasPx = -32;
inline constexpr int kEntryCameraYPx =
    kSourceCameraBottomY - kSourceToEngineCameraBiasPx;
// Source t136: X's boot raster ends at screen y191, immediately above the
// repeating floor strip that starts at y192. The general player sprite anchor
// needs this additional room-local lift after visual grounding is suppressed.
inline constexpr float kEntryPlayerRenderLiftPx = 2.0f;
inline constexpr float kFpPerPx = 256.0f;

// Offset of the ceremonial walk from the settle value at sourceTick
// (negative during the walk, exactly 0 from t117 on).
inline float entryWalkOffsetPx(int sourceTick) {
    const int t = std::clamp(sourceTick, kSourceSpawnTick,
                             kEntryWalkLastAdvanceTick);
    return static_cast<float>((t - kEntryWalkLastAdvanceTick) *
                              kEntryWalkStepFp) / kFpPerPx;
}

// Absolute source-authenticated X position. The dense player trace stores the
// RAM anchor at sprite position+32; t84..t117 is the exact 116-fp ladder.
inline float entryPlayerPositionX(int sourceTick) {
    const int t = std::clamp(sourceTick, kSourceSpawnTick,
                             kEntryWalkLastAdvanceTick);
    const int anchorFp = kEntrySettleAnchorFp +
        (t - kEntryWalkLastAdvanceTick) * kEntryWalkStepFp;
    return static_cast<float>(anchorFp) / kFpPerPx -
        static_cast<float>(kPlayerRamAnchorOffsetX);
}

// Absolute source-authenticated Y position. X holds the corridor floor through
// t120, drops through the open shutter, and reaches the arena floor at t133.
inline float entryPlayerPositionY(int sourceTick) {
    constexpr int kDescentAnchorY[] = {
        400, 401, 402, 404, 406, 408, 410,
        412, 415, 418, 421, 425, 431,
    };
    int anchorY = 399;
    if (sourceTick >= 121 && sourceTick <= kEntrySettleTick) {
        anchorY = kDescentAnchorY[sourceTick - 121];
    } else if (sourceTick > kEntrySettleTick) {
        anchorY = 431;
    }
    return static_cast<float>(anchorY - kPlayerRamAnchorOffsetY);
}

inline constexpr int entryPlayerPoseFrame(int sourceTick) {
    return sourceTick >= kEntryPlayerFallPoseFirstTick &&
                   sourceTick <= kEntryPlayerFallPoseLastTick
        ? kEntryPlayerFallPoseFrame
        : -1;
}

// Offset of the entry camera pan from the lock value at sourceTick
// (negative during the pan, exactly 0 from t116 on).
inline float entryCameraOffsetPx(int sourceTick) {
    const int t = std::clamp(sourceTick, kSourceSpawnTick,
                             kEntryCameraLockTick);
    return static_cast<float>((t - kEntryCameraLockTick) * kEntryCameraStepPx);
}

inline constexpr bool entryShutterPassable(int sourceTick) {
    return sourceTick <= kEntryCameraLockTick;
}

inline constexpr int entryShutterFrame(int sourceTick) {
    if (sourceTick < kShutterFirstVisibleTick) return -1;
    return std::clamp((sourceTick - kShutterFirstVisibleTick) / 2,
                      0, kShutterFrameCount - 1);
}

// CP-ENTRY-V3, source frames sf0015456..sf0015532: the object is live at
// t84 but has no visible body until the fall begins. Frame 7 is the measured
// tucked fall pose; frame 3 is the landing crouch; the normal idle toggle
// resumes with the HP-fill clock.
inline constexpr int entryBossPoseFrame(int sourceTick) {
    if (sourceTick < kFirstVisibleBodyTick) return -1;
    if (sourceTick <= kLandTick) return 7;
    if (sourceTick < kFillTick) return 3;
    return (sourceTick / 2) & 1;
}

inline Frame evaluate(int sourceTick,
                      float ceilingY,
                      float floorY,
                      int maxHealth) {
    Frame frame;
    if (sourceTick < kFallTick) {
        frame.y = ceilingY;
    } else if (sourceTick <= kLandTick) {
        const int fallFrame = sourceTick - kFallTick;
        const float offset =
            static_cast<float>(fallFrame * (fallFrame + 1)) / 8.0f;
        frame.y = std::min(floorY, ceilingY + offset);
    } else {
        frame.y = floorY;
    }

    if (sourceTick < kFillTick) {
        frame.displayHealth = 0;
    } else if (sourceTick < kFullTick) {
        frame.displayHealth = 1 + (sourceTick - kFillTick) / 2;
    } else {
        frame.displayHealth = maxHealth;
    }
    frame.emitFillApu =
        sourceTick >= kFillTick && sourceTick <= kLastFillApuTick &&
        (sourceTick - kFillTick) % 2 == 0;
    frame.hudVisible = sourceTick >= kHudRevealTick;
    frame.fightOn = sourceTick >= kFightTick;
    if (frame.fightOn) {
        frame.y = floorY;
        frame.displayHealth = maxHealth;
        frame.hudVisible = true;
    }
    return frame;
}

} // namespace mmx::boss_cp_intro_timeline
