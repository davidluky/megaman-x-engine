// OID50/child13: oid_0x50_pending/attack_law.json, 83:A652 and 87:D492.
#include "gameplay/gameplay_scene.h"
#include "gameplay/gameplay_enemy_contact.h"
#include "gameplay/gameplay_enemy_death_presentation.h"
#include "systems/asset_cache.h"
#include "systems/audio.h"

namespace mmx {

void GameplayScene::allocateDeckTurretShot(const Enemy& enemy) {
    using gameplay_enemy_contact::sourceWord;
    const auto& state = enemy.deckTurretState();
    deckTurretShots_.allocate_from_parent({sourceWord(enemy.position.x), sourceWord(enemy.position.y),
        state.selector_16, state.attr_11, 0x90, 0x0E});
}

void GameplayScene::updateDeckTurretShots() {
    using namespace gameplay_enemy_contact;
    namespace shot = deck_turret_shot;
    const auto px = sourceWord(player_anchor::sourceRamAnchorX(
        player_.position.x, player_.spriteWidth, player_.facingRight));
    const auto py = sourceWord(player_.position.y + 40.0f);
    const auto profile = playerProfile(player_);
    for (std::size_t index = 0; index < deckTurretShots_.pool().size(); ++index) {
        const auto& child = deckTurretShots_.pool()[index];
        shot::FrameEnvironment environment{false, sourceWord(camera_.baseX()), sourceWord(camera_.baseY())};
        // 83:A67D checks timer expiry before contact; contact uses pre-motion
        // coordinates. New state00 children initialize without contact/movement.
        if (child[0] && child[0x0A] == 0x13 && child[1] == 2 && child[0x39] != 1 &&
            !player_.isDead() && !player_.isInvulnerable() && profile &&
            characterPath == "content/x1/characters/x.json") {
            const auto playerX = static_cast<std::uint16_t>(px + profile->offsetX);
            const auto playerY = static_cast<std::uint16_t>(py + profile->offsetY);
            environment.contact_nonzero =
                inclusiveAxisOverlap(sourceWordDistance(playerX, shot::read16(child, 5)), profile->halfExtentX, 8) &&
                inclusiveAxisOverlap(sourceWordDistance(playerY, shot::read16(child, 8)), profile->halfExtentY, 5);
            if (environment.contact_nonzero && !consumeAbsorbShield()) {
                player_.takeDamage(child[0x26], playerX < shot::read16(child, 5) ? -1.0f : 1.0f);
                camera_.shake(2.0f, 8);
            }
        }
        // D+28=1 deliberately bypasses player-bullet shootdown in 83:A684.
        // All parents share these eight turret records; a full allocator drops
        // the birth while the parent still completes its source cooldown reset.
        const auto result = deckTurretShots_.dispatch(index, environment);
        if (result.effect) {
            // The source clear preserves event XY; FX allocation follows the
            // pre-motion contact/timeout anchor, not the parent's body center.
            deck_turret_effect::allocate(deckTurretEffects_, shot::read16(child,5), shot::read16(child,8));
        }
    }
    // 80:D287 runs after enemy shots. Allocate before advancing so a full
    // pool cannot reuse an effect which only expires in this later pass.
    deck_turret_effect::advance(deckTurretEffects_,sourceWord(camera_.baseX()),sourceWord(camera_.baseY()));
}

void GameplayScene::renderDeckTurretShots(float cameraX, float cameraY) {
    namespace shot = deck_turret_shot;
    const auto* texture = AssetCache::loadTexture("content/x1/sprites/enemies/se_turret_shot.png");
    if (!texture || !texture->valid()) return;
    for (const auto& child : deckTurretShots_.pool()) {
        if (!child[0] || child[0x0A] != 0x13 || child[1] == 0 || !(child[0x0E] & 0x80)) continue;
        const auto art = child[0x17];
        if (art < 0x86 || art > 0x89) continue;
        const bool right = (child[0x11] & 0x40) != 0;
        DrawTextureRec(texture->get(), {static_cast<float>((art - 0x86) * 15), 0, right ? -15.0f : 15.0f, 9},
            {static_cast<float>(shot::read16(child, 5)) - 7 - cameraX,
             static_cast<float>(shot::read16(child, 8)) - 4 - cameraY}, WHITE);
    }
    const auto* effects = AssetCache::loadTexture("content/x1/sprites/effects/enemy_death_explosion.png");
    if (!effects || !effects->valid()) return;
    for (const auto& effect : deckTurretEffects_) {
        const int cell = effect.cell();
        if (cell < 0) continue;
        // attack_effect_art.json: identical source pixels, but the graphic
        // origin is eight pixels above this effect's RAM event anchor.
        const auto& material = gameplay_enemy_death_presentation::kMaterials[cell];
        DrawTextureRec(effects->get(), {float(cell*32),0,32,32},
            {float(effect.x)+material.anchorOffsetX-cameraX,
             float(effect.y)+material.anchorOffsetY-8-cameraY}, WHITE);
    }
}

} // namespace mmx
