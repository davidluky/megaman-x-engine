// gameplay_death_orbs.h - declares player death-orb burst helpers.
// Boundary: death presentation only; player life rules stay with player flow.

#pragma once

#include <optional>
#include <vector>

namespace mmx {
class TextureResource;
}

namespace mmx::gameplay_death_orbs {

struct Orb {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    int animFrame = 0;
    int animTimer = 0;
    int lifetime = 90;
};

struct SpriteMetadata {
    int sheetX = 0;
    int sheetY = 490;
    int frameWidth = 16;
    int frameHeight = 16;
    int frameCount = 5;
};

struct State {
    std::vector<Orb> orbs;
    const TextureResource* sheet = nullptr;
    SpriteMetadata spriteMeta;
    int previousDeathTimer = -1;
};

struct DeathCameraTransition {
    bool suppressClamp = false;
    float targetX = 0.0f;
    float targetY = 0.0f;
};

struct UpdateInput {
    bool dead = false;
    int deathTimer = -1;
    float cameraTargetX = 0.0f;
    float cameraTargetY = 0.0f;
    float orbCenterX = 0.0f;
    float orbCenterY = 0.0f;
};

struct UpdateResult {
    std::optional<DeathCameraTransition> cameraTransition;
};

UpdateInput makeUpdateInput(bool dead,
                            int deathTimer,
                            float cameraTargetX,
                            float cameraTargetY,
                            float orbCenterX,
                            float orbCenterY);

const std::vector<Orb>& traceOrbs(const State& state);
void clearLiveOrbs(State& state);
void clearBorrowedSpriteSheet(State& state);
UpdateResult updateState(State& state, const UpdateInput& input);

} // namespace mmx::gameplay_death_orbs
