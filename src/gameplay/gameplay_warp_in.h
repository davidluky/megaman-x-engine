// gameplay_warp_in.h - manages player warp-in timing and presentation state.
// Boundary: entrance presentation only; spawn selection stays in stage setup.

#pragma once

namespace mmx {
class TextureResource;
}

namespace mmx::gameplay_warp_in {

inline constexpr int kDuration = 30;

struct State {
    int timer = 0;
    int duration = kDuration;
    const TextureResource* capsuleTexture = nullptr;
    const TextureResource* burstTexture = nullptr;
};

inline void setActive(State& state, bool active) {
    state.timer = active ? state.duration : 0;
}

inline bool active(const State& state) {
    return state.timer > 0;
}

inline void tick(State& state) {
    if (state.timer > 0) state.timer--;
}

inline bool hidesPlayer(const State& state) {
    return state.timer > state.duration * 15 / 100;
}

} // namespace mmx::gameplay_warp_in
