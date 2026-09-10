// stage_start_timeline.h - source-clock READY and warp-in entry choreography.
// Boundary: presentation/control timing only; stage setup owns spawn selection.

#pragma once

namespace mmx::stage_start_timeline {

// Source anchor: Chill Penguin stage-entry frame 799 is runtime tick zero.
inline constexpr int kSourceFrameZero = 799;
inline constexpr int kReadyLastTick = 110;       // source frame 909
inline constexpr int kWarpFirstTick = 112;       // source frame 911
inline constexpr int kControlFirstTick = 142;    // source frame 941
inline constexpr int kLastTick = 142;
inline constexpr int kWarpDuration = 31;         // source frames 911..941
inline constexpr int kCapsuleLastTick = 133;    // source frame932, before the burst

// R336: knowledge_base/mmx1/story/stage_entry_warp.json. Source OAM N is
// displayed at N+1; the first capsule image is held for source911/912.
inline constexpr int capsuleScreenTopAt(int tick) {
    if (tick == kCapsuleLastTick) return 127;
    const int descentSteps = tick > 113 ? tick - 113 : 0;
    return -32 + 8 * descentSteps;
}

inline constexpr int materializeCellAt(int tick) {
    if (tick < 134 || tick > kLastTick) return -1;
    if (tick <= 135) return 0;
    if (tick >= 140) return 5;
    return tick - 135;
}

struct Plan {
    bool active = false;
    bool readyVisible = false;
    bool warpActive = false;
    bool controlsLocked = false;
    int sourceFrame = -1;
};

inline constexpr bool readyVisibleAt(int tick) {
    return (tick >= 0 && tick <= 13) ||
           (tick >= 22 && tick <= 29) ||
           (tick >= 38 && tick <= 45) ||
           (tick >= 54 && tick <= 61) ||
           (tick >= 70 && tick <= 77) ||
           (tick >= 86 && tick <= 93) ||
           (tick >= 102 && tick <= 110);
}

inline constexpr int readyRasterCellAt(int tick) {
    if (!readyVisibleAt(tick)) return -1;
    // R297 source799/800 and801/802 animate the final glyph before the
    // stable source803 pattern. Later flashes reuse that complete word.
    return tick < 2 ? 0 : (tick < 4 ? 1 : 2);
}

inline constexpr int warpTimerAt(int tick) {
    if (tick < kWarpFirstTick || tick > kLastTick) return 0;
    const int age = tick - kWarpFirstTick;
    if (age <= 1) return 29;   // source frames 911/912 are byte-identical
    if (age >= 29) return 1;   // keep the reveal phase through source 941
    return 30 - age;
}

inline constexpr Plan evaluate(int tick) {
    if (tick < 0 || tick > kLastTick) return Plan{};
    return Plan{
        true,
        readyVisibleAt(tick),
        tick >= kWarpFirstTick,
        tick < kControlFirstTick,
        kSourceFrameZero + tick,
    };
}

} // namespace mmx::stage_start_timeline
