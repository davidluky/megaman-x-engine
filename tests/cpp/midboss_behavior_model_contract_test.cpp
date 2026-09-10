#include "entities/midboss_behavior_model.h"

#include <cassert>
#include <cstdio>
#include <vector>

namespace {

void advanceToActive(mmx::MidbossBehaviorModel& model) {
    for (int frame = 1; frame < model.introFrames(); ++frame) {
        assert(!model.tick().becameActive);
        assert(model.phase() == mmx::MidbossPhase::Spawning);
    }
    assert(model.tick().becameActive);
    assert(model.phase() == mmx::MidbossPhase::Active);
    assert(model.activeTicks() == 0);
}

void testSourceProfilesAndLifecycle() {
    mmx::MidbossBehaviorModel thunder(
        mmx::MidbossSourceProfile::ThunderSlimer);
    assert(thunder.maxHealth() == 48);
    assert(thunder.contactDamage() == 5);
    assert(thunder.rangedProjectileCount() == 2);
    assert(!thunder.rangedProjectileDamage().has_value());
    assert(thunder.introFrames() == 26);
    assert(thunder.deathFrames() == 277);
    advanceToActive(thunder);

    mmx::MidbossBehaviorModel fish1(
        mmx::MidbossSourceProfile::FishSubmarineFirst);
    assert(fish1.maxHealth() == 64);
    assert(fish1.contactDamage() == 4);
    assert(fish1.rangedProjectileCount() == 3);
    assert(fish1.rangedProjectileDamage().value() == 2);
    assert(fish1.introFrames() == 83);
    assert(fish1.deathFrames() == 134);

    mmx::MidbossBehaviorModel fish2(
        mmx::MidbossSourceProfile::FishSubmarineSecond);
    assert(fish2.introFrames() == 89);
    assert(fish2.deathFrames() == 131);
}

void testObservedActionMarkersStopAtEvidenceBoundary() {
    mmx::MidbossBehaviorModel thunder(
        mmx::MidbossSourceProfile::ThunderSlimer);
    advanceToActive(thunder);
    std::vector<int> thunderMarkers;
    for (int tick = 1; tick <= 1000; ++tick) {
        const auto result = thunder.tick();
        if (result.observedActionMarker) thunderMarkers.push_back(tick);
        assert(result.scheduleEvidenceEnded ==
               (tick == mmx::MidbossBehaviorModel::kObservedScheduleWindowTicks + 1));
    }
    assert((thunderMarkers == std::vector<int>{198, 518}));
    assert(!thunder.withinObservedScheduleWindow());

    mmx::MidbossBehaviorModel fish(
        mmx::MidbossSourceProfile::FishSubmarineFirst);
    advanceToActive(fish);
    std::vector<int> fishMarkers;
    for (int tick = 1; tick <= 1000; ++tick) {
        const auto result = fish.tick();
        if (result.observedActionMarker) fishMarkers.push_back(tick);
    }
    assert((fishMarkers == std::vector<int>{21, 298, 717}));
}

void testDamageClampsAndDeathUsesTheSelectedSourceInstance() {
    mmx::MidbossBehaviorModel fish(
        mmx::MidbossSourceProfile::FishSubmarineSecond);
    assert(!fish.takeDamage(8).accepted);
    advanceToActive(fish);
    assert(!fish.takeDamage(0).accepted);

    const auto first = fish.takeDamage(61);
    assert(first.accepted);
    assert(first.appliedDamage == 61);
    assert(!first.enteredDeath);
    assert(fish.health() == 3);

    const auto terminal = fish.takeDamage(8);
    assert(terminal.accepted);
    assert(terminal.appliedDamage == 3);
    assert(terminal.enteredDeath);
    assert(fish.phase() == mmx::MidbossPhase::Dying);
    assert(fish.deathFramesRemaining() == 131);
    assert(!fish.takeDamage(1).accepted);

    for (int frame = 1; frame < fish.deathFrames(); ++frame) {
        assert(!fish.tick().removed);
    }
    assert(fish.tick().removed);
    assert(fish.phase() == mmx::MidbossPhase::Removed);
}

} // namespace

int main() {
    testSourceProfilesAndLifecycle();
    testObservedActionMarkersStopAtEvidenceBoundary();
    testDamageClampsAndDeathUsesTheSelectedSourceInstance();
    std::printf("midboss behavior model contract: OK\n");
    return 0;
}
