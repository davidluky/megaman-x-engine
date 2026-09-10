#include "entities/moving_platform_model.h"

#include <cassert>
#include <cstdio>

namespace {

void testMeasuredHorizontalMotionAndAdapterReversal() {
    mmx::MovingPlatformModel platform;
    platform.reset(585 * 256, 1320 * 256);
    assert(platform.phase() == mmx::MovingPlatformPhase::Dormant);
    assert(platform.beginHorizontal(mmx::MovingPlatformDirection::Right));
    assert(
        platform.horizontalVelocityQ8_8() ==
        mmx::MovingPlatformModel::kHorizontalSpeedQ8_8
    );

    const auto right = platform.tick();
    assert(right.deltaXQ8_8 == 384);
    assert(right.deltaYQ8_8 == 0);
    assert(platform.worldXQ8_8() == 585 * 256 + 384);

    assert(
        platform.reverseAtBoundary(mmx::MovingPlatformDirection::Left)
    );
    const auto left = platform.tick();
    assert(left.deltaXQ8_8 == -384);
    assert(left.deltaYQ8_8 == 0);
}

void testMeasuredVerticalPhaseFreezesHorizontalPosition() {
    mmx::MovingPlatformModel platform;
    platform.reset(613 * 256, 1152 * 256);
    assert(platform.beginHorizontal(mmx::MovingPlatformDirection::Left));
    assert(platform.beginMeasuredVerticalPhase());
    assert(platform.phase() == mmx::MovingPlatformPhase::Vertical);
    assert(platform.horizontalVelocityQ8_8() == -384);
    assert(
        mmx::MovingPlatformModel::kSourceVerticalVelocityRaw == -512
    );

    const int x = platform.worldXQ8_8();
    for (int tick = 0; tick < 20; ++tick) {
        const auto frame = platform.tick();
        assert(frame.deltaXQ8_8 == 0);
        assert(frame.deltaYQ8_8 == 512);
        assert(platform.worldXQ8_8() == x);
    }
    assert(platform.worldYQ8_8() == 1152 * 256 + 20 * 512);
    assert(
        !platform.reverseAtBoundary(
            mmx::MovingPlatformDirection::Right
        )
    );
}

void testTransitionsRequireExplicitAdapterCommands() {
    mmx::MovingPlatformModel platform;
    platform.reset(0, 0);
    assert(!platform.beginMeasuredVerticalPhase());
    assert(
        !platform.reverseAtBoundary(
            mmx::MovingPlatformDirection::Left
        )
    );
    assert(platform.beginHorizontal(mmx::MovingPlatformDirection::Left));
    for (int tick = 0; tick < 100; ++tick) {
        assert(platform.tick().deltaXQ8_8 == -384);
        assert(platform.phase() == mmx::MovingPlatformPhase::Horizontal);
    }
}

void testCauseNeutralClosureIsNotDestruction() {
    mmx::MovingPlatformModel platform;
    platform.reset(0, 0);
    assert(platform.beginHorizontal(mmx::MovingPlatformDirection::Right));
    assert(platform.closeOffscreen());
    assert(!platform.active());
    assert(platform.phase() == mmx::MovingPlatformPhase::Closed);
    assert(
        platform.closureCause() ==
        mmx::MovingPlatformClosureCause::Offscreen
    );
    assert(platform.tick().deltaXQ8_8 == 0);
    assert(!platform.closeUnknown());

    platform.reset(0, 0);
    assert(platform.closeUnknown());
    assert(
        platform.closureCause() ==
        mmx::MovingPlatformClosureCause::Unknown
    );
}

} // namespace

int main() {
    testMeasuredHorizontalMotionAndAdapterReversal();
    testMeasuredVerticalPhaseFreezesHorizontalPosition();
    testTransitionsRequireExplicitAdapterCommands();
    testCauseNeutralClosureIsNotDestruction();
    std::printf("moving-platform model contract: OK\n");
    return 0;
}
