// gameplay_death_orbs.cpp - runs player death-orb burst lifecycle.
// Owns: death-orb spawn timing, positions, and render handoff state.

#include "gameplay/gameplay_death_orbs.h"

#include <algorithm>
#include <cmath>

namespace mmx::gameplay_death_orbs {
namespace {

std::optional<DeathCameraTransition> deathCameraTransition(
        bool dead,
        bool wasDying,
        float targetX,
        float targetY) {
    if (dead && !wasDying) {
        return DeathCameraTransition{true, targetX, targetY};
    }
    if (!dead && wasDying) {
        return DeathCameraTransition{false, targetX, targetY};
    }
    return std::nullopt;
}

void spawnRing(std::vector<Orb>& orbs, float cx, float cy, int count, float phaseDeg) {
    const float speed = 2.5f;
    const float toRad = 3.14159265358979f / 180.0f;
    for (int i = 0; i < count; i++) {
        const float angle = (360.0f / count) * i + phaseDeg;
        const float ang = angle * toRad;
        Orb orb;
        orb.x = cx;
        orb.y = cy;
        orb.vx = std::cos(ang) * speed;
        orb.vy = std::sin(ang) * speed;
        orb.animFrame = 0;
        orb.animTimer = 0;
        orb.lifetime = 60;
        orbs.push_back(orb);
    }
}

void spawnScheduledRing(std::vector<Orb>& orbs, int deathTimer, float cx, float cy) {
    if (deathTimer == 31) {
        spawnRing(orbs, cx, cy, 8, 0.0f);
    } else if (deathTimer == 32) {
        spawnRing(orbs, cx, cy, 8, 360.0f / 16.0f);
    } else if (deathTimer == 64) {
        spawnRing(orbs, cx, cy, 8, 0.0f);
    } else if (deathTimer == 98) {
        spawnRing(orbs, cx, cy, 8, 360.0f / 16.0f);
    } else if (deathTimer == 141) {
        spawnRing(orbs, cx, cy, 8, 0.0f);
    } else if (deathTimer == 184) {
        spawnRing(orbs, cx, cy, 8, 360.0f / 16.0f);
    }
}

void updateScheduledRings(State& state, const UpdateInput& input) {
    if (!input.dead) {
        state.previousDeathTimer = -1;
        return;
    }

    if (input.deathTimer == state.previousDeathTimer) return;

    spawnScheduledRing(state.orbs, input.deathTimer, input.orbCenterX, input.orbCenterY);
    state.previousDeathTimer = input.deathTimer;
}

void tickAndPrune(std::vector<Orb>& orbs, int frameCount) {
    const int ticksPerFrame = 4;
    for (auto& orb : orbs) {
        orb.x += orb.vx;
        orb.y += orb.vy;
        orb.animTimer++;
        if (orb.animTimer >= ticksPerFrame) {
            orb.animTimer = 0;
            orb.animFrame = (orb.animFrame + 1) % frameCount;
        }
        orb.lifetime--;
    }
    orbs.erase(
        std::remove_if(orbs.begin(), orbs.end(),
                       [](const Orb& orb) { return orb.lifetime <= 0; }),
        orbs.end());
}

} // namespace

UpdateInput makeUpdateInput(bool dead,
                            int deathTimer,
                            float cameraTargetX,
                            float cameraTargetY,
                            float orbCenterX,
                            float orbCenterY) {
    return UpdateInput{
        dead,
        dead ? deathTimer : -1,
        cameraTargetX,
        cameraTargetY,
        orbCenterX,
        orbCenterY,
    };
}

const std::vector<Orb>& traceOrbs(const State& state) {
    return state.orbs;
}

void clearLiveOrbs(State& state) {
    state.orbs.clear();
}

void clearBorrowedSpriteSheet(State& state) {
    state.sheet = nullptr;
}

UpdateResult updateState(State& state, const UpdateInput& input) {
    UpdateResult result;
    const bool wasDying = (state.previousDeathTimer >= 0);
    result.cameraTransition = deathCameraTransition(
        input.dead,
        wasDying,
        input.cameraTargetX,
        input.cameraTargetY);

    updateScheduledRings(state, input);
    tickAndPrune(state.orbs, state.spriteMeta.frameCount);
    return result;
}

} // namespace mmx::gameplay_death_orbs
