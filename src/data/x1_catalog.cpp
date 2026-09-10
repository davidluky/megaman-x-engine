// x1_catalog.cpp - defines canonical Mega Man X1 stage, boss, and route data.
// Boundary: catalog facts are static ids; runtime progression lives elsewhere.

#include "data/x1_catalog.h"

namespace mmx {
namespace x1_catalog {

const std::vector<MaverickBoss>& maverickBosses() {
    static const std::vector<MaverickBoss> bosses = {
        {
            StageId::fromString("chill-penguin"),
            BossId::fromString("chill-penguin"),
            WeaponId::fromString("shotgun-ice"),
            WeaponId::fromString("fire-wave"),
            "CHILL PENGUIN",
        },
        {
            StageId::fromString("storm-eagle"),
            BossId::fromString("storm-eagle"),
            WeaponId::fromString("storm-tornado"),
            WeaponId::fromString("chameleon-sting"),
            "STORM EAGLE",
        },
        {
            StageId::fromString("flame-mammoth"),
            BossId::fromString("flame-mammoth"),
            WeaponId::fromString("fire-wave"),
            WeaponId::fromString("storm-tornado"),
            "FLAME MAMMOTH",
        },
        {
            StageId::fromString("spark-mandrill"),
            BossId::fromString("spark-mandrill"),
            WeaponId::fromString("electric-spark"),
            WeaponId::fromString("shotgun-ice"),
            "SPARK MANDRILL",
        },
        {
            StageId::fromString("armored-armadillo"),
            BossId::fromString("armored-armadillo"),
            WeaponId::fromString("rolling-shield"),
            WeaponId::fromString("electric-spark"),
            "ARMORED ARMADILLO",
        },
        {
            StageId::fromString("launch-octopus"),
            BossId::fromString("launch-octopus"),
            WeaponId::fromString("homing-torpedo"),
            WeaponId::fromString("rolling-shield"),
            "LAUNCH OCTOPUS",
        },
        {
            StageId::fromString("boomer-kuwanger"),
            BossId::fromString("boomer-kuwanger"),
            WeaponId::fromString("boomerang-cutter"),
            WeaponId::fromString("homing-torpedo"),
            "BOOMER KUWANGER",
        },
        {
            StageId::fromString("sting-chameleon"),
            BossId::fromString("sting-chameleon"),
            WeaponId::fromString("chameleon-sting"),
            WeaponId::fromString("boomerang-cutter"),
            "STING CHAMELEON",
        },
    };
    return bosses;
}

const MaverickBoss* findMaverickByBoss(BossId bossId) {
    for (const auto& boss : maverickBosses()) {
        if (boss.boss == bossId) return &boss;
    }
    return nullptr;
}

const MaverickBoss* findMaverickByStage(StageId stageId) {
    for (const auto& boss : maverickBosses()) {
        if (boss.stage == stageId) return &boss;
    }
    return nullptr;
}

const std::vector<StageSelectRouteEntry>& maverickStageSelectRoute() {
    static const std::vector<StageSelectRouteEntry> route = {
        // Original MMX1 stage-select metadata. Hotspots are measured from the
        // cursor_XX -> cursor_XX_blink diffs and cover the real portrait
        // panels, not the four decorative corner plates.
        {StageId::fromString("chill-penguin"), 1, 7, "CHILL PENGUIN", 8, 2, 0, 130.0f, 18.0f, 44.0f, 44.0f, "cursor_08"},
        {StageId::fromString("storm-eagle"), 2, 0, "STORM EAGLE", 5, 0, 2, 34.0f, 112.0f, 46.0f, 46.0f, "cursor_05"},
        // R310: source cursor images and in-stage R256/R270 confirm these
        // identities; the legacy layout used their names in reverse.
        {StageId::fromString("flame-mammoth"), 3, 1, "FLAME MAMMOTH", 4, 3, 1, 177.0f, 64.0f, 46.0f, 46.0f, "cursor_04"},
        {StageId::fromString("spark-mandrill"), 4, 5, "SPARK MANDRILL", 6, 1, 3, 82.0f, 160.0f, 44.0f, 45.0f, "cursor_06"},
        {StageId::fromString("armored-armadillo"), 5, 6, "ARMORED ARMADILLO", 3, 0, 1, 34.0f, 64.0f, 46.0f, 46.0f, "cursor_03"},
        {StageId::fromString("launch-octopus"), 6, 3, "LAUNCH OCTOPUS", 1, 1, 0, 82.0f, 18.0f, 44.0f, 44.0f, "cursor_01"},
        {StageId::fromString("boomer-kuwanger"), 7, 2, "BOOMER KUWANGER", 7, 3, 2, 177.0f, 112.0f, 46.0f, 46.0f, "cursor_07"},
        {StageId::fromString("sting-chameleon"), 8, 8, "STING CHAMELEON", 2, 2, 3, 130.0f, 160.0f, 44.0f, 45.0f, "cursor_02"},
    };
    return route;
}

const std::vector<StageSelectRouteEntry>& sigmaFortressRoute() {
    static const std::vector<StageSelectRouteEntry> route = {
        {StageId::fromString("sigma-1"), 9, 0, "SIGMA 1"},
        {StageId::fromString("sigma-2"), 10, 2, "SIGMA 2"},
        {StageId::fromString("sigma-3"), 11, 6, "SIGMA 3"},
        {StageId::fromString("sigma-4"), 12, 8, "SIGMA 4"},
    };
    return route;
}

} // namespace x1_catalog
} // namespace mmx
