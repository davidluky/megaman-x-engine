// GameplayScene adapter for the source-proven solid Axe Max launcher/log stack.

#include "gameplay/gameplay_scene.h"

#include "gameplay/axemax_stack_collision.h"

namespace mmx {

void GameplayScene::resolvePlayerAxeMaxStackCollision() {
    // The launcher/log stack belongs to each active Axe Max, including one
    // placed in the test map. Gating it on the stage ID left the rendered
    // stack passable there (user review R228). The per-enemy law, including
    // the measured CP remnant source contact list, lives headless in
    // axemax_stack_collision::resolvePlayerStack.
    if (player_.isDead()) return;
    for (const auto& enemy : enemies_) {
        axemax_stack_collision::resolvePlayerStack(player_, enemy);
    }
}

} // namespace mmx
