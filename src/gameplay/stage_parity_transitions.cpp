// stage_parity_transitions.cpp - records stage transition parity observations.
// Boundary: trace emission only; stage routing remains owned by gameplay flow.

#include "gameplay/stage_parity_transitions.h"

#include <cmath>

namespace mmx::stage_parity_transitions {
namespace {

// U283 source-backed Spark boss-door reset:
// source last-alive f23117: X=7552, Y=698, camera_x=7424.
// Engine player position uses the settled -33px source->engine X offset.
constexpr float kSparkBossDoorEngineX = 7519.0f;
constexpr float kSparkBossDoorCameraX = 7424.0f;
constexpr float kSparkBossDoorYMin = 640.0f;
constexpr float kSparkBossDoorYMax = 760.0f;
constexpr int kSparkBossDoorStageWidth = 7680;

bool shouldSignalSparkBossDoorTransition(const DetectorInput& input) {
    if (input.stageId.str() != "spark-mandrill") return false;
    if (input.stageClearActive || input.gameOverActive) return false;
    if (input.tilemapPixelWidth != kSparkBossDoorStageWidth) return false;
    if (std::abs(input.cameraX - kSparkBossDoorCameraX) > 1.0f) return false;
    return input.playerX >= kSparkBossDoorEngineX - 1.0f &&
           input.playerY >= kSparkBossDoorYMin &&
           input.playerY <= kSparkBossDoorYMax;
}

} // namespace

bool shouldSignalBossDoorTransition(const DetectorInput& input) {
    return shouldSignalSparkBossDoorTransition(input);
}

} // namespace mmx::stage_parity_transitions
