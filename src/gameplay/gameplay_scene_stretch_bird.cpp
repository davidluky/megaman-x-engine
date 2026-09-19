// OID04/child OID03: head_launch.json and head_projectile_aim.json.
#include "gameplay/gameplay_scene.h"
#include "gameplay/gameplay_enemy_contact.h"
#include "entities/player_anchor.h"
#include "systems/asset_cache.h"
#include "systems/audio.h"

namespace mmx {

void GameplayScene::allocateStretchBirdShot(const Enemy& enemy) {
    using namespace gameplay_enemy_contact;
    const int index = stretchBirdShots_.allocate(stretch_bird_shot::Parent{
        sourceWord(enemy.position.x), sourceWord(enemy.position.y), 0,
        // OAM 2F is the captured normal palette/priority; bit6 is orientation.
        static_cast<std::uint8_t>(enemy.facingRight ? 0x2F : 0x6F), !enemy.facingRight});
    if (index < 0) return;
    stretchBirdShotParents_[index] = enemy.serial;
    stretchBirdShotAnimation_[index] = 0;
}

void GameplayScene::updateStretchBirdShots() {
    using namespace gameplay_enemy_contact;
    namespace shot = stretch_bird_shot;
    bool hasChildren = false;
    for (const auto& child : stretchBirdShots_.slots) hasChildren |= child.active;
    if (hasChildren) {
        // 80:D227 updates Player bullets before 80:D241's enemy-shot pool.
        // Only birth-authenticated normal Buster opts in; the generic lane
        // consumes this flag instead of advancing it a second time.
        for (auto& projectile : projectiles_) {
            if (!projectile.active || !projectile.isPlayerShot ||
                !projectile.sourceCollisionCenterOffset || projectile.advancedForEnemyShotPhase ||
                projectile.weaponId != "buster" || projectile.type != ProjectileType::Normal) continue;
            projectile.update(0);
            projectile.advancedForEnemyShotPhase = true;
        }
    }
    const shot::Environment environment{
        sourceWord(player_anchor::sourceRamAnchorX(
            player_.position.x, player_.spriteWidth, player_.facingRight)),
        sourceWord(player_.position.y + 40.0f),
        sourceWord(camera_.baseX()), sourceWord(camera_.baseY()),
        shot::ResolvedCollision::None};
    for (std::size_t index = 0; index < stretchBirdShots_.slots.size(); ++index) {
        auto& child = stretchBirdShots_.slots[index];
        const bool wasLive = child.active && child.action == 2;
        const auto result = stretchBirdShots_.step(child, environment);
        if (!wasLive) continue; // birth initialization and terminal clear do not collide
        ++stretchBirdShotAnimation_[index];
        if (result.rangeExpired) continue; // source AB8F returns before contact
        // Camera cull does not return early: ABEC returns into AB70's contact lane.
        const auto profile = playerProfile(player_);
        bool signal = false;
        if (!player_.isDead() && !player_.isInvulnerable() && profile &&
            characterPath == "content/x1/characters/x.json") {
            const auto playerX = static_cast<std::uint16_t>(environment.playerX + profile->offsetX);
            const auto playerY = static_cast<std::uint16_t>(environment.playerY + profile->offsetY);
            signal = inclusiveAxisOverlap(sourceWordDistance(playerX, shot::word(child.xFp)), profile->halfExtentX, 8)
                && inclusiveAxisOverlap(sourceWordDistance(playerY, shot::word(child.yFp)), profile->halfExtentY, 8);
            if (signal && !consumeAbsorbShield()) {
                const float direction = playerX < shot::word(child.xFp) ? -1.0f : 1.0f;
                player_.takeDamage(child.damage, direction);
                camera_.shake(2.0f, 8);
            }
        }
        if (!signal) {
            child.hp &= 0x7F; // 849B43 clears the previous hit marker before scanning.
            for (auto& projectile : projectiles_) {
                if (!projectile.active || !projectile.isPlayerShot ||
                    !projectile.sourceCollisionCenterOffset || projectile.dormantFrames > 0 ||
                    projectile.weaponId != "buster" || projectile.type != ProjectileType::Normal) continue;
                const auto offset = *projectile.sourceCollisionCenterOffset;
                const auto px = sourceWord(projectile.position.x + offset.x);
                const auto py = sourceWord(projectile.position.y + offset.y);
                if (!inclusiveAxisOverlap(sourceWordDistance(px,shot::word(child.xFp)),7,8) ||
                    !inclusiveAxisOverlap(sourceWordDistance(py,shot::word(child.yFp)),7,8)) continue;
                // Child D+28=03; weaponDamage_03 normal entry is exactly 1.
                // First accepted projectile returns immediately; no target iframe is set.
                if (child.hp > 0) --child.hp;
                signal = child.hp == 0;
                child.hp |= 0x80; // 849EA5 marks both lethal and surviving hits.
                if (!signal) {
                    child.attr11 &= static_cast<std::uint8_t>(~0x0E); // 81:ABB7 TRB0E
                    AudioManager::playApu(0x11); // 849E7C normal nonlethal hit
                }
                projectile.active = false; // source normal bullet enters impact/terminal state
                break;
            }
        }
        if (!signal) continue;
        child.action = 4;
        child.subaction = 0;
        for (auto& parent : enemies_) {
            if (parent.serial == stretchBirdShotParents_[index]) {
                parent.signalStretchBirdChild();
                break;
            }
        }
    }
}

void GameplayScene::renderStretchBirdShots(float cameraX, float cameraY) {
    bool visible = false;
    for (const auto& child : stretchBirdShots_.slots)
        visible |= child.active && child.action != 0;
    if (!visible) return;
    const auto* texture = AssetCache::loadTexture("content/x1/sprites/enemies/stretch_bird_shot.png");
    if (!texture || !texture->valid()) return;
    constexpr int frames[] = {0,2,1,2}; // source stream 0: 80,82,81,82
    for (std::size_t index = 0; index < stretchBirdShots_.slots.size(); ++index) {
        const auto& child = stretchBirdShots_.slots[index];
        if (!child.active || child.action == 0) continue;
        const int frame = frames[stretchBirdShotAnimation_[index] % 4];
        const auto* drawn = (child.attr11 & 0x0E) == 0
            ? AssetCache::loadTexture("content/x1/sprites/enemies/stretch_bird_shot_flash.png") : texture;
        if (!drawn || !drawn->valid()) drawn = texture;
        DrawTextureRec(drawn->get(), {static_cast<float>(frame * 15),0,15,15},
            {static_cast<float>(stretch_bird_shot::word(child.xFp)) - 7 - cameraX,
             static_cast<float>(stretch_bird_shot::word(child.yFp)) - 7 - cameraY}, WHITE);
    }
}

} // namespace mmx
