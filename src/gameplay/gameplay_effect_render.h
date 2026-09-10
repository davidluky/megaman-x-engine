// gameplay_effect_render.h - renders transient gameplay visual effects.
// Boundary: consumes effect state; spawning and damage semantics live elsewhere.

#pragma once

#include "gameplay/gameplay_effects.h"
#include "systems/asset_cache.h"
#include "systems/raylib_resource.h"
#include "raylib.h"

#include <vector>

namespace mmx::gameplay_effect_render {

struct TextureCache {
    const TextureResource* torpedoSmoke = nullptr;
    const TextureResource* torpedoFlash = nullptr;
    const TextureResource* busterL3Release = nullptr;
    const TextureResource* iceTrail = nullptr;
    const TextureResource* iceSpray = nullptr;
    const TextureResource* iceDebris = nullptr;
    const TextureResource* iceImpact = nullptr;
    const TextureResource* iceFlash = nullptr;
};

inline const TextureResource* cachedTexture(const TextureResource*& texture,
                                           const char* path) {
    if (!texture) {
        texture = AssetCache::loadTexture(path);
    }
    return texture;
}

inline const TextureResource* cachedPointTexture(const TextureResource*& texture,
                                                const char* path) {
    const TextureResource* result = cachedTexture(texture, path);
    if (result && result->valid()) {
        result->setFilter(TEXTURE_FILTER_POINT);
    }
    return result;
}

inline void renderTorpedoSmoke(const std::vector<gameplay_effects::TorpedoPuff>& puffs,
                              float camX,
                              float camY,
                              TextureCache& textures) {
    if (puffs.empty()) return;

    const TextureResource* smokeTexture = cachedPointTexture(
        textures.torpedoSmoke,
        "content/x1/sprites/weapons/homing_torpedo_smoke.png");
    if (!smokeTexture || !smokeTexture->valid()) return;

    gameplay_effects::emitTorpedoSmokeDraws(
        puffs,
        camX,
        camY,
        [smokeTexture](const gameplay_effects::TorpedoSmokeDraw& smoke) {
            const Rectangle src = {static_cast<float>(smoke.cell * 8), 0, 8, 8};
            DrawTextureRec(smokeTexture->get(), src, {smoke.x, smoke.y}, WHITE);
            return true;
        });
}

inline void renderIceTrailBits(
    const std::vector<gameplay_effects::IceTrailBit>& iceTrailBits,
    float camX,
    float camY,
    TextureCache& textures) {
    if (iceTrailBits.empty()) return;

    const TextureResource* trailTexture = cachedTexture(
        textures.iceTrail,
        "content/x1/sprites/weapons/shotgun_ice_trail.png");
    const TextureResource* debrisTexture = cachedTexture(
        textures.iceDebris,
        "content/x1/sprites/weapons/shotgun_ice_debris.png");

    gameplay_effects::emitIceTrailDraws(
        iceTrailBits,
        camX,
        camY,
        [trailTexture](const gameplay_effects::IceTrailSparkleDraw& sparkle) {
            if (trailTexture && trailTexture->valid()) {
                const Rectangle src = {
                    static_cast<float>(sparkle.frame * 8), 0, 8, 8};
                DrawTextureRec(trailTexture->get(), src,
                               {sparkle.x, sparkle.y}, WHITE);
            } else {
                DrawRectangle(static_cast<int>(sparkle.x) + 2,
                              static_cast<int>(sparkle.y) + 2,
                              4, 4, Color{180, 230, 255, 220});
            }
            return true;
        },
        [debrisTexture](const gameplay_effects::IceDebrisDraw& debris) {
            if (!debrisTexture || !debrisTexture->valid()) return true;
            Rectangle src = {debris.sourceX, 0, debris.width, 8};
            if (debris.hflip) { src.width = -src.width; }
            DrawTextureRec(debrisTexture->get(), src, {debris.x, debris.y}, WHITE);
            return true;
        });
}

inline void renderIceImpactBursts(
    const std::vector<gameplay_effects::IceImpactBurst>& impactBursts,
    float camX,
    float camY,
    TextureCache& textures) {
    if (impactBursts.empty()) return;

    const TextureResource* impactTexture = cachedTexture(
        textures.iceImpact,
        "content/x1/sprites/weapons/shotgun_ice_impact.png");
    if (impactTexture && impactTexture->valid()) {
        gameplay_effects::emitImpactBurstDraws(
            impactBursts,
            camX,
            camY,
            [impactTexture](const gameplay_effects::IceImpactDraw& impact) {
                const Rectangle src = {
                    static_cast<float>(impact.frame * 64), 0, 64, 64};
                DrawTextureRec(impactTexture->get(), src,
                               {impact.x, impact.y}, WHITE);
                return true;
            });
    }
}

inline void renderShotgunIceReleaseFlash(
    const gameplay_effects::ReleaseFlashState& releaseFlash,
    float camX,
    float camY,
    TextureCache& textures) {
    if (releaseFlash.frames <= 0 || releaseFlash.kind != 0) return;

    const TextureResource* flashTexture = cachedTexture(
        textures.iceFlash,
        "content/x1/sprites/weapons/shotgun_ice_flash.png");
    if (flashTexture && flashTexture->valid()) {
        gameplay_effects::emitIceReleaseSparkles(
            releaseFlash,
            camX,
            camY,
            [flashTexture](const gameplay_effects::ReleaseSparkle& sparkle) {
                Rectangle src = {static_cast<float>(sparkle.cell * 8), 0, 8, 8};
                DrawTextureRec(flashTexture->get(), src,
                               {sparkle.x, sparkle.y}, WHITE);
                return true;
            });
    }
}

inline void renderTorpedoReleaseFlash(
    const gameplay_effects::ReleaseFlashState& releaseFlash,
    bool playerFacingRight,
    float camX,
    float camY,
    TextureCache& textures) {
    if (releaseFlash.frames <= 0 || releaseFlash.kind != 1) return;

    const TextureResource* flashTexture = cachedPointTexture(
        textures.torpedoFlash,
        "content/x1/sprites/weapons/homing_torpedo_flash.png");
    if (flashTexture && flashTexture->valid()) {
        gameplay_effects::emitTorpedoReleaseSparkles(
            releaseFlash,
            playerFacingRight,
            camX,
            camY,
            [flashTexture](const gameplay_effects::ReleaseSparkle& sparkle) {
                Rectangle src = {static_cast<float>(sparkle.cell * 8), 0, 8, 8};
                if (sparkle.hflip) {
                    src.width = -src.width;   // U49: in-place flip (U40 law)
                }
                DrawTextureRec(flashTexture->get(), src,
                               {sparkle.x, sparkle.y}, WHITE);
                return true;
            });
    }
}

inline void renderBusterL3ReleaseFlash(
    const gameplay_effects::ReleaseFlashState& releaseFlash,
    bool playerFacingRight,
    float camX,
    float camY,
    TextureCache& textures) {
    gameplay_effects::BusterL3ReleaseFrame busterRelease;
    if (!gameplay_effects::makeBusterL3ReleaseFrame(
            releaseFlash, playerFacingRight, camX, camY, busterRelease)) {
        return;
    }

    const TextureResource* releaseTexture = cachedPointTexture(
        textures.busterL3Release,
        "content/x1/sprites/weapons/buster_l3_release_flash.png");
    if (releaseTexture && releaseTexture->valid()) {
        Rectangle src = {
            static_cast<float>(busterRelease.frame) *
                gameplay_effects::kBusterL3ReleaseCellW,
            0.0f,
            gameplay_effects::kBusterL3ReleaseCellW,
            gameplay_effects::kBusterL3ReleaseCellH,
        };
        if (busterRelease.hflip) src.width = -src.width;
        Rectangle dst = {
            busterRelease.x,
            busterRelease.y,
            gameplay_effects::kBusterL3ReleaseCellW,
            gameplay_effects::kBusterL3ReleaseCellH,
        };
        DrawTexturePro(releaseTexture->get(), src, dst,
                       {busterRelease.originX, 40.0f}, 0.0f, WHITE);
    }
}

inline void renderSledSpray(const std::vector<Projectile>& projectiles,
                           float camX,
                           float camY,
                           TextureCache& textures) {
    gameplay_effects::emitSledSprayPuffs(
        projectiles,
        camX,
        camY,
        [&textures](const gameplay_effects::SledSprayPuff& puff) {
            const TextureResource* sprayTexture = cachedTexture(
                textures.iceSpray,
                "content/x1/sprites/weapons/shotgun_ice_spray.png");
            if (!sprayTexture || !sprayTexture->valid()) return false;

            Rectangle src{static_cast<float>(puff.cell * 8), 0, 8, 8};
            if (puff.hflip) {
                src.width = -src.width;       // U49: in-place flip (U40 law)
            }
            DrawTextureRec(sprayTexture->get(), src, {puff.x, puff.y}, WHITE);
            return true;
        });
}

} // namespace mmx::gameplay_effect_render
