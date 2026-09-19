// projectile.cpp - updates and draws weapon projectiles.
// Owns: projectile sprite selection, serials, lifetimes, and visual effects.

#include "entities/projectile.h"
#include "app/constants.h"
#include "systems/asset_cache.h"
#include "systems/raylib_resource.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace mmx {
namespace {

constexpr const char* BUSTER_SHOT_NORMAL = "content/x1/sprites/weapons/x_buster_shot_normal.png";
constexpr const char* BUSTER_SHOT_1 = "content/x1/sprites/weapons/x_buster_shot_1.png";
constexpr const char* BUSTER_SHOT_2 = "content/x1/sprites/weapons/x_buster_shot_2.png";
constexpr const char* BUSTER_SHOT_3 = "content/x1/sprites/weapons/x_buster_shot_3.png";
constexpr const char* BUSTER_L3_ORBS = "content/x1/sprites/weapons/buster_l3_orbs.png";
// Level-2 blue charge comet (mmx1-buster sheet row 2). Real shot alternates a
// deep-BLUE phase (frame E) with a brief WHITE flash (frame D) — verified from
// per-frame flight screenshots. Sheet: [E,E,D] @44x36 so it reads mostly blue
// with a periodic white blink (David: it was too white = I'd used only the flash
// frame). Level 1 green shot stays as BUSTER_SHOT_2 (it's good).
constexpr const char* CHARGE2_FLIGHT = "content/x1/sprites/weapons/x_charge2_flight.png";

using L3HelixOffsets = std::array<float, 24>;
constexpr std::array<L3HelixOffsets, 3> BUSTER_L3_HELIX_Y = {{
    {{
        -4.5391f, -8.3281f, -11.3672f, -13.6562f,
        -15.1953f, -15.9844f, -16.0234f, -9.9844f,
        -4.6953f, -0.1562f, 3.6328f, 6.6719f,
        8.9609f, 10.5000f, 11.2891f, 5.2500f,
        -0.0391f, -4.5781f, -8.3672f, -11.4062f,
        -13.6953f, -15.2344f, -16.0234f, -9.9844f,
    }},
    {{
        -12.7891f, -12.8281f, -6.7891f, -1.5000f,
        3.0391f, 6.8281f, 9.8672f, 12.1562f,
        13.6953f, 14.4844f, 8.4453f, 3.1562f,
        -1.3828f, -5.1719f, -8.2109f, -10.5000f,
        -12.0391f, -12.8281f, -6.7891f, -1.5000f,
        3.0391f, 6.8281f, 9.8672f, 12.1562f,
    }},
    {{
        12.7891f, 12.8281f, 12.1172f, 10.6562f,
        4.6172f, -0.6719f, -5.2109f, -9.0000f,
        -12.0391f, -14.3281f, -15.8672f, -16.6562f,
        -10.6172f, -5.3281f, -0.7891f, 3.0000f,
        6.0391f, 8.3281f, 9.8672f, 10.6562f,
        4.6172f, -0.6719f, -5.2109f, -9.0000f,
    }},
}};

Color brighten(Color c, int r, int g, int b) {
    c.r = static_cast<unsigned char>(std::min(255, static_cast<int>(c.r) + r));
    c.g = static_cast<unsigned char>(std::min(255, static_cast<int>(c.g) + g));
    c.b = static_cast<unsigned char>(std::min(255, static_cast<int>(c.b) + b));
    return c;
}

Color withAlpha(Color c, unsigned char a) {
    c.a = a;
    return c;
}

void drawCapsule(float x, float y, float w, float h, Color outline, Color fill, Color core) {
    int ix = static_cast<int>(std::round(x));
    int iy = static_cast<int>(std::round(y));
    int iw = std::max(2, static_cast<int>(std::round(w)));
    int ih = std::max(2, static_cast<int>(std::round(h)));

    DrawRectangle(ix + 1, iy, iw - 2, ih, outline);
    DrawRectangle(ix, iy + 1, iw, ih - 2, outline);
    DrawRectangle(ix + 2, iy + 2, std::max(1, iw - 4), std::max(1, ih - 4), fill);
    DrawRectangle(ix + 4, iy + ih / 2 - 1, std::max(1, iw - 8), 2, core);
}

void drawDiamond(float cx, float cy, float halfW, float halfH, Color outline, Color fill, Color core) {
    Vector2 top{cx, cy - halfH};
    Vector2 right{cx + halfW, cy};
    Vector2 bottom{cx, cy + halfH};
    Vector2 left{cx - halfW, cy};
    DrawTriangle(top, left, right, outline);
    DrawTriangle(bottom, right, left, outline);
    DrawTriangle({cx, cy - halfH + 2}, {cx - halfW + 3, cy}, {cx + halfW - 3, cy}, fill);
    DrawTriangle({cx, cy + halfH - 2}, {cx + halfW - 3, cy}, {cx - halfW + 3, cy}, fill);
    DrawRectangle(static_cast<int>(cx - halfW / 2), static_cast<int>(cy - 1),
                  static_cast<int>(halfW), 2, core);
}

void drawSpriteProjectile(const Projectile& p, float drawX, float drawY, Color tint) {
    if (p.visualSpritePath.empty() || p.visualFrameWidth <= 0 ||
        p.visualFrameHeight <= 0 || p.visualFrameCount <= 0) {
        return;
    }

    const TextureResource* sheet = AssetCache::loadTexture(p.visualSpritePath);
    if (!sheet || !sheet->valid()) {
        return;
    }
    sheet->setFilter(TEXTURE_FILTER_POINT);

    int frame;
    bool mirror = p.visualMirrorsWithFacing && !p.facingRight;
    if (p.visualHeadingIndexed) {
        // Homing Torpedo S7: sheet column == 32-dir heading (0=up, cw),
        // flips baked into the strip — heading already encodes direction,
        // so the facing mirror must NOT apply.
        frame = ((p.homingHeading % 32) + 32) % 32;
        mirror = false;
    } else if (p.visualFixedFrame >= 0) {
        // Charged torpedo fan: one measured cell per member (as fired
        // right); the facing mirror handles a left-side release.
        frame = p.visualFixedFrame % p.visualFrameCount;
    } else if (p.visualIntroCells > 0 && p.visualIntroCells < p.visualFrameCount) {
        // Intro-then-loop (U29 fire-wave head): cells [0, intro) once, then
        // the tail cells loop.
        const int ticks = std::max(1, p.visualFrameTicks);
        const int step = p.ageFrames / ticks;
        frame = (step < p.visualIntroCells)
            ? step
            : p.visualIntroCells
                + (step - p.visualIntroCells)
                      % (p.visualFrameCount - p.visualIntroCells);
        frame += p.visualFrameStart;
    } else {
        frame = p.visualFrameStart +
                (p.ageFrames / std::max(1, p.visualFrameTicks)) % p.visualFrameCount;
    }
    Rectangle source{
        static_cast<float>(frame * p.visualFrameWidth),
        0.0f,
        static_cast<float>(p.visualFrameWidth),
        static_cast<float>(p.visualFrameHeight)
    };
    if (mirror) {
        // raylib 5.x flips a negative-width source IN PLACE (flipX +
        // abs(width); source.x untouched) — do NOT pre-shift x, that
        // samples the NEXT cell (U40: sting fired left was corrupted).
        source.width = -source.width;
    }

    const float drawW = p.visualFrameWidth * p.visualScale;
    const float drawH = p.visualFrameHeight * p.visualScale;
    Rectangle dest{
        drawX + p.hitboxSize.x * 0.5f
            + (mirror ? -p.visualOffsetX : p.visualOffsetX),
        drawY + p.hitboxSize.y * 0.5f + p.visualOffsetY,
        drawW,
        drawH
    };
    Vector2 origin{drawW * 0.5f, drawH * 0.5f};
    if (p.visualTileYCopies > 0) {
        for (int copy = -p.visualTileYCopies; copy <= p.visualTileYCopies; ++copy) {
            Rectangle tiledDest = dest;
            tiledDest.y += static_cast<float>(copy) * drawH;
            DrawTexturePro(sheet->get(), source, tiledDest, origin, 0.0f, tint);
        }
    } else {
        DrawTexturePro(sheet->get(), source, dest, origin, 0.0f, tint);
    }
}

} // namespace

