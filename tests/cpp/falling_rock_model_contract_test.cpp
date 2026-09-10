#include "entities/falling_rock_model.h"

#include <cassert>
#include <cstdio>

namespace {

void testFirstObservedFallMatchesSourcePrefix() {
    mmx::FallingRockModel rock;
    rock.reset(1775 * 256, 151552);
    assert(rock.phase() == mmx::FallingRockPhase::Suspended);
    assert(rock.beginFall());
    assert(!rock.beginFall());

    for (int tick = 0; tick < 28; ++tick) {
        const auto frame = rock.tick();
        assert(frame.deltaXQ8_8 == 0);
        assert(frame.deltaYQ8_8 == tick * 64);
        assert(rock.worldXQ8_8() == 1775 * 256);
    }
    assert(rock.worldYQ8_8() == 175744);
    assert(rock.sourceRawYVelocityQ8_8() == -1792);
}

void testImpactRequiresExplicitAdapterCommand() {
    mmx::FallingRockModel rock;
    rock.reset(0, 0);
    assert(!rock.markImpact());
    assert(rock.beginFall());
    for (int tick = 0; tick < 100; ++tick) {
        rock.tick();
        assert(rock.phase() == mmx::FallingRockPhase::Falling);
    }
    assert(rock.markImpact());
    assert(rock.phase() == mmx::FallingRockPhase::Impacted);
    assert(rock.sourceRawYVelocityQ8_8() == 0);
    assert(rock.tick().deltaYQ8_8 == 0);
    assert(!rock.markImpact());
}

void testSuspendedAndImpactedTicksDoNotMove() {
    mmx::FallingRockModel rock;
    rock.reset(100, 200);
    assert(rock.tick().deltaYQ8_8 == 0);
    assert(rock.worldYQ8_8() == 200);
    assert(rock.beginFall());
    assert(rock.tick().deltaYQ8_8 == 0);
    assert(rock.markImpact());
    assert(rock.tick().deltaYQ8_8 == 0);
}

void testCauseNeutralClosureIsNotDestructionProof() {
    mmx::FallingRockModel rock;
    rock.reset(0, 0);
    assert(rock.closeUnknown());
    assert(!rock.active());
    assert(rock.phase() == mmx::FallingRockPhase::Closed);
    assert(rock.tick().deltaYQ8_8 == 0);
    assert(!rock.closeUnknown());
}

} // namespace

int main() {
    testFirstObservedFallMatchesSourcePrefix();
    testImpactRequiresExplicitAdapterCommand();
    testSuspendedAndImpactedTicksDoNotMove();
    testCauseNeutralClosureIsNotDestructionProof();
    std::printf("falling rock model contract: OK\n");
    return 0;
}
