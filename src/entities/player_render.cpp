// player_render.cpp - renders the player and builds composed/palette variant textures.
// Owns: render diagnostics, facing rules, armor sheet composition, and recolor variants.

#include "entities/player.h"

#include "systems/asset_cache.h"

#include <utility>

namespace mmx {

namespace {

const char* stingPhaseKey(int phase) {
    static const char* kStingPhaseKeys[9] = {
        "sting-flash",   "sting-cycle-b", "sting-cycle-c",
        "sting-cycle-d", "sting-cycle-e", "sting-cycle-f",
        "sting-cycle-g", "sting-cycle-h", "sting-cycle-i"};
    if (phase < 0 || phase >= 9) return "";
    return kStingPhaseKeys[phase];
}


} // namespace

int Player::renderFrameIndex(int frameOverride) const {
    if (frameOverride >= 0) return frameOverride;
    int idx = anim_.currentFrameIndex();
    // Substitute the eyes-closed idle frame (sheet cell 62) during a blink.
    if (blinkCloseTimer_ > 0 && anim_.currentName() == "idle") idx = kIdleBlinkFrame;
    return idx;
}


bool Player::stingHiddenAtFrame(int d) {
    if (d >= 4 && d <= 121) return (d - 4) % 3 == 0;     // 1 of 3
    if (d >= 123 && d <= 361) return (d - 123) % 2 == 0; // 1 of 2
    if (d >= 366 && d <= 481) return (d - 366) % 5 == 0; // 1 of 5
    return false;
}


int Player::stingPhaseAtFrame(int d) {
    if (d == 1) return 0;                      // 1-frame white/purple flash
    if (d >= 8) return 1 + ((d - 8) / 6) % 8;  // b..i, 6f each, 48f period
    return -1;                                 // d0, d2..7: normal palette
}


PlayerRenderDiagnostic Player::renderDiagnostic(
    float alpha, Vector2 cameraOffset, bool suppressVisualGrounding,
    int frameOverride) {
    PlayerRenderDiagnostic diag;
    diag.frameIndex = renderFrameIndex(frameOverride);
    diag.facingRight = facingRight;
    diag.visualFacingRight = visualFacingRight();
    diag.spriteWidth = spriteWidth;
    diag.spriteHeight = spriteHeight;
    const float visualGroundingOffset =
        !suppressVisualGrounding && visualGroundingApplies()
        ? visualGroundingOffsetY
        : 0.0f;
    diag.destRect = {
        prevPosition.x + (position.x - prevPosition.x) * alpha - cameraOffset.x,
        prevPosition.y + (position.y - prevPosition.y) * alpha
            - cameraOffset.y + visualGroundingOffset,
        spriteWidth,
        spriteHeight,
    };
    diag.state = static_cast<int>(state_);
    diag.hp = health;
    diag.iframeTimer = iframeTimer_;
    diag.deathTimer = deathTimer_;
    diag.stingVisualFrame = stingVisualFrame();
    diag.stingPhase = (diag.stingVisualFrame >= 0) ? stingPhaseAtFrame(diag.stingVisualFrame) : -1;
    diag.chargeLevel = chargeLevel_;
    diag.chargeTimer = chargeTimer_;
    if (!weaponInventory.empty()) {
        diag.weaponId = weaponInventory.current().id;
    }
    diag.tintAlpha = (iframeTimer_ > 0 && state_ != PlayerState::Hurt &&
                      (iframeTimer_ / 2) % 2 == 0) ? 60 : 255;

    if (state_ == PlayerState::Die) {
        diag.bodyDrawn = false;
        diag.visibilityReason = "death_burst_only";
        return diag;
    }
    if (diag.stingVisualFrame >= 0 && stingHiddenAtFrame(diag.stingVisualFrame)) {
        diag.bodyDrawn = false;
        diag.visibilityReason = "sting_hidden";
        return diag;
    }

    const TextureResource* sheet = spriteSheetForCurrentWeapon();
    diag.sheetPathOrKey = activeSheetPath_;
    if (!weaponInventory.empty() && !weaponInventory.isBuster()) {
        diag.paletteVariantKey = weaponInventory.current().id;
        diag.sheetPathOrKey = "weapon:" + diag.paletteVariantKey + "|" + diag.sheetPathOrKey;
    }
    if (diag.stingPhase >= 0) {
        const char* key = stingPhaseKey(diag.stingPhase);
        if (const TextureResource* v = variantForRowKey(key)) {
            sheet = v;
            diag.paletteVariantKey = key;
            diag.sheetPathOrKey = std::string(key) + "|" + diag.sheetPathOrKey;
        }
    }

    if (sheet && sheet->valid()) {
        diag.sheetWidth = sheet->width();
        diag.sheetHeight = sheet->height();
        diag.sheetColumns = diag.sheetWidth / static_cast<int>(spriteWidth);
        if (diag.sheetColumns == 0) diag.sheetColumns = 1;
        const int tx = (diag.frameIndex % diag.sheetColumns) * static_cast<int>(spriteWidth);
        const int ty = (diag.frameIndex / diag.sheetColumns) * static_cast<int>(spriteHeight);
        const float srcWidth = diag.visualFacingRight ? spriteWidth : -spriteWidth;
        diag.sourceRect = {static_cast<float>(tx), static_cast<float>(ty), srcWidth, spriteHeight};
    } else {
        diag.bodyDrawn = false;
        diag.visibilityReason = "missing_sheet";
    }
    return diag;
}

void Player::render(float alpha) {
    render(alpha, {0.0f, 0.0f});
}


void Player::render(float alpha, Vector2 cameraOffset) {
    render(alpha, cameraOffset, false);
}


void Player::render(
    float alpha, Vector2 cameraOffset, bool suppressVisualGrounding,
    int frameOverride) {
    float drawX = prevPosition.x + (position.x - prevPosition.x) * alpha - cameraOffset.x;
    float drawY = prevPosition.y + (position.y - prevPosition.y) * alpha - cameraOffset.y;
    if (!suppressVisualGrounding && visualGroundingApplies()) {
        drawY += visualGroundingOffsetY;
    }

    // Death sequence: X vanishes instantly into a radial burst of "life energy"
    // nodes that fly outward (the iconic MMX death). The sprite is not drawn.
    if (state_ == PlayerState::Die) {
        for (const auto& n : deathNodes_) {
            const float nodeX = n.x - cameraOffset.x;
            const float nodeY = n.y - cameraOffset.y;
            DrawCircle(static_cast<int>(nodeX), static_cast<int>(nodeY), 3.0f, {180, 230, 255, 255});
            DrawCircle(static_cast<int>(nodeX), static_cast<int>(nodeY), 1.5f, WHITE);
        }
        return;
    }

    // Sting charged state (U03b, s7b_cgram): X's sprites vanish entirely on
    // the measured blink cadence — skip ALL drawing on hidden frames (the
    // real game drops the OAM cluster; trail/charge overlays ride the body).
    const int stingD = stingVisualFrame();
    if (stingD >= 0 && stingHiddenAtFrame(stingD)) {
        return;
    }

    const TextureResource* sheet = spriteSheetForCurrentWeapon();
    // Sting cycle phase recolor: swap in the measured CGRAM-row variant
    // (sting-flash at d1, sting-cycle-b..i from d8, 6f each). Indexed-
    // palette builds only; the base sting sheet stays for d0/d2..7.
    if (const int phase = (stingD >= 0) ? stingPhaseAtFrame(stingD) : -1;
        phase >= 0) {
        if (const TextureResource* v = variantForRowKey(stingPhaseKey(phase))) {
            sheet = v;
        }
    }
    if (!sheet || !sheet->valid()) {
        return;
    }

    // Main sprite
    {
        const Texture2D& texture = sheet->get();
        int frameIdx = renderFrameIndex(frameOverride);
        int cols = texture.width / static_cast<int>(spriteWidth);
        if (cols == 0) cols = 1;
        int tx = (frameIdx % cols) * static_cast<int>(spriteWidth);
        int ty = (frameIdx / cols) * static_cast<int>(spriteHeight);

        // Negative width in source rect flips the texture horizontally.
        // visualFacingRight: the wall-slide pose faces the wall (U73).
        float srcWidth = visualFacingRight() ? spriteWidth : -spriteWidth;
        Rectangle srcRect = { static_cast<float>(tx), static_cast<float>(ty), srcWidth, spriteHeight };
        Rectangle destRect = { drawX, drawY, spriteWidth, spriteHeight };

        Color tint = WHITE;
        if (iframeTimer_ > 0 && state_ != PlayerState::Hurt && ((iframeTimer_ / 2) % 2 == 0)) {
            tint.a = 60;
        }

        DrawTexturePro(texture, srcRect, destRect, {0, 0}, 0.0f, tint);

        // Charge body-blink: while the buster is charging, the real game flashes
        // X's body palette toward white/cyan on a 2-frames-on / 2-frames-off
        // cadence (RAM-measured: CGRAM rows 145-159 toggle every 2 frames, e.g.
        // white (31,31,31) + cyan (0,23,31)). Reproduce with an additive
        // cyan-white pass over the same sprite on the "on" frames.
        if (chargeLevel_ >= 1 && (chargeTimer_ / 2) % 2 == 0) {
            // Additive flash tinted to match the charge aura: the real charge goes
            // CYAN at level 1, then ORANGE/fire at level 2 (ROM-captured). Keep the
            // additive moderate so it tints rather than washing X to white.
            Color flash = (chargeLevel_ >= 2) ? Color{170, 80, 0, 230}   // L2 orange
                                              : Color{0, 130, 170, 190}; // L1 cyan
            BeginBlendMode(BLEND_ADDITIVE);
            DrawTexturePro(texture, srcRect, destRect, {0, 0}, 0.0f, flash);
            EndBlendMode();
        }
    }

    // Charge effect: the real SNES charge particle ring rebuilt from OAM.
    // U217 pins separate L1 cyan, L2 orange, and arm-upgrade L3 pink strips.
    // 72x64 frames, X's body center at overlay pixel (33,36), one refresh
    // layout every 4 game frames.
    if (chargeLevel_ >= 1) {
        const char* auraPath = (chargeLevel_ >= 3)
            ? "content/x1/sprites/weapons/charge_aura_l3.png"
            : (chargeLevel_ >= 2)
                ? "content/x1/sprites/weapons/charge_aura_l2.png"
                : "content/x1/sprites/weapons/charge_aura_l1.png";
        const TextureResource* aura = AssetCache::loadTexture(auraPath);
        if (aura && aura->valid()) {
            aura->setFilter(TEXTURE_FILTER_POINT);
            constexpr int FW = 72, FH = 64, FRAMES = 8;
            // Real effect refreshes particle positions every 4 frames (~15fps),
            // so each baked position-set is held for 4 game frames.
            const int tc = (chargeTimer_ >= 0 ? chargeTimer_ : 0);
            const int f = (tc / 4) % FRAMES;
            Rectangle src{static_cast<float>(f * FW), 0.0f,
                          static_cast<float>(FW), static_cast<float>(FH)};
            Rectangle dst{drawX, drawY + 4.0f,
                          static_cast<float>(FW), static_cast<float>(FH)};
            DrawTexturePro(aura->get(), src, dst, {0, 0}, 0.0f, WHITE);
        }
    }

}


void Player::refreshArmorSheet() {
    const std::string want = desiredArmorCompositeKey();
    const bool keyChanged = want != armorCompositeKey_;
    if (keyChanged) {
        armorCompositeKey_ = want;
        weaponSpriteVariants_.clear();
    }

    const std::vector<std::string> activePieces = activeArmorPieces();
    if (!sheetComposer_.compose(xPalette_, activePieces)) {
        activeSheetPath_ = armorCompositeKey_.empty() ? spriteSourcePath_
                                                      : "composite:" + armorCompositeKey_;
        return;
    }
    xPalette_.setIndexMap(sheetComposer_.indexData(),
                          sheetComposer_.width(), sheetComposer_.height());

    if (armorCompositeKey_.empty()) {
        activeSheetPath_ = spriteSourcePath_;
        if (IsWindowReady()) {
            if (const TextureResource* t = AssetCache::loadTexture(spriteSourcePath_);
                t != nullptr && t->valid()) {
                setSpriteSheetResource(t);
            }
        }
        return;
    }

    activeSheetPath_ = "composite:" + armorCompositeKey_;
    if (IsWindowReady()) {
        uploadCompositeSheetTexture();
    }
}

std::string Player::desiredArmorCompositeKey() const {
    const auto& state = progressState();
    std::vector<std::string> pieces;
    if (state.armorBoots && armorDeltaPaths_.count("boots")) pieces.push_back("boots");
    if (state.armorBuster && armorDeltaPaths_.count("buster")) pieces.push_back("buster");
    if (state.armorBody && armorDeltaPaths_.count("body")) pieces.push_back("body");
    if (state.armorHelmet && armorDeltaPaths_.count("helmet")) pieces.push_back("helmet");

    std::string key;
    for (const std::string& piece : pieces) {
        if (!key.empty()) key += "+";
        key += piece;
    }
    return key;
}

std::vector<std::string> Player::activeArmorPieces() const {
    std::vector<std::string> pieces;
    if (armorCompositeKey_.empty()) return pieces;
    const auto& state = progressState();
    const unsigned mask =
        (state.armorHelmet ? 0x01u : 0u) |
        (state.armorBody ? 0x02u : 0u) |
        (state.armorBuster ? 0x04u : 0u) |
        (state.armorBoots ? 0x08u : 0u);
    static constexpr char kHex[] = "0123456789abcdef";
    std::string comboKey = "combo_00";
    comboKey[6] = kHex[(mask >> 4) & 0x0F];
    comboKey[7] = kHex[mask & 0x0F];
    if (mask != 0 && armorDeltaPaths_.count(comboKey)) {
        pieces.push_back(comboKey);
        return pieces;
    }
    if (armorCompositeKey_ == "boots+buster+body+helmet" &&
        armorDeltaPaths_.count("fullset")) {
        pieces.push_back("fullset");
        return pieces;
    }
    size_t start = 0;
    while (start < armorCompositeKey_.size()) {
        const size_t plus = armorCompositeKey_.find('+', start);
        if (plus == std::string::npos) {
            pieces.push_back(armorCompositeKey_.substr(start));
            break;
        }
        pieces.push_back(armorCompositeKey_.substr(start, plus - start));
        start = plus + 1;
    }
    return pieces;
}

bool Player::uploadCompositeSheetTexture() {
    if (!sheetComposer_.composed() || sheetComposer_.baseData().empty()) return false;
    ImageResource image;
    if (!image.loadFromPixels(sheetComposer_.baseData().data(),
                              sheetComposer_.width(), sheetComposer_.height())) {
        return false;
    }
    if (!image.uploadTo(compositeSheetTexture_)) return false;
    compositeSheetTexture_.setFilter(TEXTURE_FILTER_POINT);
    setSpriteSheetResource(&compositeSheetTexture_);
    return true;
}


const TextureResource* Player::spriteSheetForCurrentWeapon() {
    if (weaponInventory.isBuster()) {
        return spriteSheetResource();
    }

    const Weapon& weapon = weaponInventory.current();
    auto it = weaponSpriteVariants_.find(weapon.id);
    if (it != weaponSpriteVariants_.end()) {
        return it->second.valid() ? &it->second : spriteSheetResource();
    }

    const TextureResource* variant = buildWeaponSpriteVariant(weapon);
    return variant ? variant : spriteSheetResource();
}


bool Player::buildVariantPixels(const std::string& rowKey,
                                std::vector<Color>& out) const {
    if (!sheetComposer_.composed()) return false;
    if (sheetComposer_.width() <= 0 || sheetComposer_.height() <= 0) return false;
    out = sheetComposer_.baseData();
    if (out.empty()) return false;

    if (xPalette_.indexed()
        && xPalette_.indexWidth() == sheetComposer_.width()
        && xPalette_.indexHeight() == sheetComposer_.height()) {
        for (int y = 0; y < sheetComposer_.height(); ++y) {
            for (int x = 0; x < sheetComposer_.width(); ++x) {
                Color& px = out[static_cast<size_t>(y) * sheetComposer_.width() + x];
                px = xPalette_.recolorAt(x, y, px, rowKey);
            }
        }
        return true;
    }

    if (!xPalette_.valid()) return false;
    for (Color& px : out) {
        px = xPalette_.recolor(px, rowKey);
    }
    return true;
}


const TextureResource* Player::uploadVariantTexture(const std::string& key,
                                                    const std::vector<Color>& pixels) {
    if (pixels.empty() || sheetComposer_.width() <= 0 || sheetComposer_.height() <= 0) {
        return nullptr;
    }
    ImageResource image;
    if (!image.loadFromPixels(pixels.data(), sheetComposer_.width(), sheetComposer_.height())) {
        return nullptr;
    }
    TextureResource texture;
    if (!image.uploadTo(texture)) {
        TraceLog(LOG_WARNING, "Player: failed to create palette texture for %s",
                 key.c_str());
        return nullptr;
    }
    texture.setFilter(TEXTURE_FILTER_POINT);
    auto inserted = weaponSpriteVariants_.emplace(key, std::move(texture));
    return &inserted.first->second;
}


const TextureResource* Player::buildWeaponSpriteVariant(const Weapon& weapon) {
    // Buster keeps the original (vivid) sheet untouched; only special weapons recolor.
    if (weapon.id == "buster") return nullptr;
    std::vector<Color> variantPixels;
    if (!buildVariantPixels(weapon.id, variantPixels)) return nullptr;
    return uploadVariantTexture(weapon.id, variantPixels);
}


const TextureResource* Player::variantForRowKey(const std::string& rowKey) {
    auto it = weaponSpriteVariants_.find(rowKey);
    if (it != weaponSpriteVariants_.end()) {
        return it->second.valid() ? &it->second : nullptr;
    }
    // Indexed path ONLY: the legacy LUT keys by weapon id and would silently
    // fall back to Normal colors for cycle keys — wrong, so refuse instead.
    if (!xPalette_.indexed()) return nullptr;
    std::vector<Color> variantPixels;
    if (!buildVariantPixels(rowKey, variantPixels)) return nullptr;
    return uploadVariantTexture(rowKey, variantPixels);
}


bool Player::visualFacingRight() const {
    if (state_ == PlayerState::Fall && wallDropPoseTimer_ > 0) {
        return wallDropVisualFacingRight_;
    }
    if (state_ == PlayerState::WallSlide || state_ == PlayerState::WallJump) {
        // R356 raw OAM: plain AND shooting slide/kick cells are authored
        // from a right wall. Logical facing still sends shots away from it.
        return wallSide_ > 0;
    }
    return facingRight;
}


} // namespace mmx
