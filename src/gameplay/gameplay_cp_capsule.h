// gameplay_cp_capsule.h - integrates CP capsule placement and interaction flow.
// Boundary: uses source-timed cutscene data; does not author new stage truth.

#pragma once

#include "gameplay/cp_capsule_cutscene.h"
#include "gameplay/gameplay_trace_types.h"
#include "entities/pickup.h"
#include "entities/player.h"
#include "systems/audio.h"
#include "raylib.h"

#include <string>

namespace mmx::gameplay_cp_capsule {

struct TraceState {
    int sourceFrame = 0;
    int animByte = -1;
    bool active = false;
    bool grantEvent = false;
    bool releaseEvent = false;

    void reset() {
        sourceFrame = 0;
        clearTransient();
    }

    void clearTransient() {
        animByte = -1;
        active = false;
        grantEvent = false;
        releaseEvent = false;
    }
};

struct State {
    CpCapsuleCutscene cutscene;
    bool bootsGrantApplied = false;
    TraceState trace;

    bool active() const { return cutscene.active(); }
    bool controlsLocked() const { return cutscene.controlsLocked(); }

    void reset() {
        cutscene.reset();
        bootsGrantApplied = false;
        trace.reset();
    }

    const CpCapsuleSourcePlane& sourcePlane() const { return cutscene.sourcePlane(); }
    const CpCapsuleSourceObjComposite& sourceObjComposite() const {
        return cutscene.sourceObjComposite();
    }
};

struct UpdateResult {
    bool resetAfterTrace = false;
};

inline void placePlayer(Player& player, CpCapsulePoint point, int sourceFrame) {
    const float dx = static_cast<float>(point.x) - player.position.x;
    const float dy = static_cast<float>(point.y) - player.position.y;
    player.prevPosition = player.position;
    player.position = {
        static_cast<float>(point.x),
        static_cast<float>(point.y)
    };

    const bool scriptedMotion =
        sourceFrame >= CpCapsuleCutscene::kFirstPlayerMotionFrame &&
        sourceFrame < CpCapsuleCutscene::kPedestalLockStartFrame;
    if (scriptedMotion) {
        player.velocity = {dx, dy};
        if (dx > 0.0f) {
            player.facingRight = true;
        } else if (dx < 0.0f) {
            player.facingRight = false;
        }
        player.onGround = dy == 0.0f;
        if (dx != 0.0f) {
            player.changeState(PlayerState::Run);
        } else if (dy < 0.0f) {
            player.changeState(PlayerState::Jump);
        } else if (dy > 0.0f) {
            player.changeState(PlayerState::Fall);
        } else {
            player.changeState(PlayerState::Idle);
        }
    } else {
        player.velocity = {0.0f, 0.0f};
        player.onGround = true;
        player.changeState(PlayerState::Idle);
    }
}

inline void start(State& state, Pickup& pickup, Player& player) {
    state.cutscene.start(pickup.persistentId);
    state.bootsGrantApplied = false;
    state.trace.reset();
    pickup.active = false;

    player.prevPosition = player.position;
    if (auto anchor = state.cutscene.forcedPlayerAnchor()) {
        player.position = {
            static_cast<float>(anchor->x),
            static_cast<float>(anchor->y)
        };
        player.prevPosition = player.position;
    }
    player.velocity = {0.0f, 0.0f};
    player.onGround = true;
    player.changeState(PlayerState::Idle);

    TraceLog(LOG_INFO,
             "CP capsule: source-timed boots cutscene started for pickup '%s'",
             pickup.persistentId.c_str());
}

inline UpdateResult update(State& state, Player& player) {
    UpdateResult result;
    const std::string pickupId = state.cutscene.persistentPickupId();
    const auto events = state.cutscene.tick();
    result.resetAfterTrace = events.releaseControls;

    state.trace.sourceFrame = state.cutscene.frame();
    state.trace.animByte =
        CpCapsuleCutscene::playerAnimByteForFrame(state.trace.sourceFrame).value_or(-1);
    state.trace.active = true;
    state.trace.grantEvent = events.grantBoots;
    state.trace.releaseEvent = events.releaseControls;

    if (auto anchor = state.cutscene.forcedPlayerAnchor()) {
        placePlayer(player, *anchor, state.cutscene.frame());
    } else if (events.releaseControls) {
        placePlayer(player,
                    CpCapsuleCutscene::kPostGrantMotionAnchor,
                    CpCapsuleCutscene::kFirstPostGrantMotionFrame);
    } else {
        player.prevPosition = player.position;
        player.velocity = {0.0f, 0.0f};
    }

    if (events.grantBoots && !state.bootsGrantApplied) {
        player.grantArmorBoots();
        state.bootsGrantApplied = true;
        AudioManager::playSFX(SFX::HeartTank);
        TraceLog(LOG_INFO,
                 "CP capsule: boots granted at source-local frame %d",
                 CpCapsuleCutscene::kBootsGrantFrame);
    }

    if (events.releaseControls) {
        if (!state.bootsGrantApplied) {
            player.grantArmorBoots();
        }
        if (!pickupId.empty()) {
            player.markPickupCollectedInProgress(pickupId);
        }
        TraceLog(LOG_INFO, "CP capsule: control released at source-local frame %d",
                 CpCapsuleCutscene::kFirstPostGrantMotionFrame);
    }

    return result;
}

inline void finishTrace(State& state, const UpdateResult& result) {
    state.trace.clearTransient();
    if (result.resetAfterTrace) {
        state.cutscene.reset();
        state.bootsGrantApplied = false;
    }
}

inline bool hidesPlayer(const State& state) {
    return state.active();
}

inline gameplay_trace::CpCapsuleTraceState traceState(const State& state, bool armorBoots) {
    return {
        state.trace.sourceFrame,
        state.trace.animByte,
        state.trace.active,
        state.trace.grantEvent,
        state.trace.releaseEvent,
        armorBoots,
    };
}

} // namespace mmx::gameplay_cp_capsule
