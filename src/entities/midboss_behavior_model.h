// midboss_behavior_model.h - bounded source-backed GC5 fixed-tick model.
// Scene adapters own collision, projectile motion, rendering, audio, and placement.

#pragma once

#include <optional>

namespace mmx {

enum class MidbossSourceProfile {
    ThunderSlimer,
    FishSubmarineFirst,
    FishSubmarineSecond
};

enum class MidbossPhase {
    Spawning,
    Active,
    Dying,
    Removed
};

struct MidbossFrameResult {
    bool becameActive = false;
    bool observedActionMarker = false;
    int observedActionMarkerOrdinal = 0;
    bool scheduleEvidenceEnded = false;
    bool removed = false;
};

struct MidbossDamageResult {
    bool accepted = false;
    int appliedDamage = 0;
    bool enteredDeath = false;
};

class MidbossBehaviorModel {
public:
    static constexpr int kObservedScheduleWindowTicks = 720;

    explicit MidbossBehaviorModel(MidbossSourceProfile profile);

    void reset();
    MidbossFrameResult tick();
    MidbossDamageResult takeDamage(int amount);

    MidbossSourceProfile profile() const { return profile_; }
    MidbossPhase phase() const { return phase_; }
    int health() const { return health_; }
    int maxHealth() const;
    int contactDamage() const;
    int rangedProjectileCount() const;
    std::optional<int> rangedProjectileDamage() const;
    int introFrames() const;
    int deathFrames() const;
    int activeTicks() const { return activeTicks_; }
    int deathFramesRemaining() const { return deathFramesRemaining_; }
    bool withinObservedScheduleWindow() const {
        return activeTicks_ <= kObservedScheduleWindowTicks;
    }

private:
    int actionMarkerOrdinalAt(int activeTick) const;

    MidbossSourceProfile profile_;
    MidbossPhase phase_ = MidbossPhase::Spawning;
    int health_ = 0;
    int introFramesRemaining_ = 0;
    int deathFramesRemaining_ = 0;
    int activeTicks_ = 0;
};

} // namespace mmx
