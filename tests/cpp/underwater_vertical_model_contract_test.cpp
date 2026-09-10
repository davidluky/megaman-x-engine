#include "entities/underwater_vertical_model.h"

#include <cassert>
#include <cstdio>

namespace {

void testDelayedHeldJumpMatchesMeasuredTrace() {
    mmx::UnderwaterVerticalModel model;
    model.reset(167895);
    assert(model.beginHeldJump());
    assert(!model.beginHeldJump());

    for (int sourceTick = 51; sourceTick <= 92; ++sourceTick) {
        const int expected = -1330 + (sourceTick - 51) * 33;
        assert(model.tick().deltaYQ8_8 == expected);
    }
    assert(model.worldYQ8_8() == 140448);
    assert(model.verticalDeltaQ8_8() == 56);

    assert(model.applyObservedApexTransitionAdjustment());
    assert(!model.applyObservedApexTransitionAdjustment());
    assert(model.tick().deltaYQ8_8 == 64);
    assert(model.tick().deltaYQ8_8 == 97);
}

void testApexAdjustmentRequiresExplicitFallingCommand() {
    mmx::UnderwaterVerticalModel model;
    model.reset(0);
    assert(!model.applyObservedApexTransitionAdjustment());
    assert(model.beginHeldJump());
    assert(!model.applyObservedApexTransitionAdjustment());

    while (model.verticalDeltaQ8_8() <= 0) {
        model.tick();
    }
    assert(model.verticalDeltaQ8_8() == 23);
    assert(!model.applyObservedApexTransitionAdjustment());
    assert(model.tick().deltaYQ8_8 == 23);
    assert(model.verticalDeltaQ8_8() == 56);
    assert(model.applyObservedApexTransitionAdjustment());
    assert(model.verticalDeltaQ8_8() == 64);
}

void testFallSpeedClampsAtMeasuredCap() {
    mmx::UnderwaterVerticalModel model;
    model.reset(0);
    assert(model.beginHeldJump());
    for (int tick = 0; tick < 200; ++tick) {
        const auto frame = model.tick();
        assert(frame.deltaYQ8_8 <= 737);
    }
    assert(model.verticalDeltaQ8_8() == 737);
    assert(model.tick().deltaYQ8_8 == 737);
}

void testMeasuredNeutralDescentAndAdapterOwnedSettle() {
    mmx::UnderwaterVerticalModel model;
    model.reset(150000);
    assert(model.beginMeasuredDescentAtCap());
    assert(model.tick().deltaYQ8_8 == 737);
    assert(model.tick().deltaYQ8_8 == 737);
    model.settle();
    assert(!model.active());
    assert(model.verticalDeltaQ8_8() == 0);
    assert(model.tick().deltaYQ8_8 == 0);
}

} // namespace

int main() {
    testDelayedHeldJumpMatchesMeasuredTrace();
    testApexAdjustmentRequiresExplicitFallingCommand();
    testFallSpeedClampsAtMeasuredCap();
    testMeasuredNeutralDescentAndAdapterOwnedSettle();
    std::printf("underwater vertical model contract: OK\n");
    return 0;
}
