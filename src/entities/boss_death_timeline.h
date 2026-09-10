// boss_death_timeline.h - per-boss death choreography table family.
//
// SE-B8 slice 1 proved the death GRAMMAR generalises across bosses: Storm
// Eagle repeats Chill Penguin's $13 cry at d0, the $21 explosion-start cue one
// frame before the first pop, and the same 4f/5f/6f pop cadence over $93..$96,
// differing only in the offsets table and count. (The FLINCH rule, by
// contrast, did NOT generalise - it is Chill Penguin's alone.) So the death
// choreography is data selected by boss type, not a CP-only code path.
//
// Adding a boss here needs its own measured death_timeline.json plus a
// transcription pin re-deriving the offsets from that boss's committed
// corpus; never hand-copy another boss's table.

#pragma once

#include "entities/boss_cp_death_timeline.h"
#include "entities/boss_se_death_timeline.h"

#include <string>

namespace mmx {

struct BossDeathTimeline {
    int cryApuCommand;      // APU command on the killing frame (death-local d0)
    int popApuCommandBase;  // roll(4) picks base..base+3
    int popCount;
    int slotHoldTicks;      // defeated slot lingers this long, then despawns
    const int* popOffsets;  // popCount death-local offsets, strictly ascending
};

// Returns nullptr for bosses with no measured death window; those keep the
// generic engine death pipeline.
inline const BossDeathTimeline* bossDeathTimelineFor(const std::string& type) {
    static constexpr BossDeathTimeline kChillPenguin{
        boss_cp_death_timeline::kCryApuCommand,
        boss_cp_death_timeline::kPopApuCommandBase,
        boss_cp_death_timeline::kPopCount,
        boss_cp_death_timeline::kSlotHoldTicks,
        boss_cp_death_timeline::kPopOffsets,
    };
    // NOTE: Storm Eagle's hold is a measured FLOOR, not the despawn law - the
    // corpus ends at frame 19517 with the slot still live. See
    // boss_se_death_timeline.h.
    static constexpr BossDeathTimeline kStormEagle{
        boss_se_death_timeline::kCryApuCommand,
        boss_se_death_timeline::kPopApuCommandBase,
        boss_se_death_timeline::kPopCount,
        boss_se_death_timeline::kSlotHoldTicksMeasuredFloor,
        boss_se_death_timeline::kPopOffsets,
    };
    if (type == "chill-penguin") return &kChillPenguin;
    if (type == "storm-eagle") return &kStormEagle;
    return nullptr;
}

} // namespace mmx
