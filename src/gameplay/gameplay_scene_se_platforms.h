// gameplay_scene_se_platforms.h - R96C Storm Eagle scene seam.
//
// This adapter owns only canonical-stage scope, allocation/initialization
// ordering, completed render snapshots, and the measured primary top-contact
// handoff. Secondary/wall masks remain outside this first bounded contract.

#pragma once

#include "entities/storm_eagle_platform.h"
#include "entities/storm_eagle_platform_animation.h"

#include <cstdint>
#include <string>

namespace mmx {

class Player;
class TextureResource;

struct StormEaglePlatformSceneConfig {
    std::string activeStageId;
    std::string stagePath;
    bool randomizerMode = false;
    bool arenaWaveMode = false;
    bool bossRushMode = false;
};

class StormEaglePlatformScene {
public:
    struct RenderSnapshot {
        StormEaglePlatformState state{};
        std::uint8_t pose = 0;
        bool valid = false;
    };

    enum class Lifecycle {
        Disabled,
        Unallocated,
        Allocated,
        Initialized,
        Running,
    };

    static bool canonicalScope(const StormEaglePlatformSceneConfig& config);

    void configure(const StormEaglePlatformSceneConfig& config);
    void reset();

    // Source X is an integer source anchor.  The first value at or beyond
    // 256 allocates once; a source flag is retained for fromSourceFlag during
    // the following initialization step.
    void beginFrame(std::uint16_t sourceX, std::uint8_t sourceFlag);
    // Performs exactly one pre-action lifecycle transition. Returns whether
    // the caller must run motion/contact after Player/tile physics this tick.
    bool prepareSourceFrame(std::uint16_t sourceX, std::uint8_t sourceFlag);
    void initializePending();
    void tickMotion();
    // Completes the current source object step after contact. Animation and
    // renderer queueing are cull-gated here, after physical motion/contact.
    void finishSourceFrame(std::uint16_t cameraX, std::uint16_t cameraY);
    // Draws only the already-shifted display snapshot. Rendering never mutates
    // pending/display queue state.
    void render(float cameraX, float cameraY, float alpha = 1.0f) const;
    // Per-record contact seam shared by the focused test and eventual scene.
    // It preserves source record carry and native fractions without snapping.
    bool resolvePlayerContact(Player& player, std::uint16_t cameraX,
                              std::uint16_t cameraY);

    Lifecycle lifecycle() const { return lifecycle_; }
    bool enabled() const { return enabled_; }
    std::uint16_t sourceX() const { return sourceX_; }
    std::uint8_t sourceFlag() const { return sourceFlag_; }

    const StormEaglePlatformState& state() const { return platform_.state(); }
    bool hasCompletedRenderState() const { return hasCompletedRenderState_; }
    const StormEaglePlatformState& previousRenderState() const {
        return previousRenderState_;
    }
    const StormEaglePlatformState& currentRenderState() const {
        return currentRenderState_;
    }
    const RenderSnapshot& displayedRenderSnapshot() const {
        return displayedRender_;
    }

    bool previousSupport() const { return playerSupportLatched_; }
    void finishContact(bool currentContact) {
        playerSupportLatched_ = currentContact;
    }

private:
    StormEaglePlatformSceneConfig config_{};
    StormEaglePlatform platform_{};
    StormEaglePlatformAnimation animation_{};
    StormEaglePlatformState previousRenderState_{};
    StormEaglePlatformState currentRenderState_{};
    Lifecycle lifecycle_ = Lifecycle::Disabled;
    bool enabled_ = false;
    bool hasCompletedRenderState_ = false;
    // Player support is aggregate scene state.  The platform's source D+2C
    // record latch is separate and must not be used as global player ground.
    bool playerSupportLatched_ = false;
    bool recordSupportLatched_ = false;
    std::uint16_t sourceX_ = 0;
    std::uint8_t sourceFlag_ = 0;
    RenderSnapshot pendingRender_{};
    RenderSnapshot displayedRender_{};
    bool pendingRenderQueued_ = false;
    const TextureResource* texture_ = nullptr;
};

}  // namespace mmx
