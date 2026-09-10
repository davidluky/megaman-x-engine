// boss_cp_tables.h - oracle-transcribed Chill Penguin fight constants.
// Boundary: data only; runtime state transitions stay in Boss.

#pragma once

namespace mmx::boss_cp_tables {

// s2 bar-hang exact y-offsets vs floor, 190 frames (obs_stand_p0 f679-868).
inline const int kCpHangYOff[190] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-8,-15,-22,
    -29,-36,-42,-48,-54,-60,-65,-70,-75,-80,-84,-88,-92,-95,-99,-102,-105,-107,
    -110,-112,-114,-115,-117,-118,-119,-119,-120,-120,-120,-120,-120,-120,-120,
    -120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,
    -120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,
    -120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,-120,
    -120,-120,-120,-120,-120,-119,-119,-118,-117,-116,-114,-113,-111,-108,-106,
    -103,-100,-97,-93,-90,-86,-81,-77,-72,-67,-62,-56,-51,-45,-38,-32,-25,-18,
    -11,-3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

// s6 long-leap (body-press) exact y-offsets vs floor, 172 frames
// (obs_stand_p0, the canonical stationary-X segment; nine of the ten
// corpus 172s share this fp-fixed ladder — CP-B5a 2026-08-11). Shape:
// takeoff table frame 33, apex -140 at 65..66, descend to a -24 hover held
// f97..f126, drop, land f130, then the shared 42-frame post-landing tail.
// The single moving-X corpus segment holds a higher hover instead
// (mid-flight target tracking) and is explicitly NOT modeled here.
inline const int kCpLeapLongYOff[172] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -8, -16, -24, -31, -38, -45, -52,
    -59, -65, -71, -77, -82, -87, -92, -97, -102, -106,
    -110, -114, -117, -120, -123, -126, -129, -131, -133, -135,
    -136, -137, -138, -139, -139, -140, -140, -139, -139, -138,
    -137, -136, -134, -133, -131, -128, -126, -123, -120, -117,
    -113, -110, -106, -101, -97, -92, -87, -82, -76, -71,
    -65, -58, -52, -45, -38, -31, -24, -24, -24, -24,
    -24, -24, -24, -24, -24, -24, -24, -24, -24, -24,
    -24, -24, -24, -24, -24, -24, -24, -24, -24, -24,
    -24, -24, -24, -24, -24, -24, -24, -17, -10, -3,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0
};

// s6 targeted-leap exact y-offsets vs floor, 142 frames
// (obs2_stand_fire_p0 f50-191; x motion spans table frames 32..94).
inline const int kCpLeapYOff[142] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,-8,-16,-24,-31,-39,-46,-52,-59,-65,-71,-77,-82,-88,-93,-97,
    -102,-106,-110,-114,-117,-121,-124,-126,-129,-131,-133,-135,-136,-137,-138,-139,
    -140,-140,-140,-140,-139,-138,-137,-136,-135,-133,-131,-129,-126,-123,-120,-117,
    -114,-110,-106,-102,-97,-92,-87,-82,-77,-71,-65,-58,-52,-45,-38,-31,-24,-17,-10,
    -3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0
};

// s10 flinch hop exact y-offsets vs floor, 22 frames
// (obs2_stand_fire_p0 f28-49).
inline const int kCpFlinchYOff[22] = {
    0,0,-2,-4,-5,-6,-7,-8,-8,-8,-8,-8,-7,-6,-5,-4,-2,0,0,0,0,0
};

inline constexpr int kCpFirstIdleFrames = 336;
inline constexpr int kCpIdleFrames = 335;
inline constexpr int kCpIdleShotFrame = 100;
inline constexpr int kCpSlideWindup = 52;
inline constexpr int kCpSlideV0Fp = 1520;
inline constexpr int kCpSlideDecFp = 16;
inline constexpr int kCpVolleyFrames = 195;
inline constexpr int kCpVolleyFirstShot = 57;
inline constexpr int kCpVolleyShotSpacing = 8;
inline constexpr int kCpVolleyShotCount = 5;
inline constexpr float kCpShotMouthDX = 26.0f;
inline constexpr float kCpVolleyShotVx = 2.0f;
inline constexpr int kCpVolleyShotDmg = 4;
inline constexpr float kCpIdleShotVx = 4.0f;
inline constexpr int kCpIdleShotDmg = 2;
inline constexpr int kCpFlinchFrames = 22;

} // namespace mmx::boss_cp_tables
