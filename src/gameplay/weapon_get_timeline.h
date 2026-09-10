// weapon_get_timeline.h - pure source-clock choreography for the post-boss
// weapon-get sequence (GC2).
//
// Oracle: knowledge_base/mmx1/story/weapon_get_sequence.json, reduced from
// David's 2026-07-26 Chill Penguin and Storm Eagle playthrough movies. Ticks
// here are relative to the victory cue, which the source puts at movie frame
// f17819 for Chill Penguin.
//
// The Storm Eagle cross-check showed the choreography is SHARED across bosses
// while the content is per-weapon, so the phase/warp timing below is fixed and
// only the typed-name length and demo pattern come in through Params. Six of
// the eight Mavericks are still untested.
//
// No rendering, no audio, no engine state: this module answers "what should be
// happening at tick N", nothing else.

#pragma once

namespace mmx::weapon_get_timeline {

// All relative to the victory cue at tick 0.
inline constexpr int kWarpOutStartTick = 57;    // source f17876
inline constexpr int kWarpOutEndTick = 80;      // source f17899
inline constexpr int kSpecScreenTick = 172;     // source f17991
inline constexpr int kTextStartTick = 355;      // source f18174
inline constexpr int kTextStepTicks = 4;
inline constexpr int kWarpInStartTick = 555;    // source f18374
inline constexpr int kWarpInEndTick = 571;      // source f18390
inline constexpr int kDemoFirstTick = 592;      // source f18411
inline constexpr int kReturnTick = 898;         // source f18717
inline constexpr int kSourceHandoffEndTick = 1131; // source f18950, CP only
inline constexpr int kStormSourceHandoffEndTick = 1146; // source f21054

inline constexpr int kWarpOutStartY = 431;
inline constexpr int kWarpOutEndY = 186;
inline constexpr int kWarpInStartY = 256;
inline constexpr int kWarpInEndY = 384;
inline constexpr int kWarpInStepY = 8;

enum class Phase {
    Inactive,
    VictoryPose,
    WarpOut,
    SpecScreen,
    WarpIn,
    Demo,
    Return,
};

struct Params {
    // One typed character per blip, so this is the weapon name's length.
    int textBlipCount = 17;      // Chill Penguin, measured
    int demoShotCount = 3;       // Chill Penguin, measured
    int demoShotStepTicks = 40;  // Chill Penguin, measured
};

struct Frame {
    Phase phase = Phase::Inactive;
    int playerY = 0;
    bool emitTextBlip = false;
    bool emitDemoShot = false;
    bool screenBlank = false;
};

// The warp-out ladder is NOT uniform: the source alternates 10 and 11 px per
// tick, averaging 10.652. Publishing the mean would be wrong on every other
// tick, so the ladder is reconstructed exactly by distributing the 245 px of
// travel across 23 ticks and letting the rounding fall where the source put
// it.
inline int warpOutY(int tick) {
    if (tick <= kWarpOutStartTick) return kWarpOutStartY;
    if (tick >= kWarpOutEndTick) return kWarpOutEndY;
    const int elapsed = tick - kWarpOutStartTick;
    const int span = kWarpOutEndTick - kWarpOutStartTick;
    const int travel = kWarpOutStartY - kWarpOutEndY;
    return kWarpOutStartY - (travel * elapsed + span / 2) / span;
}

inline int warpInY(int tick) {
    if (tick <= kWarpInStartTick) return kWarpInStartY;
    if (tick >= kWarpInEndTick) return kWarpInEndY;
    return kWarpInStartY + (tick - kWarpInStartTick) * kWarpInStepY;
}

inline Frame evaluate(int tick, const Params& params) {
    Frame frame;
    if (tick < 0) return frame;

    if (tick >= kReturnTick) {
        frame.phase = (tick < kReturnTick + 240) ? Phase::Return : Phase::Inactive;
        frame.screenBlank = frame.phase == Phase::Return;
        return frame;
    }

    if (tick < kWarpOutStartTick) {
        frame.phase = Phase::VictoryPose;
        frame.playerY = kWarpOutStartY;
    } else if (tick <= kWarpOutEndTick) {
        frame.phase = Phase::WarpOut;
        frame.playerY = warpOutY(tick);
    } else if (tick < kWarpInStartTick) {
        frame.phase = Phase::SpecScreen;
        frame.screenBlank = tick >= kSpecScreenTick;
    } else if (tick <= kWarpInEndTick) {
        frame.phase = Phase::WarpIn;
        frame.playerY = warpInY(tick);
    } else {
        frame.phase = Phase::Demo;
        frame.playerY = kWarpInEndY;
    }

    if (params.textBlipCount > 0 && tick >= kTextStartTick) {
        const int since = tick - kTextStartTick;
        if (since % kTextStepTicks == 0
            && since / kTextStepTicks < params.textBlipCount) {
            frame.emitTextBlip = true;
        }
    }

    if (params.demoShotCount > 0 && tick >= kDemoFirstTick
        && params.demoShotStepTicks > 0) {
        const int since = tick - kDemoFirstTick;
        if (since % params.demoShotStepTicks == 0
            && since / params.demoShotStepTicks < params.demoShotCount) {
            frame.emitDemoShot = true;
        }
    }

    return frame;
}

}  // namespace mmx::weapon_get_timeline