namespace {
int s_nextSerial = 0;
}

void Projectile::assignFreshSerial() {
    // Copies (e.g. the e-spark backward charged twin) share the source's
    // serial — trace tracks would merge; give the copy its own id.
    serial = ++s_nextSerial;
}

bool Projectile::usesOneHitPerTargetGate() const {
    return piercing && !continuousDamage;
}

void Projectile::configureSourceChargeL1(float playerRamX, float playerRamY, Vector2 muzzleOffset) {
    sourceChargeMuzzleOffset = muzzleOffset;
    // charge_l1_birth/contact_phase_2026-09-17.json: source writes integer words.
    sourceChargeL1Law = true;
    const float direction = facingRight ? 1.0f : -1.0f;
    sourceCollisionCenterOffset = Vector2{hitboxSize.x * 0.5f, hitboxSize.y * 0.5f};
    position = {std::floor(playerRamX) + direction * sourceChargeMuzzleOffset.x
                    - sourceCollisionCenterOffset->x,
                std::floor(playerRamY) + sourceChargeMuzzleOffset.y
                    - sourceCollisionCenterOffset->y};
    prevPosition = position;
    speed = source_charge_l1::kFlightStepPx;
    vx = direction * speed;
    vy = 0;
}

void Projectile::followSourceChargeL1(float playerRamX) {
    if (!sourceChargeL1Law || !sourceCollisionCenterOffset ||
        ageFrames >= source_charge_l1::kLaunchFrame) return;
    position.x = std::floor(playerRamX) + (facingRight ? 1.0f : -1.0f) *
        sourceChargeMuzzleOffset.x - sourceCollisionCenterOffset->x;
}

std::optional<source_charge_l1::HitProfile> Projectile::sourceChargeHitProfile() const {
    if (!sourceChargeL1Law || !isPlayerShot || weaponId != "buster" ||
        type != ProjectileType::ChargeL1) {
        return std::nullopt;
    }
    // update() advances ageFrames before the scene resolves contact, so the
    // claim frame arrives here as ageFrames 1 = source frame 0 of the life.
    return source_charge_l1::profileForFrame(ageFrames > 0 ? ageFrames - 1 : 0);
}

bool Projectile::shouldConsumeAfterEnemyHit(bool killedEnemy) const {
    if (type == ProjectileType::ChargeL1 && weaponId == "buster") {
        return !killedEnemy;
    }
    return !piercing;
}

int Projectile::damageToBoss() const {
    // Oracle 2026-06-15, l3_boss_cp_direct_mask2_delay620: ARM L3 spawns
    // the multi-object pink cluster, but Chill Penguin takes the normal
    // full-charge boss hit (32 -> 29) before blink invulnerability gates the
    // other objects. Keep the higher per-orb enemy damage separate.
    if (type == ProjectileType::ChargeL3 && weaponId == "buster") {
        return 3;
    }
    return damage;
}

bool Projectile::hasHitEnemySerial(int enemySerial) const {
    return std::find(hitEnemySerials.begin(), hitEnemySerials.end(), enemySerial) !=
           hitEnemySerials.end();
}

void Projectile::markHitEnemySerial(int enemySerial) {
    if (!hasHitEnemySerial(enemySerial)) {
        hitEnemySerials.push_back(enemySerial);
    }
}

