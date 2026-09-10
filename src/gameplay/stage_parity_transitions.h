// stage_parity_transitions.h - declares stage transition parity trace helpers.
// Boundary: observation API only; does not decide scene transitions.

#pragma once

#include "data/game_ids.h"

namespace mmx::stage_parity_transitions {

struct DetectorInput {
    StageId stageId;
    float playerX = 0.0f;
    float playerY = 0.0f;
    float cameraX = 0.0f;
    int tilemapPixelWidth = 0;
    bool stageClearActive = false;
    bool gameOverActive = false;
};

bool shouldSignalBossDoorTransition(const DetectorInput& input);

} // namespace mmx::stage_parity_transitions
