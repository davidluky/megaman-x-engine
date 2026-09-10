// boss_cp_death_timeline.h - pure death-window CP choreography constants.
// $source: the b8_death_window committed-corpus reduction
// (docs/evidence/2026-08-11-fb2-cp-b8-death-timeline/): killing hit at
// death-local d0 lands DURING the volley and defeats same-frame with the
// $13 cry; 78 explosion pops over $93..$96 run d+64..d+397 on a measured
// cadence (4f x36, then 6f x12, one 5f seam, then 4f x30); the defeated
// slot lingers through a 529-frame tail (freed after d+528). Pop VARIANT
// choice is uniform-random over the four ids (observed counts
// 93:20 94:19 95:16 96:23); pop TIMES are the exact observed offsets,
// deterministic-until-refuted — CP-B3F-X's second fight decides
// exact-vs-distribution.

#pragma once

namespace mmx::boss_cp_death_timeline {

inline constexpr int kCryApuCommand = 0x13;
inline constexpr int kPopApuCommandBase = 0x93;  // roll(4) -> $93..$96
inline constexpr int kPopCount = 78;
inline constexpr int kSlotHoldTicks = 528;       // Dead/freed after this
inline constexpr int kPopOffsets[kPopCount] = {
    64, 68, 72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112,
    116, 120, 124, 128, 132, 136, 140, 144, 148, 152, 156, 160, 164,
    168, 172, 176, 180, 184, 188, 192, 196, 200, 204, 210, 216, 222,
    228, 234, 240, 246, 252, 258, 264, 270, 276, 281, 285, 289, 293,
    297, 301, 305, 309, 313, 317, 321, 325, 329, 333, 337, 341, 345,
    349, 353, 357, 361, 365, 369, 373, 377, 381, 385, 389, 393, 397,
};

} // namespace mmx::boss_cp_death_timeline
