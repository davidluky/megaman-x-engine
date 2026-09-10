// stage_object.h - declares stage object roles, metadata, and runtime entity.
// Owns: stage object source IDs, blocking roles, and definition contracts.

#pragma once

#include "entities/entity.h"
#include "systems/tilemap.h"
#include <string>
#include <string_view>

namespace mmx {

enum class StageObjectRole {
    BossDoor,
    Hazard
};

struct StageObjectDefinition {
    std::string id;
    int sourceOid = -1;
    StageObjectRole role = StageObjectRole::Hazard;
    int contactDamage = 0;
    Vector2 hitboxSize = {16.0f, 16.0f};
    Vector2 hitboxOffset = {0.0f, 0.0f};
    bool damageableByPlayerShots = false;
    bool blocksEnemyDefinitionPromotion = true;
    std::string spritePath;
    std::string visualEvidencePath;
    std::string contactEvidencePath;
    std::string routingEvidencePath;
    int frameWidth = 0;
    int frameHeight = 0;
    int frameCount = 1;
    int frameTicks = 0;
};

class StageObject : public Entity {
public:
    static const StageObjectDefinition* findDefinition(std::string_view id);
    static const StageObjectDefinition* findDefinitionBySourceOid(int sourceOid);
    static bool isStageObjectSpawn(const SpawnPoint& spawn);
    static bool blocksEnemyDefinitionPromotionForSourceOid(int sourceOid);

    void init(std::string_view id, float x, float y);
    void update(float dt) override;
    void render(float alpha) override;
    void render(float alpha, Vector2 cameraOffset);

    const std::string& id() const { return id_; }
    int sourceOid() const { return definition_ ? definition_->sourceOid : -1; }
    int contactDamage() const { return definition_ ? definition_->contactDamage : 0; }
    bool canBeDamagedByPlayerShots() const {
        return definition_ && definition_->damageableByPlayerShots;
    }
    int animationFrameIndexForTest() const { return frameIndex_; }

    // Non-enemy objects intentionally ignore player projectile damage unless
    // a future evidence-backed definition opts in.
    bool applyPlayerShotDamage(int damage);

private:
    std::string id_;
    const StageObjectDefinition* definition_ = nullptr;
    int animTick_ = 0;
    int frameIndex_ = 0;
};

} // namespace mmx
