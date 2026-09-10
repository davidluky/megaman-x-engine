// hud.cpp - draws player, boss, lives, and weapon-energy HUD meters.
// Owns: HUD display smoothing, sprite loading, and meter render helpers.

#include "ui/hud.h"
#include "systems/asset_cache.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace mmx {

HUD::HUD() {}

void HUD::loadSprites() {
    if (spritesLoaded_) return;
    auto load = [](const char* p) {
        if (FileExists(p)) {
            const TextureResource* texture = AssetCache::loadTexture(p);
            if (texture) {
                texture->setFilter(TEXTURE_FILTER_POINT);
            }
            return texture;
        }
        return static_cast<const TextureResource*>(nullptr);
    };
    barLimit_ = load("content/x1/sprites/hud/bar_limit.png");
    // Real SNES HUD icon boxes extracted from the game (the life "X" + one per
    // weapon). Cells are still procedural.
    lifeIcon_ = load("content/x1/sprites/hud/life_icon.png");
    static const char* kWeaponIds[] = {
        "homing-torpedo", "chameleon-sting", "rolling-shield", "fire-wave",
        "storm-tornado", "electric-spark", "boomerang-cutter", "shotgun-ice",
    };
    for (const char* id : kWeaponIds) {
        std::string path = std::string("content/x1/sprites/hud/weapon_icon_") + id + ".png";
        weaponIcons_[id] = load(path.c_str());
    }
    // Boss/Maverick HP-bar bottom icon (skull), extracted pixel-exact from the
    // real CP boss fight (VRAM dump 850, OBJ layer).
    bossIcon_ = load("content/x1/sprites/hud/boss_skull_icon.png");
    spritesLoaded_ = true;
}

void HUD::update(int currentHP, int maxHP, int lives) {
    hp_ = currentHP;
    maxHP_ = maxHP;
    lives_ = lives;

    const float target = static_cast<float>(hp_);
    if (std::abs(displayHP_ - target) < 0.1f) {
        displayHP_ = target;
    } else {
        constexpr float speed = 0.4f;
        if (displayHP_ > target) {
            displayHP_ -= speed;
            if (displayHP_ < target) displayHP_ = target;
        } else {
            displayHP_ += speed;
            if (displayHP_ > target) displayHP_ = target;
        }
    }
}

void HUD::syncDisplay() {
    displayHP_ = static_cast<float>(hp_);
    displayWeaponE_ = static_cast<float>(weaponEnergy_);
    displayBossHP_ = static_cast<float>(bossHP_);
}

void HUD::showBossHP(int current, int max, bool immediate) {
    showBoss_ = true;
    bossHP_ = current;
    bossMaxHP_ = max;

    const float target = static_cast<float>(current);
    if (immediate) {
        // CP-B1C-H1: the source-timed boss intro already supplies the ROM's
        // exact +1-per-2-frames fill ladder, so easing it again would resample
        // an exact law. The 0.4/frame ease takes 2.5 frames per unit, which
        // quantizes to alternating 2- and 3-frame steps and stretches the
        // 62-frame source fill to ~78 frames.
        displayBossHP_ = target;
        return;
    }
    if (std::abs(displayBossHP_ - target) < 0.1f) {
        displayBossHP_ = target;
    } else {
        constexpr float speed = 0.4f;
        if (displayBossHP_ > target) {
            displayBossHP_ -= speed;
            if (displayBossHP_ < target) displayBossHP_ = target;
        } else {
            displayBossHP_ += speed;
            if (displayBossHP_ > target) displayBossHP_ = target;
        }
    }
}

void HUD::hideBossHP() {
    showBoss_ = false;
}

void HUD::setWeaponEnergy(int current, int max, Color weaponColor,
                         const std::string& weaponId) {
    showWeapon_ = true;
    weaponEnergy_ = current;
    weaponMaxEnergy_ = max;
    weaponColor_ = weaponColor;
    auto it = weaponIcons_.find(weaponId);
    curWeaponIcon_ = (it != weaponIcons_.end()) ? it->second : nullptr;

    const float target = static_cast<float>(current);
    if (std::abs(displayWeaponE_ - target) < 0.1f) {
        displayWeaponE_ = target;
    } else {
        constexpr float speed = 0.4f;
        if (displayWeaponE_ > target) {
            displayWeaponE_ -= speed;
            if (displayWeaponE_ < target) displayWeaponE_ = target;
        } else {
            displayWeaponE_ += speed;
            if (displayWeaponE_ > target) displayWeaponE_ = target;
        }
    }
}

void HUD::hideWeaponEnergy() {
    showWeapon_ = false;
}

