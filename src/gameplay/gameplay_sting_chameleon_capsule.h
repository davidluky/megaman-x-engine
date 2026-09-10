// gameplay_sting_chameleon_capsule.h - Sting body capsule runtime integration.
// Boundary: source adapter playback plus lifecycle/grant/persistence.

#pragma once

#include "gameplay/sting_chameleon_capsule_cutscene.h"
#include "entities/pickup.h"
#include "entities/player.h"
#include "systems/audio.h"
#include "raylib.h"

#include <cstdio>
#include <string>

namespace mmx::gameplay_sting_chameleon_capsule {

struct TraceState {
    int localFrame = 0;
    int sourceFrame = 0;
    bool active = false;
    bool grantEvent = false;
    bool releaseEvent = false;
    bool textBlipEvent = false;
    int textBlipCommand = -1;
    bool pickupCollected = false;
    std::string pickupId;

    void reset() {
        localFrame = 0;
        sourceFrame = 0;
        pickupId.clear();
        clearTransient();
    }

    void clearTransient() {
        active = false;
        grantEvent = false;
        releaseEvent = false;
        textBlipEvent = false;
        textBlipCommand = -1;
        pickupCollected = false;
    }
};

struct State {
    StingChameleonCapsuleCutscene cutscene;
    bool bodyGrantApplied = false;
    bool sourceVisualActive = false;
    int sourceVisualFrame = 0;
    int textBlipPlaybackCount = 0;
    bool postReleaseMotionActive = false;
    int postReleaseSourceFrame = 0;
    TraceState trace;

    bool active() const { return cutscene.active(); }
    bool controlsLocked() const { return cutscene.controlsLocked(); }
    bool visualActive() const { return sourceVisualActive; }
    // The source camera stays fixed on the arena window through the cutscene,
    // the released visual, and the proven post-release tail.
    bool cameraWindowActive() const {
        return cutscene.active() || sourceVisualActive || postReleaseMotionActive;
    }

