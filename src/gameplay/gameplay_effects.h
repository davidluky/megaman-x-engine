// gameplay_effects.h - owns shared transient effect queues for GameplayScene.
// Boundary: effects visualize events; they do not decide gameplay outcomes.

#pragma once

#include "entities/player.h"
#include "entities/player_anchor.h"
#include "entities/projectile.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mmx::gameplay_effects {

struct IceTrailBit {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float gravity = 0.0f;
    int age = 0;
    int serial = 0;
    int kind = 0;
    int lifetime = 0;
    bool hflip = false;
};

struct IceImpactBurst {
    float x = 0.0f;
    float y = 0.0f;
    int age = 0;
};

struct TorpedoPuff {
    float x = 0.0f;
    float y = 0.0f;
    int age = 0;
    int ownerSerial = 0;
};

struct TorpedoSmokeDraw {
    int cell = 0;
    float x = 0.0f;
    float y = 0.0f;
};

struct ReleaseFlashState {
    int frames = 0;
    float x = 0.0f;
    float y = 0.0f;
    int kind = 0; // 0 = ice sled, 1 = torpedo fan, 2 = buster L3
};

struct EffectViewport {
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
};

struct SledSprayPuff {
    int cell = 0;
    float x = 0.0f;
    float y = 0.0f;
    bool hflip = false;
};

struct ReleaseSparkle {
    int cell = 0;
    float x = 0.0f;
    float y = 0.0f;
    bool hflip = false;
};

struct BusterL3ReleaseFrame {
    int frame = 0;
    float x = 0.0f;
    float y = 0.0f;
    bool hflip = false;
    float originX = 48.0f;
};

struct IceTrailSparkleDraw {
    int frame = 0;
    float x = 0.0f;
    float y = 0.0f;
};

struct IceDebrisDraw {
    float sourceX = 0.0f;
    float width = 8.0f;
    float x = 0.0f;
    float y = 0.0f;
    bool hflip = false;
};

struct IceImpactDraw {
    int frame = 0;
    float x = 0.0f;
    float y = 0.0f;
};

inline constexpr float kBusterL3ReleaseCellW = 112.0f;
inline constexpr float kBusterL3ReleaseCellH = 80.0f;

inline void spawnSledDebris(std::vector<IceTrailBit>& iceTrailBits,
                            int& iceTrailSerial,
                            const Projectile& sled) {
    // MEASURED wall-break burst (WP-B cos_s6_face f1021): 6 pieces from the
    // sled anchor, exact vectors, gravity 48/256, off-screen despawn only.
    // Silent cosmetic debris; no bullet rows or APU write.
    struct Piece {
        int kind;
        bool hflip;
        float vx;
        float vyUp;
    };
    static const Piece kPieces[] = {
        {1, true,   1.0f, 4.5f}, {1, false, -2.0f, 5.0f},
        {1, true,   3.0f, 4.0f}, {1, false, -1.5f, 3.0f},
        {2, true,   2.5f, 3.5f}, {2, false, -3.5f, 2.5f},
    };

    const bool mirrored = sled.vx > 0.0f; // measured facing = left
    for (const auto& piece : kPieces) {
        IceTrailBit bit;
        bit.x = sled.position.x + sled.hitboxSize.x * 0.5f;
        bit.y = sled.position.y + sled.hitboxSize.y * 0.5f;
        bit.vx = mirrored ? -piece.vx : piece.vx;
        bit.vy = -piece.vyUp;
        bit.gravity = 0.1875f;
        bit.age = 0;
        bit.serial = ++iceTrailSerial;
        bit.kind = piece.kind;
        bit.lifetime = 0;
        bit.hflip = mirrored ? !piece.hflip : piece.hflip;
        iceTrailBits.push_back(bit);
    }
}

inline void spawnImpactBurst(std::vector<IceImpactBurst>& impactBursts,
                             float x,
                             float y) {
    impactBursts.push_back({x, y, 0});
}

template <typename Emit>
inline bool emitTorpedoSmokeDraws(const std::vector<TorpedoPuff>& puffs,
                                  float camX,
                                  float camY,
                                  Emit emit) {
    static const int kCell[7] = {0, 1, 1, 2, 2, 3, 3};
    for (const auto& puff : puffs) {
        const int age = std::clamp(puff.age, 0, 6);
        TorpedoSmokeDraw draw;
        draw.cell = kCell[age];
        draw.x = puff.x - 4.0f - camX;
        draw.y = puff.y - 4.0f - camY;
        if (!emit(draw)) return false;
    }

    return true;
}

