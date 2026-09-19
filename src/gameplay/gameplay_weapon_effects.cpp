// gameplay_weapon_effects.cpp - scene-owned weapon presentation queues.
// Boundary: render/update cosmetics only; damage and collision stay in their lanes.

#include "gameplay/gameplay_scene.h"
#include "app/constants.h"
#include "systems/asset_cache.h"

#include <algorithm>

namespace mmx {

namespace {
void placeGenericContact(gameplay_buster_impact::Impact& impact,
                         const Projectile& projectile,
                         const gameplay_buster_impact::TargetBounds& target,
                         gameplay_buster_impact::PriorityOcclusionProfile profile) {
    if (profile != gameplay_buster_impact::PriorityOcclusionProfile::None) return;
    const AABB shot = projectile.getHitbox();
    gameplay_buster_impact::placeAtProjectileContact(impact,
        {shot.x, shot.y, shot.w, shot.h, projectile.facingRight}, target);
}
} // namespace

void GameplayScene::renderTorpedoSmoke(float camX, float camY) {
    if (torpedoPuffs_.empty()) return;
    if (!torpedoSmokeTex_) {
        torpedoSmokeTex_ = AssetCache::loadTexture(
            "content/x1/sprites/weapons/homing_torpedo_smoke.png");
    }
    if (!torpedoSmokeTex_ || !torpedoSmokeTex_->valid()) return;
    torpedoSmokeTex_->setFilter(TEXTURE_FILTER_POINT);
    static const int kCell[7] = {0, 1, 1, 2, 2, 3, 3};
    for (const auto& s : torpedoPuffs_) {
        const int cell = kCell[s.age < 0 ? 0 : (s.age > 6 ? 6 : s.age)];
        const Rectangle src = {static_cast<float>(cell * 8), 0, 8, 8};
        DrawTextureRec(torpedoSmokeTex_->get(), src,
                       {s.x - 4.0f - camX, s.y - 4.0f - camY}, WHITE);
    }
}

void GameplayScene::updateBusterImpacts() {
    gameplay_buster_impact::updateBeforeCollisions(busterImpacts_);
}

void GameplayScene::spawnBusterImpact(const Projectile& projectile,
                                      int targetSerial) {
    busterImpacts_.push_back(gameplay_buster_impact::spawn({
        projectile.position.x,
        projectile.position.y,
        projectile.hitboxSize.x,
        projectile.hitboxSize.y,
        projectile.facingRight,
    }, projectile.serial, targetSerial));
}

void GameplayScene::dispatchNormalBusterContactEffects(
    const Projectile& projectile, int targetSerial, const AABB& targetBox,
    bool damagedEnemy, bool killedEnemy) {
    const bool normalShot = projectile.type == ProjectileType::Normal;
    if (gameplay_buster_impact::isNormalBusterContactPreludeEligible({
            normalShot, projectile.weaponId, damagedEnemy, killedEnemy}))
        spawnNormalBusterContactPrelude(
            projectile, targetSerial, targetBox, killedEnemy);
    if (gameplay_buster_impact::isNormalBusterSurvivorContactResidueEligible({
            normalShot, projectile.weaponId, damagedEnemy, killedEnemy}))
        spawnNormalBusterSurvivorContactResidue(
            projectile, targetSerial, targetBox);
    if (gameplay_buster_impact::isNormalBusterLethalContactResidueEligible({
            normalShot, projectile.weaponId, damagedEnemy, killedEnemy}))
        spawnNormalBusterLethalContactResidue(
            projectile, targetSerial, targetBox);
}

void GameplayScene::spawnNormalBusterContactPrelude(
    const Projectile& projectile, int targetSerial, const AABB& targetBox,
    bool killedEnemy) {
    const gameplay_buster_impact::TargetBounds target{
        targetBox.x,
        targetBox.y,
        targetBox.w,
        targetBox.h,
    };
    const auto branch = killedEnemy
        ? gameplay_buster_impact::NormalBusterContactBranch::Killed
        : gameplay_buster_impact::NormalBusterContactBranch::Survived;
    auto impact = gameplay_buster_impact::spawnNormalBusterContactPrelude(
        target, branch, projectile.serial, targetSerial);
    placeGenericContact(impact, projectile, target,
        gameplay_buster_impact::sourcePriorityOcclusionProfile(
            activeStageId_.str(), activeVisualSectionId_, target));
    busterImpacts_.push_back(impact);
}

void GameplayScene::spawnNormalBusterLethalContactResidue(
    const Projectile& projectile, int targetSerial, const AABB& targetBox) {
    const gameplay_buster_impact::TargetBounds target{
        targetBox.x,
        targetBox.y,
        targetBox.w,
        targetBox.h,
    };
    const auto priorityOcclusion =
        gameplay_buster_impact::sourcePriorityOcclusionProfile(
            activeStageId_.str(), activeVisualSectionId_, target);
    auto impact = gameplay_buster_impact::spawnNormalBusterLethalContactResidue(
        target, projectile.serial, targetSerial, priorityOcclusion);
    placeGenericContact(impact, projectile, target, priorityOcclusion);
    busterImpacts_.push_back(impact);
}

void GameplayScene::spawnNormalBusterSurvivorContactResidue(
    const Projectile& projectile, int targetSerial, const AABB& targetBox) {
    const gameplay_buster_impact::TargetBounds target{
        targetBox.x,
        targetBox.y,
        targetBox.w,
        targetBox.h,
    };
    const auto priorityOcclusion =
        gameplay_buster_impact::sourcePriorityOcclusionProfile(
            activeStageId_.str(), activeVisualSectionId_, target);
    auto impact = gameplay_buster_impact::spawnNormalBusterSurvivorContactResidue(
        target, projectile.serial, targetSerial, priorityOcclusion);
    placeGenericContact(impact, projectile, target, priorityOcclusion);
    busterImpacts_.push_back(impact);
}

void GameplayScene::spawnTableWeaponContactImpact(
    const Projectile& projectile, int targetSerial, const AABB& targetBox) {
    // T1.2a.2 step 4: the pinned special-weapon contact constants of
    // knowledge_base/mmx1/weapons/impact_contact_law_2026-09-15.json (today,
    // Shotgun Ice alone). Weapons absent from the table spawn nothing.
    const auto constants =
        gameplay_buster_impact::tableWeaponContact(projectile.weaponId);
    if (!constants.has_value()) return;
    const AABB shot = projectile.getHitbox();
    busterImpacts_.push_back(gameplay_buster_impact::spawnTableWeaponContact(
        {shot.x, shot.y, shot.w, shot.h, projectile.facingRight},
        {targetBox.x, targetBox.y, targetBox.w, targetBox.h}, *constants,
        projectile.serial, targetSerial));
}

void GameplayScene::renderBusterImpacts(float camX, float camY) {
    if (busterImpacts_.empty()) return;

    for (const auto& impact : busterImpacts_) {
        const bool normalBusterContact =
            impact.kind == gameplay_buster_impact::
                               ImpactKind::NormalBusterContactPrelude ||
            impact.kind == gameplay_buster_impact::
                               ImpactKind::NormalBusterLethalContactResidue ||
            impact.kind == gameplay_buster_impact::
                               ImpactKind::NormalBusterSurvivorContactResidue ||
            impact.kind ==
                gameplay_buster_impact::ImpactKind::TableWeaponContact;
        const TextureResource*& texture = normalBusterContact
            ? busterNormalContactResidueTex_
            : busterImpactTex_;
        if (!texture) {
            texture = AssetCache::loadTexture(
                normalBusterContact
                    ? "content/x1/sprites/weapons/buster_normal_lethal_contact_residue.png"
                    : "content/x1/sprites/weapons/buster_l1_nonlethal_impact.png");
        }
        if (!texture || !texture->valid()) continue;
        texture->setFilter(TEXTURE_FILTER_POINT);

        const auto draw = gameplay_buster_impact::drawFor(impact, camX, camY);
        if (!draw.has_value()) continue;
        Rectangle source = {
            static_cast<float>(draw->sourceX),
            static_cast<float>(draw->sourceY),
            static_cast<float>(draw->sourceWidth),
            static_cast<float>(draw->sourceHeight),
        };
        if (draw->mirror) source.width = -source.width;
        const auto drawRawAtlasCell = [&]() {
            DrawTextureRec(
                texture->get(), source,
                {draw->topLeftX, draw->topLeftY}, WHITE);
        };
        if (!draw->sourceSnowPriorityClip.has_value()) {
            drawRawAtlasCell();
            continue;
        }

        const auto& clip = *draw->sourceSnowPriorityClip;
        const auto drawInScissor = [&](int x, int y, int width, int height) {
            const int left = std::clamp(x, 0, INTERNAL_WIDTH);
            const int top = std::clamp(y, 0, INTERNAL_HEIGHT);
            const int right = std::clamp(x + width, 0, INTERNAL_WIDTH);
            const int bottom = std::clamp(y + height, 0, INTERNAL_HEIGHT);
            if (right <= left || bottom <= top) return;
            BeginScissorMode(left, top, right - left, bottom - top);
            drawRawAtlasCell();
            EndScissorMode();
        };
        drawInScissor(0, 0, INTERNAL_WIDTH, clip.fullRowsBottomYExclusive);
        drawInScissor(
            0, clip.fullRowsBottomYExclusive,
            clip.firstEdgeRightXExclusive, 1);
        drawInScissor(
            0, clip.fullRowsBottomYExclusive + 1,
            clip.secondEdgeRightXExclusive, 1);
        if (clip.hasSinglePixelAperture) {
            drawInScissor(
                clip.singlePixelApertureX,
                clip.singlePixelApertureY,
                1, 1);
        }
    }
}

} // namespace mmx
