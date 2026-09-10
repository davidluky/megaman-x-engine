// x1_catalog.h - declares static Mega Man X1 boss and stage-select catalogs.
// Boundary: exposes canonical ids and layout facts without loading content.

#pragma once

#include "data/game_ids.h"
#include <vector>

namespace mmx {
namespace x1_catalog {

struct MaverickBoss {
    StageId stage;
    BossId boss;
    WeaponId rewardWeapon;
    WeaponId weaknessWeapon;
    const char* displayName;
};

struct StageSelectRouteModel {
    StageId stage;
    int order;
    int gridPosition;
    const char* displayName;
    int originalCursorId = -1;
    int ringX = -1;
    int ringY = -1;
    float hotspotX = 0.0f;
    float hotspotY = 0.0f;
    float hotspotW = 0.0f;
    float hotspotH = 0.0f;
    const char* screenshotId = "";
};

using StageSelectRouteEntry = StageSelectRouteModel;

const std::vector<MaverickBoss>& maverickBosses();
const MaverickBoss* findMaverickByBoss(BossId bossId);
const MaverickBoss* findMaverickByStage(StageId stageId);
const std::vector<StageSelectRouteEntry>& maverickStageSelectRoute();
const std::vector<StageSelectRouteEntry>& sigmaFortressRoute();

} // namespace x1_catalog
} // namespace mmx
