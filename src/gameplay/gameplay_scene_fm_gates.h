// gameplay_scene_fm_gates.h - measured Flame Mammoth gate camera and scene lane.
// Source: knowledge_base/mmx1/stages/flame-mammoth/gate_passage/walk_camera.json.
#pragma once

#include <array>
#include <cstdint>
#include "gameplay/flame_mammoth_gate_close.h"

namespace mmx { class Tilemap; }

namespace mmx::flame_mammoth_gates {
struct PlayerSample {
    std::uint16_t xPos = 0;
    std::uint16_t xOld = 0;
};
struct CameraState {
    std::uint8_t frac = 0;             // $1E4C, carried untouched
    std::uint16_t target = 0;          // $1E4D/$1E4E
    std::uint16_t saved = 0;           // $1E6A
    std::uint16_t left = 0;            // $1E56
    std::uint16_t right = 0;           // $1E58
    std::uint16_t lock = 0;            // $1E60
    std::uint16_t levelSpecFlag = 0;   // $1E5E
    std::uint16_t borderStep = 0;      // $1E52
    std::uint16_t speedMax = 0;        // $1E54
    std::uint16_t centerLeft = 0;      // $1E74
    std::uint16_t centerRight = 0;     // $1E76
};

inline constexpr std::uint16_t add16(std::int32_t value) noexcept {
    return static_cast<std::uint16_t>(static_cast<std::uint32_t>(value) & 0xFFFFu);
}

inline constexpr std::uint16_t sub16(std::uint16_t left,
                                     std::uint16_t right) noexcept {
    return add16(static_cast<std::int32_t>(left) - right);
}

inline constexpr std::int32_t signed16(std::uint16_t value) noexcept {
    const std::uint16_t masked = value;
    return (masked & 0x8000u) ? static_cast<std::int32_t>(masked) - 0x10000
                              : static_cast<std::int32_t>(masked);
}

inline constexpr bool negative16(std::uint16_t value) noexcept {
    return signed16(value) < 0;
}

inline constexpr bool nonnegative16(std::uint16_t value) noexcept {
    return !negative16(value);
}

inline constexpr std::uint16_t followTarget(const CameraState& state,
                                            const PlayerSample& player) noexcept {
    const std::uint16_t leftDelta =
        sub16(sub16(player.xPos, state.target), state.centerLeft);
    if (!negative16(leftDelta)) {
        return add16(static_cast<std::int32_t>(state.target) + signed16(leftDelta));
    }

    const std::uint16_t rightDelta =
        sub16(sub16(player.xPos, state.target), state.centerRight);
    if (rightDelta == 0 || nonnegative16(rightDelta)) {
        return state.target;
    }
    return add16(static_cast<std::int32_t>(state.target) + signed16(rightDelta));
}

inline constexpr std::uint16_t slewToSaved(std::uint16_t desired,
                                            std::uint16_t saved,
                                            std::uint16_t speedMax) noexcept {
    const std::int32_t delta = signed16(sub16(desired, saved));
    if (delta >= 0) {
        const std::int32_t limited = delta < speedMax ? delta : speedMax;
        return add16(static_cast<std::int32_t>(saved) + limited);
    }
    const std::int32_t limited = delta > -static_cast<std::int32_t>(speedMax)
        ? delta : -static_cast<std::int32_t>(speedMax);
    return add16(static_cast<std::int32_t>(saved) + limited);
}

inline constexpr std::uint16_t clampToPreviousBorders(
    const CameraState& state, std::uint16_t target) noexcept {
    if (negative16(sub16(state.right, target))) {
        target = state.right;
    }
    if (nonnegative16(sub16(state.left, target))) {
        target = state.left;
    }
    return target;
}

inline constexpr std::array<std::uint16_t, 2> dynamicBorders(
    const CameraState& state,
    std::uint16_t target,
    const PlayerSample& player) noexcept {
    std::uint16_t left = state.left;
    std::uint16_t right = state.right;
    const std::int32_t step = state.borderStep;

    if (state.levelSpecFlag != left) {
        if (negative16(sub16(state.levelSpecFlag, target))) {
            const std::uint16_t candidate = add16(
                static_cast<std::int32_t>(sub16(
                    sub16(player.xPos, target), state.centerLeft)) + step);
            if (candidate < 4) {
                left = state.levelSpecFlag;
            } else {
                std::uint16_t delta = sub16(player.xPos, player.xOld);
                if (!negative16(delta)) {
                    delta = 0;
                }
                const std::uint16_t candidate2 = add16(
                    static_cast<std::int32_t>(target) - step + signed16(delta));
                left = nonnegative16(sub16(candidate2, state.levelSpecFlag))
                    ? candidate2 : state.levelSpecFlag;
            }
        } else {
            const std::uint16_t candidate = add16(static_cast<std::int32_t>(target) + step);
            left = negative16(sub16(candidate, state.levelSpecFlag))
                ? candidate : state.levelSpecFlag;
        }
    }

    if (state.lock != right) {
        if (nonnegative16(sub16(state.lock, target))) {
            const std::uint16_t candidate = add16(
                static_cast<std::int32_t>(sub16(
                    sub16(player.xPos, target), state.centerRight)) + step);
            if (candidate < 4) {
                right = state.lock;
            } else {
                std::uint16_t delta = sub16(player.xPos, player.xOld);
                if (negative16(delta)) {
                    delta = 0;
                }
                const std::uint16_t candidate2 = add16(
                    static_cast<std::int32_t>(target) + signed16(delta) + step);
                right = negative16(sub16(candidate2, state.lock))
                    ? candidate2 : state.lock;
            }
        } else {
            const std::uint16_t candidate = add16(static_cast<std::int32_t>(target) - step);
            right = nonnegative16(sub16(candidate, state.lock))
                ? candidate : state.lock;
        }
    }
    return {left, right};
}

// 80DE40/80DE9D/80DE7E/E0DE/E1B5 in source order.  This function has no
// frame count and no render/PPU lag input.  The gate exit test is intentionally
// outside this function because the actor compares the old target first.
inline constexpr CameraState cameraStep(const CameraState& state,
                                        const PlayerSample& player) noexcept {
    const std::uint16_t saved = state.target;
    std::uint16_t target = followTarget(state, player);
    target = slewToSaved(target, saved, state.speedMax);
    target = clampToPreviousBorders(state, target);
    const auto borders = dynamicBorders(state, target, player);
    CameraState result = state;
    result.saved = saved;
    result.target = target;
    result.left = borders[0];
    result.right = borders[1];
    return result;
}


struct WalkState {
    CameraState camera;
    std::uint32_t xFixed = 0;
    std::uint32_t yFixed = 0;
};
struct WalkStep {
    WalkState state;
    bool beginClose = false;
};
WalkStep advanceWalk(const WalkState& before) noexcept;

struct AnimationState {
    std::uint8_t animation = 0;
    std::uint8_t record = 0;
    std::uint8_t remaining = 0;
    std::uint8_t art = 0;
    std::uint8_t control = 0;
    std::uint16_t pointer = 0;
    bool complete() const { return (control & 0x80u) != 0; }
};
AnimationState beginAnimation(bool closing) noexcept;
AnimationState advanceAnimation(AnimationState state) noexcept;

// 81E8DB/84A0E2: the six decoded main blocks commit as one map operation.
// The second gate has a baked solid baseline with block zero in normal mode.
bool setGateBlocks(Tilemap& map, std::uint8_t subId, bool open, bool frozen);

enum class Phase : std::uint8_t { Waiting, Open, Walk, Close, Released };
struct Lane {
    Phase phase = Phase::Waiting;
    AnimationState animation;
    WalkState walk;
    flame_mammoth_gate_close::State close;
    std::uint16_t gateX = 7416;
    // walk_camera.json route 0 (frame 1065) is the first gate: contact 7397,
    // approach camera 7168, lock/levelSpec 7424.  Route 1 (frame 1458) is the
    // second: contact 7653, approach camera 7424, lock/levelSpec 7680, and
    // second_close.json resumes from its last row (frame 1576).
    std::uint16_t secondGateX = 7672;
    std::uint8_t subId = 4;               // gate record 1658, then 1628
    std::uint16_t approachCameraX = 7168;
    std::uint16_t cameraLockX = 7424;
    // second_release_owners.json: at the release the gate record clears the
    // Player action and the Mammoth event takes control in the SAME frame.
    bool handoverRequested = false;
    // setGateBlocks' baked-baseline selector: record 1658's six cells are
    // 0x159..0x15B in both maps, record 1628's are block 0 with baked
    // Solid collision in the normal map and 0x159..0x15B in the frozen
    // one.  Read from the map that is loaded, never assumed.
    bool frozenBaseline = true;
    std::uint16_t originX() const { return subId == 4 ? gateX : secondGateX; }
    std::uint16_t& activeX() { return subId == 4 ? gateX : secondGateX; }
    float savedVelocityX = 0;
    bool cameraActive = false;
    int cameraY = 543;
    int bottom = 543;
    int selectedBottom = 543;
    bool locked() const {
        return phase == Phase::Open || phase == Phase::Walk || phase == Phase::Close;
    }
};

} // namespace mmx::flame_mammoth_gates
