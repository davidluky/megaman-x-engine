// GameplayScene adapter for the source-proven solid Axe Max launcher/log stack.

#include "gameplay/gameplay_scene.h"

#include "gameplay/axemax_stack_collision.h"
#include "gameplay/gameplay_enemy_contact.h"

namespace mmx {

void GameplayScene::resolvePlayerAxeMaxStackCollision() {
    // The existing launcher/log envelope belongs to each active Axe Max,
    // including one placed in the test map. Gating it on the stage ID left
    // the rendered stack passable there (user review R228). The measured
    // CP remnant correction below keeps its own source-anchor eligibility;
    // enabling the envelope does not give other placements that anchor.
    if (player_.isDead()) return;

    AABB playerBox = player_.getHitbox();
    for (const auto& enemy : enemies_) {
        const auto blocker = enemy.solidAxeStackHitbox();
        AABB previousBox = playerBox;
        previousBox.x += player_.prevPosition.x - player_.position.x;
        previousBox.y += player_.prevPosition.y - player_.position.y;
        if (const auto vertical = axemax_stack_collision::resolveVertical(
                previousBox, playerBox, blocker, player_.hitboxOffset.y,
                player_.velocity.y)) {
            player_.position.y = vertical->playerPositionY;
            player_.prevPosition.y = player_.position.y;
            player_.velocity.y = 0;
            player_.onGround = vertical->onGround;
            player_.onCeiling = vertical->onCeiling;
            playerBox = player_.getHitbox();
            continue;
        }
        if (const auto sourceLog = enemy.cpRemnantLogSourceAnchor()) {
            if (const auto correction =
                    gameplay_enemy_contact::resolveSourceAxeMaxLogHorizontal(
                        player_, *sourceLog)) {
                // Source STA81B313 changes only integer X. Preserve velocity,
                // interpolation and existing wall flags; BD4 also has a wider
                // C0D2 contact producer before these measured X corrections.
                player_.position.x += static_cast<float>(*correction);
                playerBox = player_.getHitbox();
            }
        }
        const auto resolution = axemax_stack_collision::resolveHorizontal(
            playerBox, blocker, player_.hitboxOffset.x);
        if (!resolution.has_value()) continue;

        player_.position.x = resolution->playerPositionX;
        player_.touchingWallLeft = resolution->touchingWallLeft;
        player_.touchingWallRight = resolution->touchingWallRight;
        player_.velocity.x = 0.0f;
        // The collision follows the normal tile pass. Keep interpolation from
        // drawing one transient frame through the stack before the snap-back.
        player_.prevPosition.x = player_.position.x;
        playerBox = player_.getHitbox();
    }
}

} // namespace mmx