    void reset() {
        cutscene.reset();
        bodyGrantApplied = false;
        sourceVisualActive = false;
        sourceVisualFrame = 0;
        textBlipPlaybackCount = 0;
        postReleaseMotionActive = false;
        postReleaseSourceFrame = 0;
        trace.reset();
    }
};

struct UpdateResult {
    bool resetAfterTrace = false;
};

struct TraceSnapshot {
    int localFrame = 0;
    int sourceFrame = 0;
    bool active = false;
    bool grantEvent = false;
    bool releaseEvent = false;
    bool textBlipEvent = false;
    int textBlipCommand = -1;
    bool armorBody = false;
    bool pickupCollected = false;
    const std::string* pickupId = nullptr;
    bool postReleaseMotionActive = false;
    int postReleaseSourceFrame = 0;
    float semanticX = 0.0f;
    float semanticY = 0.0f;
};

inline void placePlayerAtProvenAnchor(Player& player, float x, float y) {
    player.prevPosition = player.position;
    player.position = {x, y};
    player.velocity = {0.0f, 0.0f};
    player.onGround = true;
    // changeState alone no-ops when X is already Idle, leaving the walk
    // animation he arrived with frozen mid-stride; the source shows the
    // plain standing pose for the whole scene.
    player.forceIdlePose();
}

inline void start(State& state, Pickup& pickup, Player& player) {
    state.cutscene.start(pickup.persistentId);
    state.bodyGrantApplied = false;
    state.sourceVisualActive = true;
    state.sourceVisualFrame = 0;
    state.textBlipPlaybackCount = 0;
    state.postReleaseMotionActive = false;
    state.postReleaseSourceFrame = 0;
    state.trace.reset();
    state.trace.pickupId = pickup.persistentId;
    pickup.active = false;

    placePlayerAtProvenAnchor(
        player,
        static_cast<float>(StingChameleonCapsuleCutscene::kInteractionPlayerX),
        static_cast<float>(StingChameleonCapsuleCutscene::kInteractionPlayerY));
    player.prevPosition = player.position;

    TraceLog(LOG_INFO,
             "Sting Chameleon capsule: source-timed body cutscene started for pickup '%s'",
             pickup.persistentId.c_str());
}

inline UpdateResult update(State& state, Player& player) {
    UpdateResult result;
    const std::string pickupId = state.cutscene.persistentPickupId();
    const auto events = state.cutscene.tick();
    result.resetAfterTrace = events.releaseControls;

    state.trace.localFrame = state.cutscene.frame();
    state.sourceVisualActive = true;
    state.sourceVisualFrame = state.trace.localFrame;
    state.trace.sourceFrame =
        StingChameleonCapsuleCutscene::sourceRouteFrameForLocalFrame(
            state.trace.localFrame);
    state.trace.active = true;
    state.trace.grantEvent = events.grantBody;
    state.trace.releaseEvent = events.releaseControls;
    state.trace.textBlipEvent =
        StingChameleonCapsuleCutscene::textBlipEventForFrame(
            state.trace.localFrame);
    if (state.trace.textBlipEvent) {
        state.trace.textBlipCommand =
            StingChameleonCapsuleCutscene::textBlipCommand();
        AudioManager::playApu(state.trace.textBlipCommand);
        state.textBlipPlaybackCount++;
    }

    if (state.trace.localFrame <
        StingChameleonCapsuleCutscene::kPedestalLockFrame) {
        placePlayerAtProvenAnchor(
            player,
            static_cast<float>(StingChameleonCapsuleCutscene::kInteractionPlayerX),
            static_cast<float>(StingChameleonCapsuleCutscene::kInteractionPlayerY));
    } else {
        placePlayerAtProvenAnchor(
            player,
            static_cast<float>(StingChameleonCapsuleCutscene::kPedestalX),
            static_cast<float>(StingChameleonCapsuleCutscene::kPedestalY));
    }

    if (events.grantBody && !state.bodyGrantApplied) {
        player.grantArmorBody();
        state.bodyGrantApplied = true;
        TraceLog(LOG_INFO,
                 "Sting Chameleon capsule: body granted at source-local frame %d",
                 StingChameleonCapsuleCutscene::kBodyGrantFrame);
    }

    if (events.releaseControls) {
        if (!state.bodyGrantApplied) {
            player.grantArmorBody();
            state.bodyGrantApplied = true;
        }
        if (!pickupId.empty()) {
            player.markPickupCollectedInProgress(pickupId);
            state.trace.pickupCollected = true;
        }
        state.postReleaseMotionActive = true;
        state.postReleaseSourceFrame =
            StingChameleonCapsuleCutscene::kControlReleaseSourceFrame;
        player.position = {
            StingChameleonCapsuleCutscene::postReleaseEngineX(
                state.postReleaseSourceFrame),
            StingChameleonCapsuleCutscene::postReleaseEngineY(),
        };
        player.prevPosition = player.position;
        player.velocity = {0.0f, 0.0f};
        player.onGround = true;
        player.wasOnGround = true;
        player.onCeiling = false;
        player.touchingWallLeft = false;
        player.touchingWallRight = false;
        player.changeState(PlayerState::Idle);
        TraceLog(LOG_INFO,
                 "Sting Chameleon capsule: control released at source-local frame %d",
                 StingChameleonCapsuleCutscene::kControlReleaseFrame);
    }

    return result;
}

inline void preparePostReleaseMotion(
    State& state,
    Player& player,
    bool rightHeld,
    bool leftHeld) {
    if (!state.postReleaseMotionActive) return;
    if (state.postReleaseSourceFrame >=
        StingChameleonCapsuleCutscene::kLastProvenPostReleaseSourceFrame) {
        state.postReleaseMotionActive = false;
        return;
    }
    // The oracle route holds Right. Any different live input exits this narrow
    // source replay immediately and leaves Player's ordinary update untouched.
    if (!rightHeld || leftHeld) {
        state.postReleaseMotionActive = false;
        return;
    }
    ++state.postReleaseSourceFrame;
    player.velocity.x = static_cast<float>(
        StingChameleonCapsuleCutscene::postReleaseDeltaXRaw(
            state.postReleaseSourceFrame)) / 256.0f;
}

inline void finishPostReleaseMotion(State& state, Player& player) {
    if (!state.postReleaseMotionActive) return;
    player.position.x = StingChameleonCapsuleCutscene::postReleaseEngineX(
        state.postReleaseSourceFrame);
    player.position.y = StingChameleonCapsuleCutscene::postReleaseEngineY();
    if (state.postReleaseSourceFrame >= 1743) {
        player.velocity.x = 0.0f;
        player.touchingWallRight = true;
    }
}

inline void finishTrace(State& state, const UpdateResult& result) {
    state.trace.clearTransient();
    if (result.resetAfterTrace) {
        state.cutscene.reset();
        state.bodyGrantApplied = false;
        state.trace.pickupId.clear();
    }
}

inline void finishReleasedVisual(State& state) {
    if (!state.cutscene.active() && state.sourceVisualActive) {
        state.sourceVisualActive = false;
    }
}

inline const StingChameleonCapsuleVisualFrame& visualState(const State& state) {
    return StingChameleonCapsuleCutscene::visualStateForFrame(
        state.sourceVisualFrame);
}

inline bool hidesOrdinaryPlayer(const State& state) {
    return state.visualActive() && visualState(state).character.visible();
}

template <typename DrawRegion>
inline void renderPresentation(const State& state, DrawRegion drawRegion) {
    if (!state.visualActive()) return;
    const auto& visual = visualState(state);

    // T25C2's source composition order. Optional regions remain transparent.
    drawRegion(
        StingChameleonCapsuleCutscene::bg3AtlasPath(visual.bg3.atlasPage),
        visual.bg3);
    drawRegion(
        StingChameleonCapsuleCutscene::objectAtlasPath(visual.object.atlasPage),
        visual.object);
    drawRegion(
        StingChameleonCapsuleCutscene::enhancementAtlasPath(
            visual.enhancement.atlasPage),
        visual.enhancement);
    drawRegion(
        StingChameleonCapsuleCutscene::characterAtlasPath(
            visual.character.atlasPage),
        visual.character);
    drawRegion(
        StingChameleonCapsuleCutscene::portraitAtlasPath(
            visual.portrait.atlasPage),
        visual.portrait);
}

inline TraceSnapshot traceState(const State& state, bool armorBody) {
    const float semanticX = state.trace.localFrame <
        StingChameleonCapsuleCutscene::kPedestalLockFrame
        ? static_cast<float>(StingChameleonCapsuleCutscene::kInteractionPlayerX)
        : static_cast<float>(StingChameleonCapsuleCutscene::kPedestalX);
    const float semanticY = state.trace.localFrame <
        StingChameleonCapsuleCutscene::kPedestalLockFrame
        ? static_cast<float>(StingChameleonCapsuleCutscene::kInteractionPlayerY)
        : static_cast<float>(StingChameleonCapsuleCutscene::kPedestalY);
    return {
        state.trace.localFrame,
        state.trace.sourceFrame,
        state.trace.active,
        state.trace.grantEvent,
        state.trace.releaseEvent,
        state.trace.textBlipEvent,
        state.trace.textBlipCommand,
        armorBody,
        state.trace.pickupCollected,
        &state.trace.pickupId,
        state.postReleaseMotionActive,
        state.postReleaseSourceFrame,
        semanticX,
        semanticY,
    };
}

inline void writeTraceRows(
    std::FILE* trace,
    long tick,
    const TraceSnapshot& snapshot,
    float x,
    float y,
    float vx,
    float vy) {
    if (!trace || (!snapshot.active && !snapshot.grantEvent &&
                   !snapshot.releaseEvent &&
                   !snapshot.postReleaseMotionActive)) {
        return;
    }
    if (snapshot.active || snapshot.grantEvent || snapshot.releaseEvent) {
        const char* state = snapshot.releaseEvent
            ? "release"
            : (snapshot.grantEvent ? "grant" : "active");
        const int eventCode = snapshot.releaseEvent
            ? 2
            : (snapshot.grantEvent ? 1 : 0);
        std::fprintf(
            trace,
            "%ld,sting_chameleon_capsule,%d,%s,%d,%d,%.6f,%.6f,0,0\n",
            tick, snapshot.localFrame, state, snapshot.armorBody ? 1 : 0,
            eventCode, snapshot.semanticX, snapshot.semanticY);
        std::fprintf(
            trace,
            "%ld,sting_chameleon_capsule_source,%d,source_frame,%d,%d,%.6f,%.6f,0,0\n",
            tick, snapshot.localFrame, snapshot.sourceFrame, eventCode,
            snapshot.semanticX, snapshot.semanticY);
        if (snapshot.textBlipEvent) {
            std::fprintf(
                trace,
                "%ld,sting_chameleon_capsule_blip,%d,apu_0x0b,%d,%d,%.6f,%.6f,0,0\n",
                tick, snapshot.localFrame, snapshot.sourceFrame,
                snapshot.textBlipCommand, snapshot.semanticX,
                snapshot.semanticY);
        }
        if (snapshot.pickupCollected && snapshot.pickupId &&
            !snapshot.pickupId->empty()) {
            std::fprintf(
                trace,
                "%ld,sting_chameleon_capsule_pickup,%d,%s,1,2,%.6f,%.6f,0,0\n",
                tick, snapshot.localFrame, snapshot.pickupId->c_str(),
                snapshot.semanticX, snapshot.semanticY);
        }
    }
    if (snapshot.postReleaseMotionActive) {
        std::fprintf(
            trace,
            "%ld,sting_chameleon_capsule_release_motion,%d,engine_origin,%d,0,%.6f,%.6f,%.6f,%.6f\n",
            tick, snapshot.postReleaseSourceFrame,
            snapshot.postReleaseSourceFrame, x, y, vx, vy);
    }
}

} // namespace mmx::gameplay_sting_chameleon_capsule
