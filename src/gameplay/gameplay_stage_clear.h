// gameplay_stage_clear.h - manages stage-clear timing, rewards, and handoff.
// Boundary: clear presentation only; boss defeat and save rules remain separate.

#pragma once

#include "gameplay/weapon_get_timeline.h"
#include "systems/weapon.h"

#include <optional>

namespace mmx::gameplay_stage_clear {

inline constexpr int kBestTimeReadyTimer = 20;
inline constexpr int kRewardReadyTimer = 40;
inline constexpr int kInputReadyTimer = 70;

struct State {
    bool active = false;
    int timer = 0;
    bool frozen = false;
    std::optional<Weapon> awardedWeapon;

    // GC2.1: the source-measured weapon-get choreography, advanced on the same
    // clock as `timer`. Presentation does not consume it yet - the existing
    // overlay thresholds above are untouched - so this slice adds the state
    // machine without changing what is drawn.
    weapon_get_timeline::Params sequenceParams;
    weapon_get_timeline::Frame sequence;
};

inline void reset(State& state) {
    state.active = false;
    state.timer = 0;
    state.frozen = false;
    state.awardedWeapon.reset();
    state.sequence = weapon_get_timeline::Frame{};
}

inline void begin(State& state) {
    state.active = true;
    state.timer = 0;
    state.sequence = weapon_get_timeline::evaluate(0, state.sequenceParams);
}

inline bool advance(State& state) {
    if (!state.active) return false;
    state.timer++;
    state.sequence = weapon_get_timeline::evaluate(state.timer,
                                                   state.sequenceParams);
    return true;
}

// The phase the source says should be on screen right now. Presentation will
// read this in the follow-up slice.
inline weapon_get_timeline::Phase sequencePhase(const State& state) {
    return state.active ? state.sequence.phase
                        : weapon_get_timeline::Phase::Inactive;
}

inline bool bestTimeReady(const State& state) {
    return state.active && state.timer > kBestTimeReadyTimer;
}

inline bool rewardReady(const State& state) {
    return state.active && state.timer > kRewardReadyTimer;
}

inline bool inputReady(const State& state) {
    return state.active && state.timer > kInputReadyTimer;
}

// The source weapon-get cutscene has no Confirm or Cancel skip affordance.
inline constexpr bool navigationInputAllowed(bool weaponGetSequenceActive) {
    return !weaponGetSequenceActive;
}

} // namespace mmx::gameplay_stage_clear
