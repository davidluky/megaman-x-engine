// hud.h - declares the heads-up display state and drawing API.
// Owns: health/lives/boss/weapon display values and meter visibility flags.

#pragma once

#include "app/constants.h"
#include "raylib.h"

#include <string>
#include <unordered_map>

// ============================================================================
// hud.h — Megaman X-style heads-up display
//
// The MMX HUD is iconic:
//   - Vertical health bar on the left edge, filling upward
//   - Each HP tick is a small rectangle that fills/drains with animation
//   - Lives counter in the top-left corner
//   - Boss health bar appears on the right when fighting a boss
//
// The HUD renders at internal resolution (256x224) directly — no camera
// offset. It's drawn last, on top of everything else.
//
// The health bar animates: when HP changes, ticks fill/drain one at a time
// over several frames, creating the classic cascading fill effect.
// ============================================================================

namespace mmx {

class TextureResource;

class HUD {
public:
    HUD();
    ~HUD() = default;
    HUD(const HUD&) = delete;
    HUD& operator=(const HUD&) = delete;
    HUD(HUD&&) = delete;
    HUD& operator=(HUD&&) = delete;

    void loadSprites();
    void update(int currentHP, int maxHP, int lives);
    void syncDisplay();
    void setWeaponEnergy(int current, int max, Color weaponColor,
                         const std::string& weaponId = "");
    void hideWeaponEnergy();
    void render() const;

    // Boss health bar (future — shown when boss fight starts)
    // `immediate` snaps the displayed bar to `current` instead of easing it,
    // for source-timed sequences that already carry an exact per-frame ladder.
    void showBossHP(int current, int max, bool immediate = false);
    void hideBossHP();

private:
    // Rounded end cap + the real SNES HUD icon boxes (14x17) extracted from the
    // game: the life "X" and one per weapon. Cells are procedural.
    const TextureResource* barLimit_ = nullptr;  // spr_bar1_limit  14x4  (bar end cap)
    const TextureResource* lifeIcon_ = nullptr;  // life_icon.png   14x17 (blue X box)
    std::unordered_map<std::string, const TextureResource*> weaponIcons_;
    const TextureResource* curWeaponIcon_ = nullptr;
    const TextureResource* bossIcon_ = nullptr;  // boss/Maverick skull icon (bar bottom)
    bool spritesLoaded_ = false;
    // Height of each energy cell. The real MMX gauge draws each unit as a thin
    // 1px-lit line + 1px gap (2px). The bar grows with max HP / heart tanks
    // rather than by fattening the segments.
    static constexpr int kCellHeight_ = 2;
    // Player stats (latched from update)
    int hp_ = 16;
    int maxHP_ = 16;
    int lives_ = 3;

    // Animated display HP — eases toward actual HP for cascading fill
    float displayHP_ = 16.0f;

    // Weapon energy bar
    bool showWeapon_ = false;
    int weaponEnergy_ = 0;
    int weaponMaxEnergy_ = 16;
    float displayWeaponE_ = 0.0f;
    Color weaponColor_ = {100, 200, 255, 255};

    // Boss bar
    bool showBoss_ = false;
    int bossHP_ = 0;
    int bossMaxHP_ = 0;
    float displayBossHP_ = 0.0f;

    void renderHealthBar(float x, float y, float displayHP, int maxHP,
                          Color fillColor, Color emptyColor,
                          const TextureResource* iconTex) const;
};

} // namespace mmx
