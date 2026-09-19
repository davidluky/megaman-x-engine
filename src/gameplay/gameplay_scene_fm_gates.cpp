// gameplay_scene_fm_gates.cpp - original gate-owned Player/camera order.
#include "gameplay/gameplay_scene_fm_gates.h"
#include "systems/tilemap.h"

namespace mmx::flame_mammoth_gates {
namespace {
struct Token { std::uint8_t duration, control, art; };
// ROM AF:B020 and AF:B055. Terminal record durations are retained, but the
// gate changes phase as soon as control bit 80 loads (52 and 84 elapsed ticks).
constexpr Token openTokens[] = {
    {4,0,0},{2,0,4},{4,0,5},{2,0,4},{4,0,1},{4,0,2},{4,0,3},{4,0,0},
    {2,0,1},{4,0,2},{2,0,3},{2,0,4},{4,0,5},{2,1,4},{4,0,6},{4,0,7},{16,128,8}
};
constexpr Token closeTokens[] = {
    {16,0,8},{4,0,7},{4,0,6},{2,0,4},{4,0,5},{2,0,4},{16,0,0},{4,0,1},
    {4,0,2},{4,0,3},{4,0,0},{4,0,1},{4,0,2},{4,0,3},{2,0,4},{4,0,5},
    {2,0,4},{4,128,0}
};
AnimationState loadToken(AnimationState state) {
    const auto& token = state.animation ? closeTokens[state.record] : openTokens[state.record];
    state.remaining = token.duration;
    state.control = token.control;
    state.art = token.art;
    state.pointer = static_cast<std::uint16_t>((state.animation ? 0xB055 : 0xB020) + 3 * state.record);
    return state;
}
} // namespace

AnimationState beginAnimation(bool closing) noexcept {
    AnimationState state;
    state.animation = closing ? 1 : 0;
    return loadToken(state);
}
AnimationState advanceAnimation(AnimationState state) noexcept {
    if (state.complete()) return state;
    if (--state.remaining == 0) {
        ++state.record;
        state = loadToken(state);
    }
    return state;
}

WalkStep advanceWalk(const WalkState& before) noexcept {
    WalkStep result{before, before.camera.target == before.camera.lock};
    // ROM 81E7A9 installs 0074; 81E7BE moves Player before testing the camera
    // from the previous tick. Camera 80DDDB runs afterwards. No frame timer.
    result.state.xFixed = (before.xFixed + 0x74u) & 0xFFFFFFu;
    result.state.camera = cameraStep(before.camera, {
        static_cast<std::uint16_t>(result.state.xFixed >> 8),
        static_cast<std::uint16_t>(before.xFixed >> 8)});
    return result;
}

bool setGateBlocks(Tilemap& map, std::uint8_t subId, bool open, bool frozen) {
    if ((subId != 4 && subId != 5) || map.width() != 512 ||
        map.height() != 64 || map.tileSize() != 16) return false;
    // Source helper column order, original gate coordinates (7416/7672,647).
    // The gate actor later shifts by 16; the map still uses these original cells.
    constexpr int open4[] = {4,9,9,4,9,9};
    constexpr int open5[] = {4,9,9,0x28F,0x299,0x2A4};
    const auto* opened = subId == 4 ? open4 : open5;
    const int column = subId == 4 ? 463 : 479;
    std::vector<DecodedMainBlockReplacement> batch;
    batch.reserve(6);
    for (int col = 0; col < 2; ++col) {
        for (int row = 0; row < 3; ++row) {
            const int index = col * 3 + row;
            const int baseline = subId == 5 && !frozen ? 0 : 0x159 + row;
            batch.push_back({column + col, 39 + row,
                open ? baseline : opened[index],
                open ? opened[index] : 0x159 + row,
                open && baseline == 0 ? std::optional<TileType>{TileType::Solid}
                                      : std::nullopt});
        }
    }
    return map.replaceDecodedMainBlocks(batch);
}
} // namespace mmx::flame_mammoth_gates
