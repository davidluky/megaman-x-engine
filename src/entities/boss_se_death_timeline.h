// boss_se_death_timeline.h - pure death-window Storm Eagle choreography.
// $source: the se_movie_dense committed-corpus reduction
// (knowledge_base/mmx1/bosses/storm-eagle/death_timeline.json, SE-B8 slice 1):
// the killing hit at death-local d0 defeats same-frame with the $13 cry, then
// 79 explosion pops over $93..$96 run d+63..d+406 on the SAME cadence
// grammar Chill Penguin uses (4f x62, 6f x15, one 5f seam) - SE-B8 proved the
// death grammar GENERALISES across bosses, unlike the flinch rule.
//
// SE-specific trap: Storm Eagle writes the SFX mailbox through DB=$00
// ($002140), not $80 like Chill Penguin. The same corpus carries a $862140
// mirror carrying a LATER burst (d+539..d+555); filtering on it yields 254
// false pops. tools/tests/test_se_death_timeline_pin.py pins the addr.
//
// NOT A LAW: kSlotHoldTicksMeasuredFloor is a FLOOR, not the despawn offset.
// The corpus ENDS at frame 19517 with slot 1 still live, so how long the
// defeated slot really lingers is not pinnable from this capture. The engine
// holds at least this long, which is all the evidence supports; a longer
// capture is needed to promote it to a law (SE-B5 E1 follow-up row).

#pragma once

namespace mmx::boss_se_death_timeline {

inline constexpr int kCryApuCommand = 0x13;
inline constexpr int kPopApuCommandBase = 0x93;  // roll(4) -> $93..$96
inline constexpr int kPopCount = 79;
inline constexpr int kSlotHoldTicksMeasuredFloor = 535;  // floor, see above
inline constexpr int kPopOffsets[kPopCount] = {
    63, 67, 71, 75, 79, 83, 87, 91, 95, 99, 103, 107, 111, 115, 119,
    123, 127, 131, 135, 139, 143, 147, 151, 155, 159, 163, 167, 171,
    175, 179, 183, 187, 191, 196, 202, 208, 214, 220, 226, 232, 238,
    244, 250, 256, 262, 268, 274, 280, 286, 290, 294, 298, 302, 306,
    310, 314, 318, 322, 326, 330, 334, 338, 342, 346, 350, 354, 358,
    362, 366, 370, 374, 378, 382, 386, 390, 394, 398, 402, 406
};

} // namespace mmx::boss_se_death_timeline