template <typename EmitSparkle, typename EmitDebris>
inline bool emitIceTrailDraws(const std::vector<IceTrailBit>& iceTrailBits,
                              float camX,
                              float camY,
                              EmitSparkle emitSparkle,
                              EmitDebris emitDebris) {
    for (const auto& bit : iceTrailBits) {
        if (bit.kind == 0) {
            IceTrailSparkleDraw draw;
            draw.frame = (bit.age / 4) % 4;
            draw.x = bit.x - camX - 4.0f;
            draw.y = bit.y - camY - 4.0f;
            if (!emitSparkle(draw)) return false;
            continue;
        }

        IceDebrisDraw draw;
        draw.width = (bit.kind == 1) ? 8.0f : 16.0f;
        draw.sourceX = (bit.kind == 1) ? 0.0f : 8.0f;
        draw.x = bit.x - camX - draw.width * 0.5f;
        draw.y = bit.y - camY - 4.0f;
        draw.hflip = bit.hflip;
        if (!emitDebris(draw)) return false;
    }

    return true;
}

template <typename Emit>
inline bool emitImpactBurstDraws(const std::vector<IceImpactBurst>& impactBursts,
                                 float camX,
                                 float camY,
                                 Emit emit) {
    for (const auto& burst : impactBursts) {
        IceImpactDraw draw;
        draw.frame = burst.age;
        draw.x = burst.x - camX - 32.0f;
        draw.y = burst.y - camY - 32.0f;
        if (!emit(draw)) return false;
    }

    return true;
}

template <typename Emit>
inline bool emitIceReleaseSparkles(const ReleaseFlashState& releaseFlash,
                                   float camX,
                                   float camY,
                                   Emit emit) {
    if (releaseFlash.frames <= 0 || releaseFlash.kind != 0) return true;

    // Charged Shotgun Ice release: 8 one-frame sparkles around X.
    struct Sparkle {
        int cell;
        float dx;
        float dy;
    };
    static const Sparkle kFlash[8] = {
        {1, -4.0f, 10.0f}, {1, 32.0f, 25.0f},
        {2,  8.0f, 34.0f}, {2, 20.0f,  2.0f},
        {3,  4.0f,  8.0f}, {3, 24.0f, 28.0f},
        {0, 22.0f, 38.0f}, {0,  6.0f, -2.0f},
    };

    for (const auto& sparkle : kFlash) {
        ReleaseSparkle draw;
        draw.cell = sparkle.cell;
        draw.x = releaseFlash.x + sparkle.dx - camX;
        draw.y = releaseFlash.y + sparkle.dy - camY;
        if (!emit(draw)) return false;
    }
    return true;
}

template <typename Emit>
inline bool emitTorpedoReleaseSparkles(const ReleaseFlashState& releaseFlash,
                                       bool playerFacingRight,
                                       float camX,
                                       float camY,
                                       Emit emit) {
    if (releaseFlash.frames <= 0 || releaseFlash.kind != 1) return true;

    // Same 8-sparkle geometry in torpedo pal-3 colors, x-mirrored for left.
    struct Sparkle {
        int cell;
        bool hflip;
        float dx;
        float dy;
    };
    static const Sparkle kFlash[8] = {
        {0, false, -10.0f, -22.0f}, {0, true,   6.0f, 18.0f},
        {1, true,  -20.0f, -10.0f}, {1, true,  16.0f,  5.0f},
        {2, false,  -8.0f,  14.0f}, {2, true,   4.0f, -18.0f},
        {3, true,  -12.0f, -12.0f}, {3, true,   8.0f,  8.0f},
    };

    for (const auto& sparkle : kFlash) {
        ReleaseSparkle draw;
        draw.cell = sparkle.cell;
        draw.hflip = playerFacingRight ? sparkle.hflip : !sparkle.hflip;
        const float dx = playerFacingRight ? sparkle.dx : -sparkle.dx - 8.0f;
        draw.x = releaseFlash.x + dx - camX;
        draw.y = releaseFlash.y + sparkle.dy - camY;
        if (!emit(draw)) return false;
    }
    return true;
}

