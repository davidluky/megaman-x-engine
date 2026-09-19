// OID53/child23: oid_0x53_mad_pecker/attack_child_law.json, 87:8968.
#include "gameplay/gameplay_scene.h"
#include "gameplay/gameplay_enemy_contact.h"
#include "systems/asset_cache.h"
#include "systems/audio.h"

namespace mmx {
void GameplayScene::allocateMadPeckerShot(const Enemy& enemy) {
    using gameplay_enemy_contact::sourceWord;
    const auto& state=enemy.madPeckerState();
    madPeckerShots_.allocate_from_parent({sourceWord(enemy.position.x),sourceWord(enemy.position.y),
        static_cast<std::uint16_t>(state.speedX1A),static_cast<std::uint16_t>(state.speedY1C),
        0,state.attr11,0,0x88});
    // gfx slot and palette are represented by the retained native atlas;
    // no level-specific WRAM graphics-allocation index is invented here.
}

void GameplayScene::updateMadPeckerShots() {
    using namespace gameplay_enemy_contact;
    namespace shot=mad_pecker_shot;
    const auto px=sourceWord(player_anchor::sourceRamAnchorX(
        player_.position.x,player_.spriteWidth,player_.facingRight));
    const auto py=sourceWord(player_.position.y+40.0f);
    const auto profile=playerProfile(player_);
    for (std::size_t i=0;i<madPeckerShots_.pool().size();++i) {
        const auto& child=madPeckerShots_.pool()[i];
        // 87:89A2 decrements lifetime before calling 84:9B03. C3A2 is
        // centered at child (0,-16), inclusive half extents (5,5).
        if (child[0] && child[0x0A]==0x23 && child[1]==2 && child[0x38]!=1 &&
            child[0x27] && (child[0x0E]&0x7F) && profile &&
            !player_.isDead() && !player_.isInvulnerable() &&
            characterPath=="content/x1/characters/x.json") {
            const auto x=static_cast<std::uint16_t>(px+profile->offsetX);
            const auto y=static_cast<std::uint16_t>(py+profile->offsetY);
            const auto cx=shot::word(child,5);
            const auto cy=static_cast<std::uint16_t>(shot::word(child,8)-16);
            if (inclusiveAxisOverlap(sourceWordDistance(x,cx),profile->halfExtentX,5) &&
                inclusiveAxisOverlap(sourceWordDistance(y,cy),profile->halfExtentY,5) &&
                !consumeAbsorbShield()) {
                player_.takeDamage(child[0x26],x<cx?-1.0f:1.0f);
                camera_.shake(2.0f,8);
            }
        }
        const auto result=madPeckerShots_.dispatch(i,{sourceWord(camera_.baseX()),sourceWord(camera_.baseY())});
        if (result.initialized) AudioManager::playApu(0x34);
    }
}

void GameplayScene::renderMadPeckerShots(float cameraX,float cameraY) {
    const auto* texture=AssetCache::loadTexture("content/x1/sprites/enemies/mad_pecker_shot.png");
    if (!texture || !texture->valid()) return;
    for (const auto& child:madPeckerShots_.pool()) {
        if (!child[0] || child[0x0A]!=0x23 || child[1]!=2 || !(child[0x0E]&0x80)) continue;
        // Paired source OAM tile298/CGRAM12: one 8x8 cell, event offset(-4,-20).
        DrawTextureRec(texture->get(),{0,0,8,8},
            {float(mad_pecker_shot::word(child,5))-4-cameraX,
             float(mad_pecker_shot::word(child,8))-20-cameraY},WHITE);
    }
}
} // namespace mmx
