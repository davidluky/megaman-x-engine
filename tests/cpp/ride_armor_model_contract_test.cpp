#include "entities/ride_armor_model.h"

#include <cassert>
#include <cstdio>

namespace {

void finishMount(mmx::RideArmorModel& armor) {
    assert(armor.beginMount());
    for (int frame = 0; frame < mmx::RideArmorModel::kMountFrames - 1;
         ++frame) {
        const auto result = armor.tick({}, true);
        assert(!result.mountCompleted);
        assert(armor.phase() == mmx::RideArmorPhase::Mounting);
    }
    const auto completed = armor.tick({}, true);
    assert(completed.mountCompleted);
    assert(armor.phase() == mmx::RideArmorPhase::Ready);
}

void testPlacementMountMotionAndDismount() {
    mmx::RideArmorModel armor;
    armor.park(4640 * 256, 910 * 256);
    assert(armor.phase() == mmx::RideArmorPhase::Parked);
    assert(armor.health() == 16);
    assert(armor.worldXQ8_8() == 4640 * 256);
    assert(armor.worldYQ8_8() == 910 * 256);

    finishMount(armor);

    mmx::RideArmorInput right;
    right.rightHeld = true;
    assert(armor.tick(right, true).deltaXQ8_8 == 256);
    assert(armor.tick(right, true).deltaXQ8_8 == 512);

    mmx::RideArmorInput dash = right;
    dash.dashHeld = true;
    assert(armor.tick(dash, true).deltaXQ8_8 == 256);
    assert(armor.tick(dash, true).deltaXQ8_8 == 1024);
    assert(armor.tick(dash, true).deltaXQ8_8 == 0);
    assert(armor.tick(dash, true).deltaXQ8_8 == 1024);
    assert(armor.tick(dash, true).deltaXQ8_8 == 1024);
    assert(armor.tick(dash, true).deltaXQ8_8 == 1024);
    assert(armor.tick(dash, true).deltaXQ8_8 == 1024);

    mmx::RideArmorInput jump = right;
    jump.jumpPressed = true;
    assert(armor.tick(jump, true).deltaYQ8_8 == 0);
    int rise = 0;
    for (int frame = 0; frame < 21; ++frame) {
        rise += armor.tick(right, false).deltaYQ8_8;
    }
    assert(rise == -54 * 256);
    assert(armor.phase() == mmx::RideArmorPhase::Airborne);

    mmx::RideArmorInput upOnly;
    upOnly.upHeld = true;
    assert(!armor.tick(upOnly, false).dismounted);
    mmx::RideArmorInput jumpOnly;
    jumpOnly.jumpPressed = true;
    assert(!armor.tick(jumpOnly, false).dismounted);
    mmx::RideArmorInput dismount;
    dismount.upHeld = true;
    dismount.jumpPressed = true;
    assert(armor.tick(dismount, false).dismounted);
    assert(armor.phase() == mmx::RideArmorPhase::Parked);
}

void testPunchStartupLifetimeAndNoBusyRepeat() {
    mmx::RideArmorModel armor;
    armor.park(0, 0);
    finishMount(armor);

    mmx::RideArmorInput punch;
    punch.punchPressed = true;
    assert(armor.tick(punch, true).punchStarted);
    assert(!armor.punchActive());
    assert(!armor.tick(punch, true).punchStarted);
    assert(!armor.punchActive());
    assert(!armor.tick({}, true).punchStarted);
    assert(armor.punchActive());
    assert(armor.punchActiveFramesRemaining() == 40);

    for (int frame = 0; frame < 39; ++frame) {
        assert(!armor.tick(punch, true).punchStarted);
        assert(armor.punchActive());
    }
    assert(!armor.tick({}, true).punchStarted);
    assert(!armor.punchActive());
    assert(mmx::RideArmorModel::kPunchDamage == 5);

    assert(armor.tick(punch, true).punchStarted);
}

void testDamageInvulnerabilityZeroLatchAndDestruction() {
    mmx::RideArmorModel armor;
    armor.park(0, 0);
    finishMount(armor);

    auto hit = armor.takeIncomingDamage(2);
    assert(hit.accepted);
    assert(hit.hurtFlash);
    assert(!hit.protectionLost);
    assert(hit.playerDamage == 0);
    assert(armor.health() == 14);
    assert(armor.invulnerabilityFrames() == 121);
    assert(!armor.takeIncomingDamage(3).accepted);

    for (int frame = 0; frame < 120; ++frame) {
        armor.tick({}, true);
    }
    assert(armor.invulnerabilityFrames() == 1);
    assert(!armor.takeIncomingDamage(3).accepted);
    armor.tick({}, true);
    assert(armor.invulnerabilityFrames() == 0);

    assert(armor.takeIncomingDamage(13).accepted);
    assert(armor.health() == 1);
    for (int frame = 0; frame < 121; ++frame) {
        armor.tick({}, true);
    }
    assert(armor.takeIncomingDamage(3).accepted);
    assert(armor.health() == 0);
    assert(armor.active());
    assert(armor.controlsEnabled());
    assert(armor.invulnerabilityFrames() == 121);

    armor.tick({}, true);
    assert(armor.health() == 0);
    assert(armor.invulnerabilityFrames() == 0);
    assert(armor.active());

    const auto trigger = armor.takeIncomingDamage(2);
    assert(trigger.accepted);
    assert(trigger.protectionLost);
    assert(trigger.pilotEjected);
    assert(trigger.playerDamage == 1);
    assert(armor.phase() == mmx::RideArmorPhase::Destroying);
    assert(armor.destructionCountdownSteps() == 30);

    for (int frame = 0; frame < 59; ++frame) {
        const auto result = armor.tick({}, true);
        assert(!result.objectRemoved);
        assert(armor.active());
    }
    assert(armor.destructionCountdownSteps() == 1);
    const auto removed = armor.tick({}, true);
    assert(removed.objectRemoved);
    assert(!armor.active());
    assert(armor.phase() == mmx::RideArmorPhase::Removed);
}

} // namespace

int main() {
    testPlacementMountMotionAndDismount();
    testPunchStartupLifetimeAndNoBusyRepeat();
    testDamageInvulnerabilityZeroLatchAndDestruction();
    std::printf("ride-armor model contract: OK\n");
    return 0;
}
