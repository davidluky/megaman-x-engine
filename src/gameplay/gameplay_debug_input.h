// gameplay_debug_input.h - handles gameplay-only debug and shortcut inputs.
// Boundary: debug routing must not become normal progression or tuning logic.

#pragma once

#include "gameplay/gameplay_debug_overlay.h"
#include "gameplay/gameplay_sprite_test.h"
#include "entities/player.h"
#include "app/input.h"
#include "systems/raylib_resource.h"

#include "raylib.h"

#include <algorithm>

namespace mmx::gameplay_debug_input {

inline void handleOverlayToggles(gameplay_debug_overlay::State& state) {
    if (Input::isDebug1Pressed()) {
        Input::consumeDebug1Press();
        gameplay_debug_overlay::toggleCollision(state);
    }
    if (Input::isDebug2Pressed()) {
        Input::consumeDebug2Press();
        gameplay_debug_overlay::toggleHud(state);
    }
}

template <typename GrantAllWeapons>
inline void handleWeaponGrant(GrantAllWeapons grantAllWeapons) {
    if (Input::isDebug4Pressed()) {
        Input::consumeDebug4Press();
        grantAllWeapons(false);
        TraceLog(LOG_INFO, "Debug: granted all weapons (F4, no armor mutation)");
    }
}

inline void handleArmorGrantInputs(Player& player) {
    if (Input::isUpgradeGrant1Pressed()) {
        Input::consumeUpgradeGrant1Press();
        player.grantArmorBoots();
        TraceLog(LOG_INFO, "GRANT: legs capsule (dash unlocked)");
    }
    if (Input::isUpgradeGrant2Pressed()) {
        Input::consumeUpgradeGrant2Press();
        player.grantArmorBody();
        TraceLog(LOG_INFO, "GRANT: body capsule (half damage)");
    }
    if (Input::isUpgradeGrant3Pressed()) {
        Input::consumeUpgradeGrant3Press();
        player.grantArmorHelmet();
        TraceLog(LOG_INFO, "GRANT: helmet capsule (headbutt)");
    }
    if (Input::isUpgradeGrant4Pressed()) {
        Input::consumeUpgradeGrant4Press();
        player.grantArmorBuster();
        TraceLog(LOG_INFO, "GRANT: buster capsule (charge L3)");
    }
}

inline void toggleSpriteTestModeIfRequested(gameplay_sprite_test::State& state) {
    if (Input::isDebug3Pressed()) {
        Input::consumeDebug3Press();
        gameplay_sprite_test::toggle(state);
    }
}

inline int spriteTestMaxFrame(const Player& player) {
    int maxFrame = 0;
    const TextureResource* playerSheet = player.spriteSheetResource();
    if (playerSheet && playerSheet->valid()) {
        const int cols = playerSheet->width() / static_cast<int>(player.spriteWidth);
        const int rows = playerSheet->height() / static_cast<int>(player.spriteHeight);
        maxFrame = cols * rows - 1;
    }
    return maxFrame;
}

inline void updateSpriteTestFrameNavigation(gameplay_sprite_test::State& state,
                                            int maxFrame) {
    if (state.delay > 0) state.delay--;
    if (state.delay == 0) {
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            state.frame = (state.frame + 1) % (maxFrame + 1);
            state.delay = 10;
        }
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            state.frame--;
            if (state.frame < 0) state.frame = maxFrame;
            state.delay = 10;
        }
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) {
            state.frame = std::min(state.frame + 10, maxFrame);
            state.delay = 10;
        }
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) {
            state.frame = std::max(state.frame - 10, 0);
            state.delay = 10;
        }
    }
}

} // namespace mmx::gameplay_debug_input