void Projectile::init(float x, float y, float inVX, float inVY, ProjectileType ptype) {
    // Monotonic id so traces/tests can follow one projectile across the
    // compacting projectiles_ vector (Task-17 ghost proof).
    serial = ++s_nextSerial;
    sourceCollisionCenterOffset.reset();
    sourceChargeL1Law = false;
    sourceChargeMuzzleOffset = {source_charge_l1::kClaimAnchorOffsetX, source_charge_l1::kClaimAnchorOffsetY};
    sourceAxeMaxLog = false;
    advancedForEnemyShotPhase = false;
    position = {x, y};
    prevPosition = position;
    type = ptype;
    active = true;
    ageFrames = 0;
    visualSpritePath.clear();
    visualFrameWidth = 0;
    visualFrameStart = 0;
    visualFrameHeight = 0;
    visualFrameCount = 0;
    visualFrameTicks = 4;
    visualScale = 1.0f;
    visualIntroCells = 0;
    flickerAlternate = false;
    visualOffsetX = 0.0f;
    visualOffsetY = 0.0f;
    hitBossOnce = false;
    hitEnemySerials.clear();
    renderSuppressFrames = 0;
    l3HelixVariant = -1;
    visualHeadingIndexed = false;
    visualFixedFrame = -1;
    visualTileYCopies = 0;
    formationSpritePath.clear();
    formationFrames = 0;
    sledMovingSpritePath.clear();
    sledMovingFrameWidth = 0;
    sledMovingFrameHeight = 0;
    sledMovingFrameCount = 0;
    sfxFanRelease = -1;
    sfxShatter = -1;
    sfxShieldHum = -1;
    sfxShieldHumFirst = 0;
    sfxShieldHumEvery = 0;
    sfxSegmentPlant = -1;
    bouncesOffWalls = false;

    switch (ptype) {
        case ProjectileType::Normal:
            hitboxSize = {8, 6};
            hitboxOffset = {0, 0};
            speed = 5.0f;
            damage = 1;
            piercing = false;
            lifetime = 180;
            // Static yellow ball (extracted from the real game) — the uncharged
            // shot does not animate through the charge-sparkle frames.
            visualStyle = ProjectileVisualStyle::BusterSprite;
            visualSpritePath = BUSTER_SHOT_NORMAL;
            visualFrameWidth = 10;
            visualFrameHeight = 8;
            visualFrameCount = 1;
            visualFrameTicks = 1;
            break;
        case ProjectileType::ChargeL1:
            // In the real game, releasing at the blue (level-1) charge already
            // fires the green energy comet — the same shot as a fuller charge,
            // just a weaker hit. (There is no small distinct blue-charge shot.)
            hitboxSize = {40, 14};
            hitboxOffset = {0, 0};
            speed = 5.5f;
            damage = 2;
            piercing = false;
            lifetime = 180;
            visualStyle = ProjectileVisualStyle::BusterSprite;
            visualSpritePath = BUSTER_SHOT_2;
            visualFrameWidth = 43;
            visualFrameHeight = 24;
            visualFrameStart = 4;
            visualFrameCount = 3;
            visualFrameTicks = 3;
            visualScale = 1.25f;
            break;
        case ProjectileType::ChargeL2:
            // U71 (supersedes the 2026-06-04 "keep the blue ball" note —
            // David 2026-06-12: "the animations of the shot, the colors and
            // the timing are wrong"): MEASURED from build/u71/l2_shot
            // (bullets.csv oid=3 + per-frame OAM/VRAM composes):
            //   - claim at release, held AT THE MUZZLE 6 frames, then moves
            //     at exactly 8.0 px/f (raw 2048)
            //   - flight cells cycle a 6-frame ping-pong (spr 2,2,3,4,4,3)
            //   - art = the real white-core/cyan crackle comet, ripped via
            //     X-template subtraction (buster_charge2_real.png: 6f intro
            //     [hold orb, forming swirl] + 6f loop, ticks 1).
            // Non-piercing: disappears on enemy hit like the lemon.
            hitboxSize = {26, 24};
            hitboxOffset = {0, 0};
            speed = 8.0f;            // measured raw vx 2048
            damage = 3;
            piercing = false;
            lifetime = 240;
            // NOTE: the measured 6f muzzle hold is applied in
            // Player::fireShot for the BUSTER only — special weapons fire
            // their charged shots as ChargeL2 too and have their own laws.
            visualStyle = ProjectileVisualStyle::BusterSprite;
            visualSpritePath = "content/x1/sprites/weapons/buster_charge2_real.png";
            visualFrameWidth = 56;
            visualFrameHeight = 44;
            visualFrameStart = 0;
            visualFrameCount = 12;
            visualIntroCells = 6;
            visualFrameTicks = 1;
            break;
        case ProjectileType::ChargeL3:
            hitboxSize = {46, 24};
            hitboxOffset = {0, 0};
            speed = 5.5f;
            damage = 5;
            piercing = true;
            lifetime = 300;
            spiralTimer = 0;
            visualStyle = ProjectileVisualStyle::BusterSprite;
            // U45/U103: ARM-upgrade full charge uses the pink orb cluster.
            // Player::fireShot supplies the measured three-object sine path;
            // keep the base projectile contract on the same generated cell.
            visualSpritePath = BUSTER_L3_ORBS;
            visualFrameWidth = 56;
            visualFrameHeight = 48;
            visualFrameStart = 0;
            visualFrameCount = 1;
            visualFrameTicks = 2;
            break;
    }

    // Store actual velocity components. For player shots, inVY is typically 0.
    // For boss spread patterns, both components carry the intended direction.
    this->vx = inVX;
    this->vy = inVY;

    if (inVX >= 0) {
        position.x = x;
        facingRight = true;
    } else {
        position.x = x - hitboxSize.x;
        facingRight = false;
    }
    position.y = y - hitboxSize.y / 2;
}