void HUD::render() const {
    // Life and weapon gauges sit side by side at the top-left, equal height and
    // top-aligned, each with a small icon at the BOTTOM — matching the real MMX
    // HUD. Lives are not shown in-play (only on the death/ready screen).
    // Empty cells are solid near-black (24,24,24) — same as the inter-cell gap —
    // so the depleted region reads as one black column, as in the real HUD
    // (not the lighter grey segments it had before).
    // BOTTOM anchor (the icon's top) for the gauges. The gauges are bottom-aligned
    // and grow UPWARD, so Heart Tanks extend the life bar up while the icon stays
    // put — as in the real game. Measured from the real MMX1 V1.1 ROM (two Chill
    // Penguin gameplay AVIs): at the base 16 HP the life-bar top cap sits at y=45
    // and the X icon at y=80, with NO bar frame above y=45. (Our foreground aligns
    // 1:1 with the AVIs, so this is a true position, not a capture offset.)
    // capY 80 + 16 cells*2 + 4 cap == top y44 at base HP; was top-anchored at y8.
    const float kHudIconY = 80.0f;

    renderHealthBar(9.0f, kHudIconY, displayHP_, maxHP_,
                    {255, 198, 0, 255}, {24, 24, 24, 255}, lifeIcon_);

    if (showWeapon_) {
        renderHealthBar(25.0f, kHudIconY, displayWeaponE_, weaponMaxEnergy_,
                        weaponColor_, {24, 24, 24, 255}, curWeaponIcon_);
    }

    if (showBoss_) {
        // Maverick HP bar: same segmented style as X's gauge (yellow fill, solid
        // near-black empty cells), mirrored to the right edge, with the boss SKULL
        // icon at the bottom — extracted pixel-exact from the real CP boss fight
        // (VRAM dump 850, OBJ layer). Real bar left edge is x=233 (INTERNAL_WIDTH-23).
        renderHealthBar(INTERNAL_WIDTH - 23.0f, kHudIconY, displayBossHP_, bossMaxHP_,
                        {255, 198, 0, 255}, {24, 24, 24, 255}, bossIcon_);
    }
}

void HUD::renderHealthBar(float x, float y, float displayHP, int maxHP,
                          Color fillColor, Color emptyColor,
                          const TextureResource* iconTex) const {
    int ix = static_cast<int>(x);

    const int topCapH = (barLimit_ && barLimit_->valid()) ? barLimit_->height() : 4;
    const int slotH = kCellHeight_;  // 2px units (1px lit + 1px gap), as in MMX

    // `y` is the BOTTOM anchor (the icon's top). The bar grows UPWARD from here,
    // so the icon/bottom stays fixed as maxHP changes — Heart Tanks add cells on
    // TOP, exactly like the real MMX life gauge. (Previously top-anchored, which
    // slid the icon DOWN on every Heart Tank and grew the gauge the wrong way.)
    const int capY = static_cast<int>(y);
    int iy = capY - maxHP * slotH;   // top of the segment stack

    const Color frameBlack = {24, 24, 24, 255};
    const Color frameWhite = {255, 255, 255, 255};
    const Color frameGray = {140, 140, 140, 255};
    const Color highlight = {255, 255, 255, 255};

    // Plain end cap on top — drawn ABOVE the segment stack (bottom-anchored).
    const int capTopY = iy - topCapH;
    if (barLimit_ && barLimit_->valid()) {
        DrawTexture(barLimit_->get(), ix, capTopY, WHITE);
    } else {
        DrawRectangle(ix + 2, capTopY + 0, 10, 1, frameBlack);
        DrawRectangle(ix + 1, capTopY + 1, 12, 1, frameWhite);
        DrawRectangle(ix, capTopY + 2, 14, 1, frameGray);
        DrawRectangle(ix, capTopY + 3, 14, 1, frameBlack);
    }

    for (int i = 0; i < maxHP; i++) {
        const int tickY = iy + (maxHP - 1 - i) * slotH;
        const bool filled = (static_cast<float>(i) < displayHP);

        // Left/right frame brackets for every cell (procedural so the cell can be
        // taller than the 2px bar_area sprite).
        DrawRectangle(ix + 0, tickY, 1, slotH, frameBlack);
        DrawRectangle(ix + 1, tickY, 1, slotH, frameWhite);
        DrawRectangle(ix + 2, tickY, 1, slotH, frameGray);
        DrawRectangle(ix + 3, tickY, 1, slotH, frameBlack);
        DrawRectangle(ix + 10, tickY, 1, slotH, frameBlack);
        DrawRectangle(ix + 11, tickY, 1, slotH, frameGray);
        DrawRectangle(ix + 12, tickY, 1, slotH, frameWhite);
        DrawRectangle(ix + 13, tickY, 1, slotH, frameBlack);

        // Inner energy cell: a solid colored block with a lighter sheen down the
        // middle, and a 1px dark gap at the bottom so stacked cells read as pips.
        const int cellFillH = slotH - 1;
        if (filled) {
            DrawRectangle(ix + 4, tickY, 6, cellFillH, fillColor);
            DrawRectangle(ix + 6, tickY, 2, cellFillH, highlight);
        } else {
            DrawRectangle(ix + 4, tickY, 6, cellFillH, emptyColor);
        }
        DrawRectangle(ix + 4, tickY + cellFillH, 6, 1, frameBlack);
    }

    // Real SNES icon box (life "X" / weapon symbol) blitted directly under the
    // last cell — connects to the bar with no gap. Falls back to the plain end
    // cap when there's no icon (e.g. the boss bar).
    // capY (the bottom anchor) already equals iy + maxHP*slotH by construction.
    if (iconTex && iconTex->valid()) {
        DrawTexture(iconTex->get(), ix, capY, WHITE);
    } else if (barLimit_ && barLimit_->valid()) {
        DrawTexture(barLimit_->get(), ix, capY, WHITE);
    }
}

} // namespace mmx
