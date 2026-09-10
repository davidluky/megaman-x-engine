// gameplay_trace_types.h - defines lightweight trace value types for gameplay.
// Boundary: shared trace types only; no scene state ownership.

#pragma once

#include "data/game_ids.h"

#include <string>

namespace mmx::gameplay_trace {

struct CpCapsuleTraceState {
    int sourceFrame = 0;
    int animByte = -1;
    bool active = false;
    bool grantEvent = false;
    bool releaseEvent = false;
    bool armorBoots = false;
};

struct ParityFrameState {
    const std::string& activeCameraSectionId;
    const StageId& activeStageId;
    bool bossLocked = false;
    bool stageClear = false;
    int stageClearTimer = 0;
    bool gameOver = false;
    bool paused = false;
    int transitionState = 0;
    int transitionTimer = 0;
    bool returnToStageSelect = false;
    bool returnToTitle = false;
};

} // namespace mmx::gameplay_trace
