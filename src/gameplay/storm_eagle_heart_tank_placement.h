// storm_eagle_heart_tank_placement.h - source-backed GC8 placement facts.
//
// The source artifact measures the drawn sprite's world extent and camera
// conversion. David approved its stable centre as the protected stage anchor
// on 2026-08-09; the helpers below bridge that centre to the runtime atlas.
//
// Oracle: knowledge_base/mmx1/stages/storm_eagle/heart_tank_placement.json
#pragma once

#include <array>

namespace mmx::storm_eagle_heart_tank_placement {

struct Point {
    int x = 0;
    int y = 0;
};

struct RuntimeTopLeft {
    float x = 0.0f;
    float y = 0.0f;
};

struct Extent {
    int x0 = 0;
    int x1 = 0;
    int y0 = 0;
    int y1 = 0;
};

struct CameraCalibration {
    int camX = 0;
    int camY = 0;
    bool camXConstant = false;
    bool camYConstant = false;
};

struct AnimationPhase {
    int frames = 0;
    Extent screen;
    Extent world;
};

struct CollectionEvidence {
    int frame = 0;
    int heartBit = 0;
    int playerSpriteX0 = 0;
    int playerSpriteX1 = 0;
    int playerWorldX = 0;
    int playerWorldY = 0;
    int spriteOverlapPx = 0;
};

inline constexpr CameraCalibration kMeasuredCamera{0, 1263, true, true};
inline constexpr Extent kMeasuredWorldExtent{25, 38, 1395, 1402};
inline constexpr int kMeasuredSpriteWidth = 14;
inline constexpr int kMeasuredSpriteHeight = 8;
inline constexpr int kMeasuredAnimationPhaseCount = 3;
inline constexpr int kCollectionFrame = 2271;
inline constexpr int kCollectedHeartBit = 0x04;
inline constexpr float kApprovedStageCentreX = 31.5f;
inline constexpr float kApprovedStageCentreY = 1398.5f;
inline constexpr float kRuntimeAtlasFrameSize = 16.0f;

inline constexpr std::array<AnimationPhase, 3> kAnimationPhases = {{
    AnimationPhase{10, {25, 38, 132, 136}, {25, 38, 1395, 1399}},
    AnimationPhase{15, {26, 37, 133, 138}, {26, 37, 1396, 1401}},
    AnimationPhase{12, {27, 36, 134, 139}, {27, 36, 1397, 1402}},
}};

inline constexpr CollectionEvidence kCollectionEvidence{
    kCollectionFrame, kCollectedHeartBit, 32, 61, 44, 1391, 6};

inline constexpr int extentWidth(const Extent& extent) {
    return extent.x1 - extent.x0 + 1;
}

inline constexpr int extentHeight(const Extent& extent) {
    return extent.y1 - extent.y0 + 1;
}

// Twice the centre avoids introducing floating-point placement semantics into
// the evidence model. The measured centre is (63/2, 2797/2).
inline constexpr int centreXTwice(const Extent& extent) {
    return extent.x0 + extent.x1;
}

inline constexpr int centreYTwice(const Extent& extent) {
    return extent.y0 + extent.y1;
}

inline constexpr RuntimeTopLeft runtimeTopLeftFromCentre(float centreX,
                                                         float centreY) {
    return {centreX - kRuntimeAtlasFrameSize / 2.0f,
            centreY - kRuntimeAtlasFrameSize / 2.0f};
}

inline constexpr RuntimeTopLeft runtimeTopLeftFromApprovedCentre() {
    return runtimeTopLeftFromCentre(kApprovedStageCentreX,
                                    kApprovedStageCentreY);
}

inline constexpr Point worldFromScreen(Point screen,
                                       const CameraCalibration& camera) {
    return {screen.x + camera.camX, screen.y + camera.camY};
}

inline constexpr bool horizontalCentresStable() {
    const int centre = centreXTwice(kAnimationPhases.front().world);
    for (const AnimationPhase& phase : kAnimationPhases) {
        if (centreXTwice(phase.world) != centre) return false;
    }
    return true;
}

inline constexpr bool phasesUnionToMeasuredExtent() {
    int x0 = kAnimationPhases.front().world.x0;
    int x1 = kAnimationPhases.front().world.x1;
    int y0 = kAnimationPhases.front().world.y0;
    int y1 = kAnimationPhases.front().world.y1;
    for (const AnimationPhase& phase : kAnimationPhases) {
        if (phase.world.x0 < x0) x0 = phase.world.x0;
        if (phase.world.x1 > x1) x1 = phase.world.x1;
        if (phase.world.y0 < y0) y0 = phase.world.y0;
        if (phase.world.y1 > y1) y1 = phase.world.y1;
    }
    return x0 == kMeasuredWorldExtent.x0 &&
           x1 == kMeasuredWorldExtent.x1 &&
           y0 == kMeasuredWorldExtent.y0 &&
           y1 == kMeasuredWorldExtent.y1;
}

}  // namespace mmx::storm_eagle_heart_tank_placement