void Projectile::applyWeaponVisual(const std::string& id, bool charged) {
    weaponId = id;

    if (id == "buster") {
        // init() already selected the DD-derived buster sprite strip for this
        // charge level. Keep this explicit so special-weapon fallback art
        // cannot accidentally rewrite the X-Buster profile.
        return;
    }

    visualSpritePath.clear();
    visualFrameWidth = 0;
    visualFrameStart = 0;
    visualFrameHeight = 0;
    visualFrameCount = 0;
    visualFrameTicks = charged ? 3 : 4;
    visualScale = charged ? 1.2f : 1.0f;
    visualIntroCells = 0;
    visualHeadingIndexed = false;
    visualFixedFrame = -1;
    visualTileYCopies = 0;
    flickerAlternate = false;
    visualOffsetX = 0.0f;
    visualOffsetY = 0.0f;
    formationSpritePath.clear();
    formationFrames = 0;
    sledMovingSpritePath.clear();
    sledMovingFrameWidth = 0;
    sledMovingFrameHeight = 0;
    sledMovingFrameCount = 0;

    if (id == "shotgun-ice") {
        visualStyle = ProjectileVisualStyle::ShotgunIce;
        if (charged) {
            // Charged = ice sled. VRAM-decoded (S7 2026-06-09): the real
            // moving body is ONE static 40x16 image — the wheel byte
            // animates the snow SPRAY (drawn by the scene, renderSledSpray),
            // not the body. The old 4-variant strip was GIF-derived and
            // doesn't exist in the real game.
            visualSpritePath = "content/x1/sprites/weapons/shotgun_ice_sled.png";
            visualFrameWidth  = 40;
            visualFrameHeight = 16;
            visualFrameCount  = 1;
            visualScale       = 1.0f;  // already full-size, don't upscale
            sledMovingSpritePath = "content/x1/sprites/weapons/shotgun_ice_sled_moving.png";
            sledMovingFrameWidth = 80;
            sledMovingFrameHeight = 16;
            sledMovingFrameCount = 4;
            // Formation growth (oracle S3): ~51 frames of chunk frames
            // spr0..4 (phase = ageFrames/10) before the full body appears.
            formationSpritePath = "content/x1/sprites/weapons/shotgun_ice_formation.png";
            formationFrames = 51;
        } else {
            // Normal = ice pellet (x=124-136, y=11-23), centered in 16x16. Static.
            visualSpritePath = "content/x1/sprites/weapons/shotgun_ice_shot.png";
            visualFrameWidth  = 16;
            visualFrameHeight = 16;
            visualFrameCount  = 1;
        }
    } else if (id == "storm-tornado") {
        // U30 S7 (col_vram_pf per-frame dumps + art-hash timeline): the
        // column GROWS through sprs 1..31 at 2f/phase over the 65f window
        // (the gust LENGTHENS rightward 37->133px in 4-phase swirl cycles),
        // then the rush loops [31,28,29,30] — 36 cells of 136x32, anchor
        // (16,12), via visualIntroCells. The real column is drawn only on
        // EVEN frames (SNES flicker translucency) -> flickerAlternate.
        // Charged halves get their own sheets in fireShot.
        visualStyle = ProjectileVisualStyle::StormTornado;
        visualSpritePath = "content/x1/sprites/weapons/storm_tornado_gust.png";
        visualFrameWidth = 136;
        visualFrameHeight = 32;
        visualFrameCount = 36;
        visualFrameTicks = 2;
        visualIntroCells = 32;
        visualOffsetX = 52.0f;   // cell center (68,16) - anchor (16,12)
        visualOffsetY = 4.0f;
        flickerAlternate = true;
    } else if (id == "fire-wave") {
        visualStyle = ProjectileVisualStyle::FireWave;
        if (charged) {
            // U29 S7 (sm_wake_iso isolation + cp_wave_vram per-frame dumps,
            // build_s7_wave_content.py): the WAKE flame plays spr 128..146
            // at 2f per phase — grow 8->37px tall then lift/burn out — over
            // its exact 40f life (20 cells of 24x48, anchor (12,32) = 8px
            // below cell center). U43 re-measure (build/u43/wave, FM
            // feet-visible fixture): the wake RAM anchor rests at
            // floor-11 (y 677 vs floor 688), art bottoms ride floor-2..0
            // — the old offY +4 planted the anchor ON the floor and sank
            // the art 11px under the walking line (David's report).
            // anchor screen y = (groundY-24) + 12 + offY + 8 => offY -7
            // puts the anchor at floor-11. HEAD overridden in fireShot.
            // U43 REBUILD (build/u43/wave, all 5 wakes' OAM union): the
            // flame grows ON the floor (ages 0-16), then LIFTS off rising
            // up to 64px above it (ages 17-39) — beyond the old 48px cell
            // (the clamped lift was David's "top animation bugged"). New
            // cells 40x80, FLOOR baked at row 76: center must sit at
            // groundY-36 => offY = -24 ((groundY-24)+12+offY = groundY-36).
            visualSpritePath = "content/x1/sprites/weapons/fire_wave_wave.png";
            visualFrameWidth = 40;
            visualFrameHeight = 80;
            visualFrameCount = 20;
            visualFrameTicks = 2;
            // U87 fresh FM oracle: wake OAM sits dx -9..+7 around the RAM
            // anchor; the U43 runtime cells are centered 10px too far right.
            visualOffsetX = -10.0f;
            visualOffsetY = -24.0f;
        } else {
            // U77 (s1_vram bullets.csv): the stream segment holds spr128
            // for 5f, spr129 for 2f, then plays 131/132/133 for one frame
            // each across its 10f life. The sheet duplicates cells to keep
            // the runtime on a simple one-tick cadence.
            visualSpritePath = "content/x1/sprites/weapons/fire_wave_shot.png";
            visualFrameWidth = 16;
            visualFrameHeight = 24;
            visualFrameCount = 10;
            visualFrameTicks = 1;
        }
    } else if (id == "electric-spark") {
        visualStyle = ProjectileVisualStyle::ElectricSpark;
        if (charged) {
            // S7 (2026-06-10): tall electric column, flight frames spr137-140
            // (16-19 x 90-96 px incl. the crackle ends — David's clipping
            // report caught the first decode cutting them), one anim step per
            // frame, arc dancing fore/aft of the anchor — cells 44x104 at
            // measured offsets (build_s7_espark_content.py over s3_vram).
            // COLUMN POSITION (sm_column_check OAM bridge, sm_setup_REPORT
            // 2026-06-10, latency+cam corrected): horizontally CENTERED on
            // the bullet x, ~70px above / ~24px below the RAM anchor. The
            // s3_vram-era cells hold the art at +3.5 below / -8.5 left of
            // the cell anchor -> draw correction (+8,-27). Zero these if the
            // strip is ever rebuilt re-anchored.
            visualSpritePath = "content/x1/sprites/weapons/electric_spark_charged.png";
            visualFrameWidth = 44;
            visualFrameHeight = 104;
            visualFrameCount = 4;
            visualFrameTicks = 1;
            visualOffsetX = 8.0f;
            visualOffsetY = -27.0f;
        } else {
            // S7 (2026-06-10): 4-frame pulsing cycle spr128-131 (18x18
            // crackle / 16x16 ball / 12x12 ball / 16x16 flipped ball), one
            // anim step per frame — cells 24x24 with the ball center on the
            // cell center (anchor at (15,8): center (12,12) - corr (-3,+4)).
            visualSpritePath = "content/x1/sprites/weapons/electric_spark_shot.png";
            visualFrameWidth = 24;
            visualFrameHeight = 24;
            visualFrameCount = 4;
            visualFrameTicks = 1;
        }
    } else if (id == "rolling-shield") {
        // S7 (2026-06-11, s7_decode_rollingshield): the ball is a 4-phase
        // 31x30 spin (cells 32x40, anchor at (16,20) — the art hangs 4px
        // below the RAM anchor, hence visualOffsetY); the charged shield is
        // the stable 46x46 bubble centered on X (cell 48x56, anchor (24,28);
        // spr10, seen 106 frames — the 7f growth pop-in is $open).
        visualStyle = ProjectileVisualStyle::RollingShield;
        if (charged) {
            // U42 box 2 (shield_vram f211-460, oid 18): the release plays a
            // 37-frame X-centered effect — contracting sparkle ring (spr0-4),
            // ring pulse (3/4/2), bubble-forming flashes (7,5,6,8,9) — then
            // the stable bubble (spr10, the old orbit cell) persists. Sheet
            // = the temporal sequence, 1 cell per frame, bubble appended as
            // the loop cell. The 96x96 cells bake the measured centering, so
            // the glued hitbox renders with offset 0 (U42 README).
            visualSpritePath = "content/x1/sprites/weapons/rolling_shield_release.png";
            visualFrameWidth = 96;
            visualFrameHeight = 96;
            visualFrameCount = 38;
            visualIntroCells = 37;
            visualFrameTicks = 1;
            visualOffsetY = 0.0f;   // cells are effect-centered already
        } else {
            visualSpritePath = "content/x1/sprites/weapons/rolling_shield_shot.png";
            visualFrameWidth = 32;
            visualFrameHeight = 40;
            visualFrameCount = 4;
            visualFrameTicks = 2;
            // U28 ground-line fix (SM feet-visible fixture, sm_ball_s1 OAM):
            // the real art rides anchor+13..+45 ahead (right-facing) with its
            // bottom ON the floor line, breathing 0-2px up per spin phase
            // (breathing is baked in the cells: bottoms rows 37-39). Engine
            // rest = hitbox bottom on the floor (pos.y+14) -> art bottom
            // (cellTop+40) must equal pos.y+14: offY = -13. The RAM anchor
            // already spawns at X-anchor+16; the rendered ball center is
            // X-anchor+29, so this visual lead is +13 from the hitbox center.
            visualOffsetX = 13.0f;
            visualOffsetY = -13.0f;
        }
    } else if (id == "homing-torpedo") {
        visualStyle = ProjectileVisualStyle::HomingTorpedo;
        visualScale = 1.0f;  // full-size VRAM rips, both tiers
        if (charged) {
            // S7 (2026-06-10): the five fan members are FISH — one fixed
            // measured 16x16 cell per slot (E,S,N,NE,SE; HV-as-fired-right
            // baked), separate charged art set (same spr12 hashes differ
            // from the normal body). Fan spawn sets visualFixedFrame.
            visualSpritePath = "content/x1/sprites/weapons/homing_torpedo_charged.png";
            visualFrameWidth = 16;
            visualFrameHeight = 16;
            visualFrameCount = 5;
        } else {
            // S7 (2026-06-10): rotation by VRAM re-upload — 5 artworks x
            // OAM flips = 32 headings, baked into 32 cells indexed by
            // homingHeading (tile center == visual center == anchor +
            // (-3,+4), so the centered draw lands pixel-exact).
            visualSpritePath = "content/x1/sprites/weapons/homing_torpedo_shot.png";
            visualFrameWidth = 16;
            visualFrameHeight = 16;
            visualFrameCount = 32;
            visualHeadingIndexed = true;
        }
    } else if (id == "boomerang-cutter") {
        visualStyle = ProjectileVisualStyle::BoomerangCutter;
        visualScale = 1.0f;  // both strips are full-size VRAM rips
        if (charged) {
            // S7 (2026-06-11) + U110: the four giants cycle spr0-7, ONE
            // frame per state; multi-tile clusters composited per state from
            // the UP giant's OAM. U110 expanded the provisional 56x56 cells
            // to 104x144 so the lower crescent is not clipped; anchor stays
            // on the cell center (build_s7_cutter_content.py over s3_vram).
            visualSpritePath = "content/x1/sprites/weapons/boomerang_cutter_charged.png";
            visualFrameWidth = 104;
            visualFrameHeight = 144;
            visualFrameCount = 8;
            visualFrameTicks = 1;
        } else {
            // S7 (2026-06-11): 8-state spin cycle spr129-136 (tiles 34/36/38
            // with per-state flips), TWO frames per state; one 16x16 tile in
            // 20x20 cells with the visual center (anchor + (-6,+5)) on the
            // cell center (build_s7_cutter_content.py over s1_vram).
            visualSpritePath = "content/x1/sprites/weapons/boomerang_cutter_shot.png";
            visualFrameWidth = 20;
            visualFrameHeight = 20;
            visualFrameCount = 8;
            visualFrameTicks = 2;
        }
    } else if (id == "chameleon-sting") {
        visualStyle = ProjectileVisualStyle::ChameleonSting;
        // S7 (2026-06-11, s1_vram via build_s7_sting_content.py): ONE strip,
        // 15 cells 52x34 with the RAM anchor at in-cell (16,14) -> draw
        // correction (+10,+3) to the cell center. Cells 0-11 = the muzzle
        // bolt's TEMPORAL sequence (128(==129 bitmap), 130..134, 133
        // reprise, 135, 136, 137 x3 pads — no modulo wrap inside the 23f
        // life) at 2f/cell; cells 12-14 = the darts (fixed per direction,
        // scene sets visualFixedFrame=12+cell and widens count to 15).
        // Pixel-verified 0-mismatch vs same-run screenshots (underground
        // pixels floor-occluded; down dart proven as the up dart's v-flip).
        visualSpritePath = "content/x1/sprites/weapons/chameleon_sting_shot.png";
        visualFrameWidth = 52;
        visualFrameHeight = 34;
        visualFrameCount = 12;   // muzzle anim window; darts override to 15
        visualFrameTicks = 2;
        visualOffsetX = 10.0f;
        visualOffsetY = 3.0f;
    } else {
        visualStyle = ProjectileVisualStyle::EnemyOrb;
    }
}

