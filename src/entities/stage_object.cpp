// stage_object.cpp - runs static stage-object behavior and rendering.
// Owns: stage object definitions, spawn classification, and damage response.

#include "entities/stage_object.h"
#include "systems/asset_cache.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace mmx {
namespace {

const std::array<StageObjectDefinition, 1> kStageObjectDefinitions = {{
    {
        "boss-door-flame-mammoth",
        0x3d,
        StageObjectRole::BossDoor,
        0,
        {32.0f, 80.0f},
        {0.0f, 0.0f},
        false,
        true,
        "content/x1/sprites/objects/boss_door_flame_mammoth.png",
        "docs/evidence/2026-06-17-boss-door-fm/",
        "",
        "tools/mmx_rom_ripper.py",
        32,
        80,
        4,
        8
    }
}};

bool isObjectSpawnType(std::string_view type) {
    return type == "object" || type == "stage_object" || type == "hazard";
}

} // namespace

const StageObjectDefinition* StageObject::findDefinition(std::string_view id) {
    auto it = std::find_if(
        kStageObjectDefinitions.begin(), kStageObjectDefinitions.end(),
        [&](const StageObjectDefinition& def) { return def.id == id; });
    return it == kStageObjectDefinitions.end() ? nullptr : &(*it);
}

const StageObjectDefinition* StageObject::findDefinitionBySourceOid(int sourceOid) {
    auto it = std::find_if(
        kStageObjectDefinitions.begin(), kStageObjectDefinitions.end(),
        [&](const StageObjectDefinition& def) { return def.sourceOid == sourceOid; });
    return it == kStageObjectDefinitions.end() ? nullptr : &(*it);
}

bool StageObject::isStageObjectSpawn(const SpawnPoint& spawn) {
    return isObjectSpawnType(spawn.type) && findDefinition(spawn.id) != nullptr;
}

bool StageObject::blocksEnemyDefinitionPromotionForSourceOid(int sourceOid) {
    const StageObjectDefinition* def = findDefinitionBySourceOid(sourceOid);
    return def && def->blocksEnemyDefinitionPromotion;
}

void StageObject::init(std::string_view id, float x, float y) {
    definition_ = findDefinition(id);
    if (!definition_) {
        active = false;
        id_.clear();
        return;
    }

    id_ = definition_->id;
    position = {x, y};
    prevPosition = position;
    hitboxOffset = definition_->hitboxOffset;
    hitboxSize = definition_->hitboxSize;
    active = true;
    facingRight = true;
    animTick_ = 0;
    frameIndex_ = 0;

    if (!definition_->spritePath.empty() && IsWindowReady()) {
        setSpriteSheetResource(AssetCache::loadTexture(definition_->spritePath));
    } else {
        setSpriteSheetResource(nullptr);
    }
}

void StageObject::update(float /*dt*/) {
    prevPosition = position;
    if (!definition_ || definition_->frameCount <= 1 || definition_->frameTicks <= 0) {
        return;
    }
    if (++animTick_ >= definition_->frameTicks) {
        animTick_ = 0;
        frameIndex_ = (frameIndex_ + 1) % definition_->frameCount;
    }
}

void StageObject::render(float alpha) {
    render(alpha, {0.0f, 0.0f});
}

void StageObject::render(float alpha, Vector2 cameraOffset) {
    if (!active || !hasSpriteSheet()) return;

    const TextureResource* tex = spriteSheetResource();
    const float drawX = prevPosition.x + (position.x - prevPosition.x) * alpha - cameraOffset.x;
    const float drawY = prevPosition.y + (position.y - prevPosition.y) * alpha - cameraOffset.y;
    const int defFrameW = definition_ ? definition_->frameWidth : 0;
    const int defFrameH = definition_ ? definition_->frameHeight : 0;
    const float frameW = static_cast<float>(defFrameW > 0 ? defFrameW : tex->get().width);
    const float frameH = static_cast<float>(defFrameH > 0 ? defFrameH : tex->get().height);
    Rectangle src{
        frameW * static_cast<float>(frameIndex_),
        0.0f,
        frameW,
        frameH
    };
    Rectangle dst{
        std::round(drawX),
        std::round(drawY),
        src.width,
        src.height
    };
    DrawTexturePro(tex->get(), src, dst, {0.0f, 0.0f}, 0.0f, WHITE);
}

bool StageObject::applyPlayerShotDamage(int /*damage*/) {
    return canBeDamagedByPlayerShots();
}

} // namespace mmx
