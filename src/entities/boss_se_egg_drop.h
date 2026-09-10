// boss_se_egg_drop.h - Storm Eagle's measured egg drop.
//
// $source: knowledge_base/mmx1/bosses/storm-eagle/attack_kinematics.json
// `eggDrop`, and, independently, David's Storm Eagle movie (plan task
// R7.0x55, harvest knowledge_base/_movies/storm-eagle/enemies.csv frames
// 16707..16737, the OID 0x55 instance).
//
// The program is deterministic, so the BOSS pushes the egg rather than a
// callback: it spawns on the Gust step's frame 169 (5/5 live gusts in the
// observation corpus, and the movie's f16538 + 169 = f16707), at an offset of
// (+18, -6) from a boss holding at (6043, 106) with the egg at (6061, 100).
//
// The trajectory, in the source's 1/256 px fixed point, over its 31 rows:
//   dx: 0, then +512 twenty-nine times      = 14,848 fp = 58 px
//   dy: 0, 64, 128, 192, ..., 1856          = 27,840 fp = 108.75 px
// One HOLD frame, then a constant 2 px/frame on x while y accelerates by
// exactly +64 fp every frame, with NO cap -- the drop passes 7.25 px/frame,
// well past the 4 px/frame the engine's other gravity terms clamp to.
// `Projectile::update` moves and THEN accelerates, so the launch vy that
// reproduces the sequence frame for frame is +64 fp, not zero.
#pragma once

namespace mmx::boss_se_egg {

// Frames into the Gust step at which the egg leaves the boss.
inline constexpr int kSpawnFrameInGust = 169;
// The egg's spawn offset from the boss's position.
inline constexpr float kSpawnOffsetX = 18.0f;
inline constexpr float kSpawnOffsetY = -6.0f;
// 512 fp per frame.
inline constexpr float kVelocityX = 2.0f;
// The launch vy under move-then-accelerate; 64 fp.
inline constexpr float kVelocityY = 0.25f;
// 64 fp per frame squared, uncapped.
inline constexpr float kGravity = 0.25f;
// The egg's last logged row is the frame before the hatch, which is the Gust
// step's frame 200 (16738 = 16537 + 201 = the egg's last row + 1).
// The hatch is 31 frames after the spawn -- movie f16707 + 31 = f16738, the
// frame after the egg's last row. The Gust step itself is only 200 frames
// long, so that frame is already the FIRST frame of the following Hover step;
// counting from the egg's own spawn instead of from the step keeps the law
// where it was measured and does not depend on where the step boundary falls.
inline constexpr int kHatchDelayFromSpawn = 31;
inline constexpr int kHatchFrameInGust = kSpawnFrameInGust + kHatchDelayFromSpawn;
// The source logs 31 rows: the spawn row, one hold delta, then 29 moving
// deltas. The engine's projectile does not spend a lifetime frame on its
// spawn-tick hold, so 30 is the count that leaves it on its last measured row
// and removes it on the hatch frame.
inline constexpr int kLifetimeFrames = 30;

// Not a measurement: no hit in either movie is the egg's, so this is the
// stage data's own authored egg_drop damage
// (knowledge_base/mmx1/bosses/storm-eagle/boss.json, source
// "engine_extraction"), the waiver-W2 stand-in the R1 enemies use for an
// unobserved contact damage.
inline constexpr int kProvisionalDamage = 2;

inline constexpr const char* kSpritePath =
    "content/x1/sprites/enemies/se_egg_sheet.png";
inline constexpr float kSpriteWidth = 16.0f;
inline constexpr float kSpriteHeight = 16.0f;

// The hatch (OID 0x56, plan task R7.0x56). All FOUR eaglets appear on the
// frame after the egg's last row -- the Gust step's frame 200 -- at
// (6121, 215) against an egg that spawned at (6061, 100), so the offset from
// the egg's own spawn is (+60, +115). Each takes one of the four diagonals
// the KB's `slotDiagonals` names, at +/-361 fixed point per axis (a 512 fp
// step split 45 degrees). What differs is the LIFETIME: the two +x ones stop
// being logged after 6 frames and the two -x ones run 57.
inline constexpr float kHatchOffsetXFromEgg = 60.0f;
inline constexpr float kHatchOffsetYFromEgg = 115.0f;
inline constexpr int kEagletSpeedFp = 361;
inline constexpr int kEagletCount = 4;
struct Eaglet {
    int vxFp;
    int vyFp;      // positive is DOWN, the way the movie's rows integrate
    int lifeFrames;
};
// Screen terms: slot 2 up-left, 3 down-left, 4 down-right, 5 up-right.
inline constexpr Eaglet kEaglets[kEagletCount] = {
    {-kEagletSpeedFp, -kEagletSpeedFp, 57},
    {-kEagletSpeedFp, +kEagletSpeedFp, 57},
    {+kEagletSpeedFp, +kEagletSpeedFp, 6},
    {+kEagletSpeedFp, -kEagletSpeedFp, 6},
};
inline constexpr const char* kEagletEnemyId = "se_eaglet";

} // namespace mmx::boss_se_egg
