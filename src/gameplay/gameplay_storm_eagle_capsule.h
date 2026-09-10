// gameplay_storm_eagle_capsule.h - integrates the Storm Eagle helmet capsule.
// Boundary: source-timed movement/grant flow only; visual playback is separate.

#pragma once

#include "gameplay/storm_eagle_capsule_cutscene.h"
#include "entities/pickup.h"
#include "entities/player.h"
#include "systems/audio.h"
#include "raylib.h"

#include <string>

namespace mmx::gameplay_storm_eagle_capsule {

struct TraceState {
    int sourceFrame = 0;
    int animByte = -1;
    bool active = false;
    bool grantEvent = false;
    bool releaseEvent = false;
    bool pickupCollected = false;
    std::string pickupId;

    void reset() {
        sourceFrame = 0;
        pickupId.clear();
        clearTransient();
    }

    void clearTransient() {
        animByte = -1;
        active = false;
        grantEvent = false;
        releaseEvent = false;
        pickupCollected = false;
    }
};

struct State {
    StormEagleCapsuleCutscene cutscene;
    bool helmetGrantApplied = false;
    bool sourceVisualActive = false;
    int sourceVisualFrame = 1;
    TraceState trace;

    bool active() const { return cutscene.active(); }
    bool controlsLocked() const { return cutscene.controlsLocked(); }
    bool visualActive() const { return sourceVisualActive; }

    void reset() {
        cutscene.reset();
        helmetGrantApplied = false;
        sourceVisualActive = false;
        sourceVisualFrame = 1;
        trace.reset();
    }
};

struct UpdateResult {
    bool resetAfterTrace = false;
};

struct TraceSnapshot {
    int sourceFrame = 0;
    int animByte = -1;
    bool active = false;
    bool grantEvent = false;
    bool releaseEvent = false;
    bool armorHelmet = false;
    bool pickupCollected = false;
    const std::string* pickupId = nullptr;
};

inline void placePlayer(Player& player, StormEagleCapsulePoint point) {
    const float dx = static_cast<float>(point.x) - player.position.x;
    const float dy = static_cast<float>(point.y) - player.position.y;
    player.prevPosition = player.position;
    player.position = {
        static_cast<float>(point.x),
        static_cast<float>(point.y),
    };
    player.velocity = {dx, dy};
    player.onGround = dy == 0.0f;
    if (dx > 0.0f) {
        player.facingRight = true;
    } else if (dx < 0.0f) {
        player.facingRight = false;
    }
    if (dx != 0.0f) {
        player.changeState(PlayerState::Run);
    } else if (dy < 0.0f) {
        player.changeState(PlayerState::Jump);
    } else if (dy > 0.0f) {
        player.changeState(PlayerState::Fall);
    } else {
        player.changeState(PlayerState::Idle);
    }
}

inline void start(State& state, Pickup& pickup, Player& player) {
    state.cutscene.start(pickup.persistentId);
    state.helmetGrantApplied = false;
    state.sourceVisualActive = true;
    state.sourceVisualFrame = 1;
    state.trace.reset();
    state.trace.pickupId = pickup.persistentId;
    pickup.active = false;

    if (auto anchor = state.cutscene.forcedPlayerAnchor()) {
        placePlayer(player, *anchor);
        player.prevPosition = player.position;
        player.velocity = {0.0f, 0.0f};
    }

    TraceLog(LOG_INFO,
             "Storm Eagle capsule: source-timed helmet cutscene started for pickup '%s'",
             pickup.persistentId.c_str());
}

inline UpdateResult update(State& state, Player& player) {
    UpdateResult result;
    const std::string pickupId = state.cutscene.persistentPickupId();
    const auto events = state.cutscene.tick();
    result.resetAfterTrace = events.releaseControls;

    state.trace.sourceFrame = state.cutscene.frame();
    state.sourceVisualActive = true;
    state.sourceVisualFrame = state.trace.sourceFrame;
    state.trace.animByte = StormEagleCapsuleCutscene::playerAnimByteForFrame(
        state.trace.sourceFrame).value_or(-1);
    state.trace.active = true;
    state.trace.grantEvent = events.grantHelmet;
    state.trace.releaseEvent = events.releaseControls;

    if (auto anchor = StormEagleCapsuleCutscene::playerAnchorForFrame(
            state.trace.sourceFrame)) {
        placePlayer(player, *anchor);
    }

    if (events.grantHelmet && !state.helmetGrantApplied) {
        player.grantArmorHelmet();
        state.helmetGrantApplied = true;
        AudioManager::playSFX(SFX::HeartTank);
        TraceLog(LOG_INFO,
                 "Storm Eagle capsule: helmet granted at source-local frame %d",
                 StormEagleCapsuleCutscene::kHelmetGrantFrame);
    }

    if (events.releaseControls) {
        if (!state.helmetGrantApplied) {
            player.grantArmorHelmet();
            state.helmetGrantApplied = true;
        }
        if (!pickupId.empty()) {
            player.markPickupCollectedInProgress(pickupId);
            state.trace.pickupCollected = true;
        }
        TraceLog(LOG_INFO,
                 "Storm Eagle capsule: control released at source-local frame %d",
                 StormEagleCapsuleCutscene::kFirstPostGrantMotionFrame);
    }

    return result;
}

inline void finishTrace(State& state, const UpdateResult& result) {
    state.trace.clearTransient();
    if (result.resetAfterTrace) {
        state.cutscene.reset();
        state.helmetGrantApplied = false;
        state.trace.pickupId.clear();
    }
}

inline void finishReleasedVisual(State& state) {
    if (!state.cutscene.active() && state.sourceVisualActive) {
        state.sourceVisualActive = false;
    }
}

inline const StormEagleCapsuleVisualFrame& visualState(const State& state) {
    return StormEagleCapsuleCutscene::visualStateForFrame(state.sourceVisualFrame);
}

inline TraceSnapshot traceState(const State& state, bool armorHelmet) {
    return {
        state.trace.sourceFrame,
        state.trace.animByte,
        state.trace.active,
        state.trace.grantEvent,
        state.trace.releaseEvent,
        armorHelmet,
        state.trace.pickupCollected,
        &state.trace.pickupId,
    };
}

} // namespace mmx::gameplay_storm_eagle_capsule