void Projectile::applyEnemyVisual() {
    visualStyle = ProjectileVisualStyle::EnemyOrb;
    visualSpritePath.clear();
    visualFrameWidth = 0;
    visualFrameHeight = 0;
    visualFrameCount = 0;
    visualScale = 1.0f;
    visualMirrorsWithFacing = true;
    sledMovingSpritePath.clear();
    sledMovingFrameWidth = 0;
    sledMovingFrameHeight = 0;
    sledMovingFrameCount = 0;
}

void Projectile::update(float /*dt*/) {
    if (!active) return;
    if (renderSuppressFrames > 0) renderSuppressFrames--;

    if (dormantFrames > 0) {
        // Not yet "claimed" (Electric Spark backward twin): inert, undrawn,
        // untraced until the SNES slot-claim frame.
        dormantFrames--;
        prevPosition = position;
        return;
    }

    if (holdFirstTick) {
        // Spawn tick: stay at the measured spawn position (WP-C); the SNES
        // bullet's first visible frame is unmoved, motion starts next frame.
        holdFirstTick = false;
        prevPosition = position;
        return;
    }

    prevPosition = position;
    ageFrames++;

    if (ageFrames <= launchDelayFrames) {
        // Stationary formation window (Electric Spark charged giants:
        // 12 frames before motion, oracle s3_charged f272-283).
        return;
    }

    // weapons/buster/normal_motion.json: accelerate before moving, then
    // cap the stored velocity. A capped shot moves 6.25 px and stores 6.
    const bool normalBuster = isPlayerShot && weaponId == "buster" &&
                              type == ProjectileType::Normal;
    if (sourceChargeL1Law && sourceChargeHitProfile()) {
        // charge_l1_birth_2026-09-17.json: live words hold for ten frames,
        // then advance six pixels, including the first flight update.
        const float direction = vx == 0 ? (facingRight ? 1.0f : -1.0f)
                                        : (vx > 0 ? 1.0f : -1.0f);
        position.x += direction * source_charge_l1::stepPxForFrame(ageFrames - 1);
    } else {
        if (normalBuster) vx += facingRight ? 0.25f : -0.25f;
        position.x += vx;
        position.y += vy;
        if (normalBuster) vx = std::clamp(vx, -6.0f, 6.0f);
    }

    // U219: ARM L3 helix uses source bullet y-offset tables for the first
    // visible/damaging window. Long misses repeat the measured 24f cadence.
    if (l3HelixVariant >= 0 &&
        l3HelixVariant < static_cast<int>(BUSTER_L3_HELIX_Y.size())) {
        const int t = ageFrames - launchDelayFrames - 1;
        if (t >= 0) {
            const auto& offsets = BUSTER_L3_HELIX_Y[static_cast<size_t>(l3HelixVariant)];
            position.y = sineBaseY + offsets[static_cast<size_t>(t) % offsets.size()];
        }
    } else if (sinePeriodFrames > 0) {
        const int t = (ageFrames - launchDelayFrames) + sinePhaseFrames;
        position.y = sineBaseY + sineAmplitude *
                     std::sin(6.2831853f * static_cast<float>(t)
                              / static_cast<float>(sinePeriodFrames));
    }

    // R7.eggs: the measured ballistic drop, uncapped and applied after the
    // move -- the source's egg reads dy 0, 64, 128, ... 1856 fp from a launch
    // vy of 64 fp, so the acceleration lands on the velocity the NEXT frame
    // uses (boss_se_egg_drop.h).
    if (ballisticGravity != 0.0f) {
        vy += ballisticGravity;
    }

    if (rolling) {
        vy += projGravity;
        if (vy > 4.0f) vy = 4.0f;
    }

    // Ground-following projectiles without a sled formation phase (the U29
    // fire-wave head) need their own gravity; the sled's lives inside the
    // sledLaunchFrame machine below.
    if (groundFollow && sledLaunchFrame == 0 && !rolling) {
        vy += projGravity;
        if (vy > 4.0f) vy = 4.0f;
    }

    // U19 bat mine: gravity fall until the scene's tile pass lands it
    // (which zeroes both velocities; the re-snap keeps it stable).
    if (mineHazard) {
        vy += projGravity;
        if (vy > 4.0f) vy = 4.0f;
    }

    // Charged Shotgun Ice sled phase machine (oracle 2026-06-09): stationary
    // formation until sledLaunchFrame, then accelerate toward facing at
    // sledAccel px/f^2 up to sledMaxSpeed. Vertical settle/ground contact is
    // handled by the scene's tile pass (groundFollow), gravity like rolling.
    if (sledLaunchFrame > 0) {
        if (ageFrames < sledLaunchFrame) {
            vx = 0;
            vy += projGravity;          // settle onto the ground during formation
            if (vy > 2.0f) vy = 2.0f;   // measured settle terminal ~2.0 px/f
        } else {
            const float dir = sledFacingRight ? 1.0f : -1.0f;
            vx += dir * sledAccel;
            if (vx >  sledMaxSpeed) vx =  sledMaxSpeed;
            if (vx < -sledMaxSpeed) vx = -sledMaxSpeed;
            vy += projGravity;
            if (vy > 4.0f) vy = 4.0f;
        }
    }

    if (boomerang) {
        // Boomerang Cutter (oracle 2026-06-11, _boomerang_cutter_runs):
        // constant-magnitude flight on the 32-entry SNES slope table.
        // Quadrant entries at raw 2048; other magnitudes = floor(entry *
        // mag/2048) — bit-identical to every measured pair (1152 gives
        // 1028/513, 920/690, 812/812, 690/920, 513/1028, 279/1116, 0/1152).
        static constexpr int kQuad[9][2] = {
            {2048, 0}, {1984, 496}, {1828, 912}, {1636, 1228}, {1444, 1444},
            {1228, 1636}, {912, 1828}, {496, 1984}, {0, 2048},
        };
        auto tableVel = [&](int idx, float& ovx, float& ovy) {
            idx = ((idx % 32) + 32) % 32;
            const int q = idx / 8, j = idx % 8;
            int vxr, vyr;
            switch (q) {  // screen coords: +x right, +y down; idx grows clockwise
                case 0:  vxr =  kQuad[j][0];     vyr =  kQuad[j][1];     break;
                case 1:  vxr = -kQuad[8 - j][0]; vyr =  kQuad[8 - j][1]; break;
                case 2:  vxr = -kQuad[j][0];     vyr = -kQuad[j][1];     break;
                default: vxr =  kQuad[8 - j][0]; vyr = -kQuad[8 - j][1]; break;
            }
            ovx = std::floor(vxr * static_cast<float>(boomMagRaw) / 2048.0f) / 256.0f;
            ovy = std::floor(vyr * static_cast<float>(boomMagRaw) / 2048.0f) / 256.0f;
        };
        boomerangTimer++;
        // First-turn route decision (oracle s5_route_REPORT 2026-06-10,
        // 55/56 decisions): at the decision tick, turn DOWN iff X's body
        // center (returnY, scene-live) has fallen below the flight line
        // (our own center — still in straight flight here). DOWN swaps in
        // the measured 21f straight + 14deg first entry (step 1, no skip);
        // otherwise the spawn UP defaults (19f, 2-entry skip) stand.
        if (boomDecisionTick > 0 && !boomRouteDecided
            && boomerangTimer >= boomDecisionTick) {
            boomRouteDecided = true;
            const float flightY = position.y + hitboxSize.y * 0.5f;
            if (boomSteer && returnY > flightY) {
                boomRotSense = -boomRotSense;        // first turn TOWARD X
                boomStraightFrames = boomDownStraightFrames;
                boomFirstStep = boomDownFirstStep;
            }
        }
        if (boomerangTimer > boomStraightFrames) {
            const int t = boomerangTimer - boomStraightFrames - 1;
            if (t % boomTurnEvery == 0) {
                int step;
                if (!boomTurned) {
                    step = boomRotSense * boomFirstStep;  // first turn tick (normal: 2 entries)
                    boomTurned = true;
                } else if (boomSteer) {
                    // Bang-bang toward the LIVE bearing to X (returnX/Y =
                    // player center, scene-updated each frame).
                    const float dx = returnX - (position.x + hitboxSize.x * 0.5f);
                    const float dy = returnY - (position.y + hitboxSize.y * 0.5f);
                    const float ang = std::atan2(dy, dx);  // screen y-down, cw-positive
                    int target = static_cast<int>(std::lround(ang / (2.0f * 3.14159265f) * 32.0f));
                    target = ((target % 32) + 32) % 32;
                    const int diff = ((target - boomAngleIdx) % 32 + 32) % 32;
                    step = (diff == 0) ? boomRotSense : (diff <= 16 ? 1 : -1);
                } else if (boomTurnSteps < 30) {
                    // U46 (build/u46/charged): the real giants step the
                    // 32-entry table ~30 times (one curl, f234-263) then
                    // HOLD the final heading and exit straight — the old
                    // endless monotonic rotation was David's "dancing too
                    // much on the screen".
                    step = boomRotSense;
                } else {
                    step = 0;                              // exit straight
                }
                boomTurnSteps++;
                boomAngleIdx = ((boomAngleIdx + step) % 32 + 32) % 32;
            }
            tableVel(boomAngleIdx, vx, vy);
        }
    }

    if (homing) {
        // Homing Torpedo steering (oracle 2026-06-12, FINDINGS.md — the
        // model replays 2321 oracle frames with zero deviations). All math
        // in raw SNES sub-units (v_px * 256, exact in float).
        auto raw = [](float v) { return static_cast<int>(std::lround(v * 256.0f)); };
        int vxr = raw(vx), vyr = raw(vy);
        // T[d]: 32-dir accel table (0=up, clockwise), y SCREEN-DOWN.
        auto tableT = [&](int d, int& ax, int& ay) {
            d = ((d % 32) + 32) % 32;
            const int* Q = homingTableQRaw;
            int x, yUp;
            if (d <= 8)       { x =  Q[8 - d];  yUp =  Q[d]; }
            else if (d <= 16) { x =  Q[d - 8];  yUp = -Q[16 - d]; }
            else if (d <= 24) { x = -Q[24 - d]; yUp = -Q[d - 16]; }
            else              { x = -Q[d - 24]; yUp =  Q[32 - d]; }
            ax = x; ay = -yUp;
        };
        if (homingCountdown > 0) {
            // Launch: straight accel along facing, straight cap.
            homingCountdown--;
            const int dir = vxr >= 0 ? 1 : -1;
            vxr += homingAccelRaw * dir;
            if (vxr * dir > homingStraightCapRaw) vxr = homingStraightCapRaw * dir;
        } else {
            // SNES per frame: v += T; pos += v; v = cap(v). The engine moved
            // above with last frame's post-accel velocity, so cap it NOW
            // (with the pre-rotation heading), then rotate, then accelerate.
            auto dist = [&](int hd, int c0, int c1) {
                auto cd = [&](int c) {
                    int d = ((hd - c) % 32 + 32) % 32;
                    return d < 32 - d ? d : 32 - d;
                };
                const int a = cd(c0), b = cd(c1);
                return a < b ? a : b;
            };
            const int cx = homingXcapRaw[dist(homingHeading, 0, 16)];
            const int cy = homingXcapRaw[dist(homingHeading, 8, 24)];
            if (vxr >  cx) vxr =  cx;
            if (vxr < -cx) vxr = -cx;
            if (vyr >  cy) vyr =  cy;
            if (vyr < -cy) vyr = -cy;
            // Bearing: quantize the direction to the live target (octant +
            // slope ratio at eighth boundaries). No target: hold heading.
            int bearing = homingHeading;
            if (homingHasTarget) {
                const float dx = returnX - (position.x + hitboxSize.x * 0.5f);
                const float dyUp = -(returnY - (position.y + hitboxSize.y * 0.5f));
                const float au = dx < 0 ? -dx : dx, av = dyUp < 0 ? -dyUp : dyUp;
                if (au > 0 || av > 0) {
                    const float big = au > av ? au : av;
                    const float small = au > av ? av : au;
                    const float r = small / big;
                    const int off = (r > 0.125f) + (r > 0.375f)
                                  + (r > 0.625f) + (r > 0.875f);
                    if (au >= av) {
                        if (dx > 0) bearing = dyUp > 0 ? 8 - off : 8 + off;
                        else        bearing = dyUp > 0 ? 24 + off : 24 - off;
                    } else {
                        if (dyUp > 0) bearing = dx > 0 ? off : 32 - off;
                        else          bearing = dx > 0 ? 16 - off : 16 + off;
                    }
                    bearing = ((bearing % 32) + 32) % 32;
                }
            }
            // Heading: +/-1 toward bearing, short way, tie -> decrement.
            bool ccwLanded = false;
            int arc = ((bearing - homingHeading) % 32 + 32) % 32;
            if (arc != 0) {
                const int step = (arc != 0 && arc < 16) ? 1 : -1;
                homingHeading = ((homingHeading + step) % 32 + 32) % 32;
                ccwLanded = (step == -1) && (homingHeading % 8 == 0);
            }
            // CCW landing on a cardinal zeroes that axis (measured
            // asymmetry; cw pass-throughs decay by table instead).
            if (ccwLanded && homingCcwZero) {
                if (homingHeading % 16 == 0) vxr = 0;  // 0/16 zero x
                else                         vyr = 0;  // 8/24 zero y
            }
            // Accelerate at the bearing FOLDED across the first cardinal
            // strictly between heading and bearing (no thrust reversal
            // until the heading has rotated past it).
            int eff = bearing;
            arc = ((bearing - homingHeading) % 32 + 32) % 32;
            if (arc != 0) {
                const int sgn = arc < 16 ? 1 : -1;
                const int n = arc < 16 ? arc : 32 - arc;
                for (int i = 1; i < n; i++) {
                    const int pos32 = ((homingHeading + sgn * i) % 32 + 32) % 32;
                    if (pos32 % 8 == 0) {
                        eff = ((2 * pos32 - bearing) % 32 + 32) % 32;
                        break;
                    }
                }
            }
            int ax, ay;
            tableT(eff, ax, ay);
            vxr += ax;
            vyr += ay;
        }
        vx = vxr / 256.0f;
        vy = vyr / 256.0f;
    }

    if (torpedoFanMember && !homing) {
        // Charged torpedo fan member: accel1 until the countdown ends, then
        // accel2; per-axis caps applied to LAST frame's velocity (the SNES
        // accel -> move -> cap order, same as the homing block).
        auto raw = [](float v) { return static_cast<int>(std::lround(v * 256.0f)); };
        int vxr = raw(vx), vyr = raw(vy);
        if (fanCapX > 0) {
            if (vxr >  fanCapX) vxr =  fanCapX;
            if (vxr < -fanCapX) vxr = -fanCapX;
        }
        if (fanCapY > 0) {
            if (vyr >  fanCapY) vyr =  fanCapY;
            if (vyr < -fanCapY) vyr = -fanCapY;
        }
        if (fanCountdown > 0) {
            fanCountdown--;
            vxr += fanA1x;
            vyr += fanA1y;
            // Homing gate (w70iso_REPORT 2026-06-10): the member steers iff
            // its release-acquired target is STILL ALIVE when the countdown
            // expires — switch to the standard homing model so steering
            // begins at expiry+1 (measured: cd==1 at f316, first heading
            // step f317), starting from the measured per-member heading.
            // Dead/no target = the accel2 branch below forever (w70: tgt
            // high byte cleared at expiry+1, straight flight; that is the
            // previously shipped behavior, kept as the no-target branch).
            if (fanCountdown == 0 && fanTargetAlive) {
                homing = true;
                homingHeading = fanHeading;
                homingCountdown = 0;   // no straight-launch phase on switch
            }
        } else {
            vxr += fanA2x;
            vyr += fanA2y;
        }
        vx = vxr / 256.0f;
        vy = vyr / 256.0f;
    }

    lifetime--;
    if (lifetime <= 0) {
        active = false;
    }

    // Gameplay scenes cull against the live camera. Keep this only as a
    // runaway-object safety net; decoded stages extend well past x=5000.
    constexpr float kSafetyWorldLimit = 100000.0f;
    if (position.x < -kSafetyWorldLimit || position.x > kSafetyWorldLimit ||
        position.y < -kSafetyWorldLimit || position.y > kSafetyWorldLimit) {
        active = false;
    }
}

