// midboss_behavior_model.cpp - advances the bounded GC5 source model.

#include "entities/midboss_behavior_model.h"

#include <algorithm>

namespace mmx {

MidbossBehaviorModel::MidbossBehaviorModel(MidbossSourceProfile profile)
    : profile_(profile) {
    reset();
}

int MidbossBehaviorModel::maxHealth() const {
    return profile_ == MidbossSourceProfile::ThunderSlimer ? 48 : 64;
}

int MidbossBehaviorModel::contactDamage() const {
    return profile_ == MidbossSourceProfile::ThunderSlimer ? 5 : 4;
}

int MidbossBehaviorModel::rangedProjectileCount() const {
    return profile_ == MidbossSourceProfile::ThunderSlimer ? 2 : 3;
}

std::optional<int> MidbossBehaviorModel::rangedProjectileDamage() const {
    // Thunder's neutral bubbles never hit X in the promoted capture. Their
    // damage remains unknown instead of borrowing the 5-damage contact value.
    if (profile_ == MidbossSourceProfile::ThunderSlimer) {
        return std::nullopt;
    }
    return 2;
}

int MidbossBehaviorModel::introFrames() const {
    switch (profile_) {
        case MidbossSourceProfile::ThunderSlimer:
            return 26;
        case MidbossSourceProfile::FishSubmarineFirst:
            return 83;
        case MidbossSourceProfile::FishSubmarineSecond:
            return 89;
    }
    return 0;
}

int MidbossBehaviorModel::deathFrames() const {
    switch (profile_) {
        case MidbossSourceProfile::ThunderSlimer:
            return 277;
        case MidbossSourceProfile::FishSubmarineFirst:
            return 134;
        case MidbossSourceProfile::FishSubmarineSecond:
            return 131;
    }
    return 0;
}

void MidbossBehaviorModel::reset() {
    phase_ = MidbossPhase::Spawning;
    health_ = maxHealth();
    introFramesRemaining_ = introFrames();
    deathFramesRemaining_ = 0;
    activeTicks_ = 0;
}

int MidbossBehaviorModel::actionMarkerOrdinalAt(int activeTick) const {
    if (profile_ == MidbossSourceProfile::ThunderSlimer) {
        if (activeTick == 198) return 1;
        if (activeTick == 518) return 2;
        return 0;
    }
    if (activeTick == 21) return 1;
    if (activeTick == 298) return 2;
    if (activeTick == 717) return 3;
    return 0;
}

MidbossFrameResult MidbossBehaviorModel::tick() {
    MidbossFrameResult result;
    if (phase_ == MidbossPhase::Removed) return result;

    if (phase_ == MidbossPhase::Spawning) {
        if (introFramesRemaining_ > 0) introFramesRemaining_--;
        if (introFramesRemaining_ == 0) {
            phase_ = MidbossPhase::Active;
            result.becameActive = true;
        }
        return result;
    }

    if (phase_ == MidbossPhase::Dying) {
        if (deathFramesRemaining_ > 0) deathFramesRemaining_--;
        if (deathFramesRemaining_ == 0) {
            phase_ = MidbossPhase::Removed;
            result.removed = true;
        }
        return result;
    }

    activeTicks_++;
    const int ordinal = actionMarkerOrdinalAt(activeTicks_);
    if (ordinal != 0) {
        result.observedActionMarker = true;
        result.observedActionMarkerOrdinal = ordinal;
    }
    if (activeTicks_ == kObservedScheduleWindowTicks + 1) {
        result.scheduleEvidenceEnded = true;
    }
    return result;
}

MidbossDamageResult MidbossBehaviorModel::takeDamage(int amount) {
    MidbossDamageResult result;
    if (phase_ != MidbossPhase::Active || amount <= 0) return result;

    result.accepted = true;
    result.appliedDamage = std::min(amount, health_);
    health_ -= result.appliedDamage;
    if (health_ == 0) {
        phase_ = MidbossPhase::Dying;
        deathFramesRemaining_ = deathFrames();
        result.enteredDeath = true;
    }
    return result;
}

} // namespace mmx