inline bool makeBusterL3ReleaseFrame(const ReleaseFlashState& releaseFlash,
                                     bool playerFacingRight,
                                     float camX,
                                     float camY,
                                     BusterL3ReleaseFrame& draw) {
    if (releaseFlash.frames <= 0 || releaseFlash.kind != 2) return false;

    constexpr int kFrameCount = 11;
    draw.frame = std::clamp(kFrameCount - releaseFlash.frames, 0, kFrameCount - 1);
    draw.x = releaseFlash.x - camX;
    draw.y = releaseFlash.y - camY;
    draw.hflip = !playerFacingRight;
    draw.originX = playerFacingRight ? 48.0f : kBusterL3ReleaseCellW - 48.0f;
    return true;
}

template <typename Projectiles, typename Emit>
inline bool emitSledSprayPuffs(const Projectiles& projectiles,
                               float camX,
                               float camY,
                               Emit emit) {
    // Snow spray at the moving sled's rear-bottom. Offsets are OAM-measured
    // per wheel state; strip cells: 0=tile60, 1=45, 2=47, 3=61, 4=63.
    struct PuffCell {
        int cell;
        float dx;
        float dy;
    };
    static const PuffCell kStates[4][5] = {
        {{0, -8.0f, 8.0f}, {-1, 0.0f, 0.0f}, {-1, 0.0f, 0.0f},
         {-1, 0.0f, 0.0f}, {-1, 0.0f, 0.0f}},
        {{1, 1.0f, 8.0f}, {1, 9.0f, 8.0f}, {4, -6.0f, 7.0f},
         {0, -8.0f, 8.0f}, {-1, 0.0f, 0.0f}},
        {{3, 0.0f, 8.0f}, {3, 8.0f, 8.0f}, {4, -16.0f, 2.0f},
         {4, -8.0f, 4.0f}, {0, -8.0f, 8.0f}},
        {{2, 0.0f, 8.0f}, {2, 3.0f, 8.0f}, {4, -25.0f, 5.0f},
         {0, -8.0f, 8.0f}, {-1, 0.0f, 0.0f}},
    };

    for (const auto& projectile : projectiles) {
        if (!projectile.active || !projectile.rideable || !projectile.isPlayerShot) {
            continue;
        }
        if (!projectile.sledMovingSpritePath.empty()) continue; // composite owns spray.
        if (std::fabs(projectile.vx) <= 0.01f) continue;

        const bool right = projectile.vx > 0.0f;
        const float bodyX = projectile.position.x - camX;
        const float anchorY = projectile.position.y - camY;
        // spr 5,5,6,6,6,7,... -> ((movingFrames + 1) / 3) % 4.
        const int movingFrames =
            std::max(0, projectile.ageFrames - projectile.sledLaunchFrame);
        const int state = ((movingFrames + 1) / 3) % 4;

        for (const auto& puff : kStates[state]) {
            if (puff.cell < 0) continue;
            SledSprayPuff draw;
            draw.cell = puff.cell;
            draw.y = anchorY + puff.dy;
            if (right) {
                draw.x = bodyX + 8.0f + puff.dx;
            } else {
                draw.x = bodyX + 24.0f - puff.dx;
                draw.hflip = true;
            }
            if (!emit(draw)) return false;
        }
    }

    return true;
}