void Projectile::render(float alpha) {
    render(alpha, {0.0f, 0.0f});
}

void Projectile::render(float alpha, Vector2 cameraOffset) {
    if (!active) return;
    if (dormantFrames > 0) return;  // slot not yet claimed (e-spark twin)
    if (renderSuppressFrames > 0) return;
    // U30 storm-tornado: the SNES draws it only on even frames (alternate-
    // frame flicker = its translucency); reproduce the cadence exactly.
    if (flickerAlternate && (ageFrames % 2) == 1) return;

    const float drawX = prevPosition.x + (position.x - prevPosition.x) * alpha - cameraOffset.x;
    const float drawY = prevPosition.y + (position.y - prevPosition.y) * alpha - cameraOffset.y;

    Color drawColor = color;
    if (type == ProjectileType::ChargeL1) {
        drawColor = brighten(drawColor, 50, 50, 0);
    } else if (type == ProjectileType::ChargeL2) {
        drawColor = brighten(drawColor, 80, 80, 40);
    } else if (type == ProjectileType::ChargeL3) {
        drawColor = WHITE;
    }

    if (!formationSpritePath.empty() && ageFrames < formationFrames) {
        // Charged Shotgun Ice formation (S7/WP-A): growing chunk frames
        // spr0..4. Cells are 48x16 with content at the OAM+VRAM-measured
        // placement — cell x=0 == body-left - 4, cell rows == body box rows
        // (build_s7_content.py) — so the whole cell lands pixel-exact when
        // drawn at (hitbox.x - 4, hitbox.bottom - 16); the 48px box is
        // symmetric around the 40px body, so the x anchor holds for both
        // facings with the cell content hflipped.
        const TextureResource* sheet = AssetCache::loadTexture(formationSpritePath);
        if (sheet && sheet->valid()) {
            sheet->setFilter(TEXTURE_FILTER_POINT);
            const int frame = std::min(ageFrames / 10, 4);
            Rectangle source{static_cast<float>(frame * 48), 0.0f, 48.0f, 16.0f};
            if (!facingRight) {
                source.width = -source.width;  // U49: in-place flip (U40 law)
            }
            Rectangle dest{drawX - 4.0f, drawY + hitboxSize.y - 16.0f, 48.0f, 16.0f};
            DrawTexturePro(sheet->get(), source, dest, {0.0f, 0.0f}, 0.0f, WHITE);
        }
        return;
    }

    if (rideable && !sledMovingSpritePath.empty() &&
        ageFrames >= sledLaunchFrame && std::fabs(vx) > 0.01f) {
        Projectile moving = *this;
        moving.visualSpritePath = sledMovingSpritePath;
        moving.visualFrameWidth = sledMovingFrameWidth;
        moving.visualFrameHeight = sledMovingFrameHeight;
        moving.visualFrameCount = sledMovingFrameCount;
        moving.visualFrameTicks = 1;
        moving.visualScale = 1.0f;
        moving.visualOffsetX = 0.0f;
        moving.visualOffsetY = 0.0f;
        const int m = std::max(0, ageFrames - sledLaunchFrame);
        moving.visualFixedFrame = ((m + 1) / 3) % std::max(1, sledMovingFrameCount);
        drawSpriteProjectile(moving, drawX, drawY, WHITE);
        return;
    }

    if (!visualSpritePath.empty()) {
        // The charged Rolling Shield bubble surrounds X — the SNES draws it
        // with color-math translucency; an alpha tint keeps X visible
        // (exact blend mode = audio/visual-rip campaign territory).
        const Color tint = absorbShield ? Color{255, 255, 255, 150} : WHITE;
        drawSpriteProjectile(*this, drawX, drawY, tint);
        return;
    }

    const float cx = drawX + hitboxSize.x * 0.5f;
    const float cy = drawY + hitboxSize.y * 0.5f;
    const float w = hitboxSize.x * visualScale;
    const float h = hitboxSize.y * visualScale;
    const float left = cx - w * 0.5f;
    const float top = cy - h * 0.5f;

    switch (visualStyle) {
        case ProjectileVisualStyle::ShotgunIce:
            drawDiamond(cx, cy, w * 0.55f, h * 0.55f,
                        {40, 120, 180, 255}, {170, 235, 255, 255}, WHITE);
            break;
        case ProjectileVisualStyle::StormTornado: {
            const int phase = (ageFrames / 3) % 3;
            DrawRectangle(static_cast<int>(left), static_cast<int>(top + 2 + phase),
                          static_cast<int>(w), 2, withAlpha(drawColor, 210));
            DrawRectangle(static_cast<int>(left + 2), static_cast<int>(cy - 1),
                          static_cast<int>(w - 4), 2, WHITE);
            DrawRectangle(static_cast<int>(left), static_cast<int>(top + h - 4 - phase),
                          static_cast<int>(w), 2, withAlpha(drawColor, 180));
            break;
        }
        case ProjectileVisualStyle::FireWave:
            DrawRectangle(static_cast<int>(left), static_cast<int>(top + h * 0.35f),
                          static_cast<int>(w), static_cast<int>(h * 0.45f),
                          {220, 40, 20, 255});
            DrawRectangle(static_cast<int>(left + 2), static_cast<int>(top + 1),
                          static_cast<int>(w * 0.45f), static_cast<int>(h * 0.65f),
                          {255, 140, 30, 255});
            DrawRectangle(static_cast<int>(left + w * 0.45f), static_cast<int>(top + 3),
                          static_cast<int>(w * 0.35f), static_cast<int>(h * 0.55f),
                          {255, 220, 80, 255});
            break;
        case ProjectileVisualStyle::ElectricSpark:
            DrawRectangle(static_cast<int>(cx - 1), static_cast<int>(top),
                          2, static_cast<int>(h), {255, 255, 160, 255});
            DrawRectangle(static_cast<int>(left), static_cast<int>(cy - 1),
                          static_cast<int>(w), 2, {255, 255, 80, 255});
            DrawRectangle(static_cast<int>(left + 2), static_cast<int>(top + 2),
                          static_cast<int>(w - 4), static_cast<int>(h - 4),
                          withAlpha(drawColor, 90));
            break;
        case ProjectileVisualStyle::RollingShield:
            DrawCircleV({cx, cy}, std::max(w, h) * 0.45f, withAlpha(drawColor, 90));
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy),
                            std::max(w, h) * 0.45f, {255, 245, 120, 255});
            DrawCircleV({cx, cy}, std::max(w, h) * 0.22f, {255, 255, 210, 255});
            break;
        case ProjectileVisualStyle::HomingTorpedo: {
            const float nose = facingRight ? left + w : left;
            const float tail = facingRight ? left : left + w;
            DrawTriangle({nose, cy}, {tail, top}, {tail, top + h}, {240, 240, 255, 255});
            DrawRectangle(static_cast<int>(std::min(nose, tail)), static_cast<int>(cy - 2),
                          static_cast<int>(std::abs(nose - tail)), 4, drawColor);
            break;
        }
        case ProjectileVisualStyle::BoomerangCutter: {
            const float dir = facingRight ? 1.0f : -1.0f;
            DrawTriangle({cx + dir * w * 0.5f, cy}, {cx - dir * w * 0.35f, top},
                         {cx - dir * w * 0.1f, cy}, {255, 250, 120, 255});
            DrawTriangle({cx + dir * w * 0.5f, cy}, {cx - dir * w * 0.35f, top + h},
                         {cx - dir * w * 0.1f, cy}, {230, 190, 40, 255});
            break;
        }
        case ProjectileVisualStyle::ChameleonSting:
            drawCapsule(left, top + h * 0.25f, w, h * 0.5f,
                        {20, 70, 40, 255}, drawColor, {240, 255, 220, 255});
            break;
        case ProjectileVisualStyle::EnemyOrb:
        case ProjectileVisualStyle::BusterSprite:
        case ProjectileVisualStyle::BusterCharge1:
        default:
            drawCapsule(left, top, w, h, {120, 60, 20, 255}, drawColor, WHITE);
            break;
    }
}

void renderProjectileVisualAt(const std::string& weaponId,
                              int ageFrames,
                              Vector2 center,
                              bool facingRight) {
    Projectile projectile;
    // This is a render-only sample: do not call init(), because init assigns
    // a gameplay serial. Weapon-get presentation must not consume serials or
    // mutate the live projectile identity stream.
    projectile.active = true;
    projectile.type = ProjectileType::Normal;
    projectile.hitboxSize = {8.0f, 6.0f};
    projectile.hitboxOffset = {0.0f, 0.0f};
    projectile.applyWeaponVisual(weaponId, false);
    projectile.position = {
        center.x - projectile.hitboxSize.x * 0.5f,
        center.y - projectile.hitboxSize.y * 0.5f,
    };
    projectile.prevPosition = projectile.position;
    projectile.facingRight = facingRight;
    projectile.ageFrames = std::max(0, ageFrames);
    projectile.render(1.0f, {0.0f, 0.0f});
}

} // namespace mmx