template <typename Projectiles>
inline void updateWeaponCosmetics(std::vector<IceTrailBit>& iceTrailBits,
                                  int& iceTrailSerial,
                                  std::vector<IceImpactBurst>& impactBursts,
                                  ReleaseFlashState& releaseFlash,
                                  const Player& player,
                                  const Projectiles& projectiles,
                                  const EffectViewport& viewport) {
    // Physics + despawn first so new bits are visible for a full tick.
    for (auto& bit : iceTrailBits) {
        bit.vy += bit.gravity;
        bit.x += bit.vx;
        bit.y += bit.vy;
        bit.age++;
    }

    // Trail bits die by timer; sled debris falls through terrain and prunes
    // only when off-screen.
    iceTrailBits.erase(
        std::remove_if(iceTrailBits.begin(), iceTrailBits.end(),
                       [&](const IceTrailBit& bit) {
                           if (bit.kind == 0) return bit.age >= bit.lifetime;
                           return bit.x < viewport.left || bit.x > viewport.right ||
                                  bit.y < viewport.top || bit.y > viewport.bottom;
                       }),
        iceTrailBits.end());

    for (auto& burst : impactBursts) burst.age++;
    impactBursts.erase(
        std::remove_if(impactBursts.begin(), impactBursts.end(),
                       [](const IceImpactBurst& burst) { return burst.age >= 21; }),
        impactBursts.end());

    if (releaseFlash.frames > 0) releaseFlash.frames--;
    for (const auto& projectile : projectiles) {
        if (!projectile.active || !projectile.isPlayerShot ||
            projectile.ageFrames != 1) {
            continue;
        }

        if (projectile.rideable) {
            releaseFlash.x = player.position.x;
            releaseFlash.y = player.position.y;
            releaseFlash.kind = 0;
            releaseFlash.frames = 1;
        } else if (projectile.torpedoFanMember) {
            releaseFlash.x = player_anchor::sourceRamAnchorX(
                player.position.x, player.spriteWidth, player.facingRight);
            releaseFlash.y = player.position.y + 37.0f;
            releaseFlash.kind = 1;
            releaseFlash.frames = 1;
        } else if (projectile.type == ProjectileType::ChargeL3 &&
                   projectile.weaponId == "buster" &&
                   projectile.sinePhaseFrames == 0) {
            releaseFlash.x = player_anchor::sourceRamAnchorX(
                player.position.x, player.spriteWidth, player.facingRight);
            releaseFlash.y = player.position.y + 37.0f;
            releaseFlash.kind = 2;
            releaseFlash.frames = 11;
        }
    }

    // Shotgun Ice pellets shed one stationary trail bit at the measured cadence.
    for (const auto& projectile : projectiles) {
        if (!projectile.active || !projectile.isPlayerShot ||
            projectile.trailEveryFrames <= 0) {
            continue;
        }
        if (projectile.ageFrames <= 0 ||
            (projectile.ageFrames % projectile.trailEveryFrames) != 0) {
            continue;
        }

        IceTrailBit bit;
        bit.x = projectile.position.x + projectile.hitboxSize.x * 0.5f;
        bit.y = projectile.position.y + projectile.hitboxSize.y * 0.5f;
        bit.vx = 0.0f;
        bit.vy = projectile.trailRiseVy;
        bit.gravity = projectile.trailGravity;
        bit.age = 0;
        bit.serial = ++iceTrailSerial;
        bit.kind = 0;
        bit.lifetime = 30;
        iceTrailBits.push_back(bit);
    }
}

template <typename Projectiles>
inline void updateTorpedoSmoke(std::vector<TorpedoPuff>& puffs,
                               Projectiles& projectiles) {
    // Age + prune first so a puff's first visible tick is its spawn position.
    for (auto& puff : puffs) puff.age++;
    puffs.erase(
        std::remove_if(puffs.begin(), puffs.end(),
                       [&](const TorpedoPuff& puff) {
                           if (puff.age >= 7) return true;
                           return std::none_of(
                               projectiles.begin(),
                               projectiles.end(),
                               [&](const Projectile& projectile) {
                                   return projectile.active &&
                                          projectile.serial == puff.ownerSerial;
                               });
                       }),
        puffs.end());

    // Homing Torpedo puffs spawn every 3 frames in the wake, using the body
    // center from the previous puff tick plus the measured nose-ward offset.
    for (auto& projectile : projectiles) {
        if (!projectile.active || !projectile.isPlayerShot ||
            projectile.weaponId != "homing-torpedo") {
            continue;
        }
        if (!projectile.homing && !projectile.torpedoFanMember) continue;

        const float cx = projectile.position.x + projectile.hitboxSize.x * 0.5f;
        const float cy = projectile.position.y + projectile.hitboxSize.y * 0.5f;
        if (projectile.ageFrames <= 1) {
            projectile.puffWakeX = cx;
            projectile.puffWakeY = cy;
            continue;
        }

        if (projectile.ageFrames % 3 == 0) {
            puffs.push_back(
                {projectile.puffWakeX + (projectile.facingRight ? 2.0f : -2.0f),
                 projectile.puffWakeY,
                 0,
                 projectile.serial});
            projectile.puffWakeX = cx;
            projectile.puffWakeY = cy;
        }
    }
}

} // namespace mmx::gameplay_effects
