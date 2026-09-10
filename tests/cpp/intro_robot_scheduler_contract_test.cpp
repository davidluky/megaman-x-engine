// FE5.1-SCHEDULER-RUNTIME-KERNEL contract for the evidence-bounded Intro
// Highway OID 0x29 scheduler. Authoritative source evidence lives in:
//   docs/evidence/2026-07-15-fe51-intro-robot-scheduler/
//
// This std-only contract owns idle/helper/RNG/action cadence and constructor
// call events. It intentionally does not bind the source-global RNG, choose
// activation/offscreen policy, materialize constructor geometry, model
// allocator-full behavior, or claim animation/pixel parity.

#include "entities/intro_robot_scheduler.h"

#include <cassert>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

using mmx::IntroRobotScheduler;

namespace {

struct LowByteQueue {
    explicit LowByteQueue(std::vector<std::uint8_t> bytes)
        : values(std::move(bytes)) {}

    std::uint8_t next() {
        assert(index < values.size());
        return values[index++];
    }

    std::vector<std::uint8_t> values;
    std::size_t index = 0;
};

struct WaitMeasurement {
    int updates = 0;
    std::vector<std::uint8_t> helperStatesAtExpiry;
    std::size_t draws = 0;
    std::uint8_t selectedAction = 0;
    std::uint8_t selectedVariant = 0;
};

WaitMeasurement measureNextIdleWait(std::vector<std::uint8_t> lows) {
    IntroRobotScheduler scheduler;
    LowByteQueue queue(std::move(lows));
    const IntroRobotScheduler::RngLowProvider rng =
        [&queue]() { return queue.next(); };

    // Select and finish the normalized initial action. The following idle
    // episode begins with helper state 0x81.
    while (scheduler.action() == IntroRobotScheduler::kIdleAction) {
        scheduler.update(rng);
    }
    while (scheduler.action() != IntroRobotScheduler::kIdleAction) {
        scheduler.update(rng);
    }

    WaitMeasurement result;
    do {
        scheduler.update(rng);
        ++result.updates;
        if (result.updates % IntroRobotScheduler::kIdleBlockUpdates == 0) {
            result.helperStatesAtExpiry.push_back(scheduler.helperState());
        }
    } while (scheduler.action() == IntroRobotScheduler::kIdleAction);
    result.draws = queue.index;
    result.selectedAction = scheduler.action();
    result.selectedVariant = scheduler.variant();
    return result;
}

} // namespace

int main() {
    // Initialization is the engine-normalized action-2 entry. An absent RNG
    // provider must fail closed before any timer or state advances; the kernel
    // owns no hidden fallback PRNG.
    {
        IntroRobotScheduler scheduler;
        assert(scheduler.action() == IntroRobotScheduler::kIdleAction);
        assert(scheduler.waitTimer() ==
               IntroRobotScheduler::kIdleBlockUpdates);
        assert(scheduler.helperState() == 0);
        assert(scheduler.actionAge() == 0);

        const auto blocked = scheduler.update({});
        assert(!blocked.advanced);
        assert(blocked.providerMissing);
        assert(!blocked.enteredAction);
        assert(!blocked.enteredIdle);
        assert(blocked.callCount == 0);
        assert(scheduler.action() == IntroRobotScheduler::kIdleAction);
        assert(scheduler.waitTimer() ==
               IntroRobotScheduler::kIdleBlockUpdates);
        assert(scheduler.helperState() == 0);
        assert(scheduler.lastStep().providerMissing);
    }

    // Missing-provider failure is atomic at the RNG boundary too: reaching
    // timer 1 with a valid-but-unused provider does not permit the next update
    // to select an action until the caller supplies source RNG.
    {
        IntroRobotScheduler scheduler;
        const IntroRobotScheduler::RngLowProvider unused = []() {
            assert(false);
            return std::uint8_t{0};
        };
        for (int idleUpdate = 1; idleUpdate < 60; ++idleUpdate) {
            scheduler.update(unused);
        }
        assert(scheduler.waitTimer() == 1);

        const auto blocked = scheduler.update({});
        assert(blocked.providerMissing);
        assert(!blocked.advanced);
        assert(scheduler.waitTimer() == 1);
        assert(scheduler.helperState() == 0);

        LowByteQueue queue({1, 10});
        const auto selected = scheduler.update([&queue]() {
            return queue.next();
        });
        assert(selected.enteredAction);
        assert(scheduler.action() == IntroRobotScheduler::kAction6);
        assert(scheduler.variant() == 2);
        assert(queue.index == 2);
    }

    // State-zero helper entry consumes no RNG. Two distinct subsequent lows
    // select action 6 and variant 2. Selection occurs on idle update 60;
    // active update 80 exposes both ordered calls and active update 91 returns
    // to a fresh 60-update idle block.
    {
        IntroRobotScheduler scheduler;
        LowByteQueue queue({1, 10});
        const IntroRobotScheduler::RngLowProvider rng =
            [&queue]() { return queue.next(); };

        for (int idleUpdate = 1; idleUpdate < 60; ++idleUpdate) {
            const auto step = scheduler.update(rng);
            assert(step.advanced);
            assert(!step.enteredAction);
            assert(step.callCount == 0);
            assert(scheduler.action() == IntroRobotScheduler::kIdleAction);
            assert(scheduler.waitTimer() == 60 - idleUpdate);
            assert(queue.index == 0);
        }

        const auto selected = scheduler.update(rng);
        assert(selected.advanced);
        assert(selected.enteredAction);
        assert(selected.activeUpdate == 0);
        assert(scheduler.action() == IntroRobotScheduler::kAction6);
        assert(scheduler.variant() == 2);
        assert(scheduler.helperState() == 0x81);
        assert(queue.index == 2);

        for (int activeUpdate = 1; activeUpdate < 80; ++activeUpdate) {
            const auto step = scheduler.update(rng);
            assert(step.activeUpdate == activeUpdate);
            assert(step.callCount == 0);
            assert(queue.index == 2);
        }

        const auto calls = scheduler.update(rng);
        assert(calls.activeUpdate == 80);
        assert(calls.callCount == 2);
        assert(calls.calls[0].action == IntroRobotScheduler::kAction6);
        assert(calls.calls[0].variant == 2);
        assert(calls.calls[0].callIndex == 1);
        assert(calls.calls[1].action == IntroRobotScheduler::kAction6);
        assert(calls.calls[1].variant == 2);
        assert(calls.calls[1].callIndex == 2);

        const auto blockedAfterCalls = scheduler.update({});
        assert(blockedAfterCalls.providerMissing);
        assert(!blockedAfterCalls.advanced);
        assert(blockedAfterCalls.callCount == 0);
        assert(scheduler.actionAge() == 80);
        assert(scheduler.lastStep().callCount == 0);

        const auto cleared = scheduler.update(rng);
        assert(cleared.activeUpdate == 81);
        assert(cleared.callCount == 0);
        assert(scheduler.lastStep().callCount == 0);

        for (int activeUpdate = 82; activeUpdate <= 91; ++activeUpdate) {
            const auto step = scheduler.update(rng);
            assert(step.activeUpdate == activeUpdate);
            if (activeUpdate < 91) {
                assert(!step.enteredIdle);
                assert(scheduler.action() == IntroRobotScheduler::kAction6);
            } else {
                assert(step.enteredIdle);
                assert(scheduler.action() ==
                       IntroRobotScheduler::kIdleAction);
                assert(scheduler.waitTimer() ==
                       IntroRobotScheduler::kIdleBlockUpdates);
                assert(scheduler.actionAge() == 0);
                assert(scheduler.variant() == 2);
            }
            assert(step.callCount == 0);
        }
        assert(queue.index == 2); // active dispatch consumes no scheduler RNG

        const auto clearedIdleEvent = scheduler.update(rng);
        assert(clearedIdleEvent.advanced);
        assert(!clearedIdleEvent.enteredIdle);
        assert(scheduler.waitTimer() == 59);

        scheduler.reset();
        assert(scheduler.action() == IntroRobotScheduler::kIdleAction);
        assert(scheduler.variant() == 0);
        assert(scheduler.helperState() == 0);
        assert(scheduler.waitTimer() ==
               IntroRobotScheduler::kIdleBlockUpdates);
        assert(scheduler.actionAge() == 0);
        assert(!scheduler.lastStep().advanced);
        assert(!scheduler.lastStep().providerMissing);
        assert(scheduler.lastStep().callCount == 0);
    }

    // Action 4 retains the separate variant low, calls on active updates 80
    // and 123, and returns to idle after exactly 134 active updates. On the
    // local test clock these are ticks 140, 183, and 194 respectively.
    {
        IntroRobotScheduler scheduler;
        LowByteQueue queue({0, 14});
        const IntroRobotScheduler::RngLowProvider rng =
            [&queue]() { return queue.next(); };
        std::vector<int> callTicks;
        std::vector<int> callIndices;
        int idleReturnTick = 0;

        for (int tick = 1; tick <= 194; ++tick) {
            const auto step = scheduler.update(rng);
            if (step.enteredAction) {
                assert(tick == 60);
                assert(scheduler.action() == IntroRobotScheduler::kAction4);
                assert(scheduler.variant() == 6);
            }
            if (step.callCount != 0) {
                assert(step.callCount == 1);
                assert(step.calls[0].action == IntroRobotScheduler::kAction4);
                assert(step.calls[0].variant == 6);
                callTicks.push_back(tick);
                callIndices.push_back(step.calls[0].callIndex);
                assert(step.activeUpdate ==
                       (callTicks.size() == 1 ? 80 : 123));
            }
            if (step.enteredIdle) {
                idleReturnTick = tick;
                assert(step.activeUpdate == 134);
            }
        }

        assert((callTicks == std::vector<int>{140, 183}));
        assert((callIndices == std::vector<int>{1, 1}));
        assert(idleReturnTick == 194);
        assert(scheduler.action() == IntroRobotScheduler::kIdleAction);
        assert(scheduler.waitTimer() ==
               IntroRobotScheduler::kIdleBlockUpdates);
        assert(scheduler.variant() == 6);
        assert(queue.index == 2);
    }

    // The helper recurrence yields the three dynamically observed idle waits.
    // Every action/variant pair uses an observed action-6/variant-2 witness;
    // helper lows alone choose 60, 120, or 180 updates.
    {
        const auto wait60 = measureNextIdleWait({1, 2, 0, 1, 2});
        assert(wait60.updates == 60);
        assert((wait60.helperStatesAtExpiry ==
                std::vector<std::uint8_t>{0x82}));
        assert(wait60.draws == 5);
        assert(wait60.selectedAction == IntroRobotScheduler::kAction6);
        assert(wait60.selectedVariant == 2);

        const auto wait120 = measureNextIdleWait({1, 2, 1, 0, 1, 2});
        assert(wait120.updates == 120);
        assert((wait120.helperStatesAtExpiry ==
                std::vector<std::uint8_t>{1, 0x81}));
        assert(wait120.draws == 6);
        assert(wait120.selectedAction == IntroRobotScheduler::kAction6);
        assert(wait120.selectedVariant == 2);

        const auto wait180 = measureNextIdleWait({1, 2, 1, 1, 1, 1, 2});
        assert(wait180.updates == 180);
        assert((wait180.helperStatesAtExpiry ==
                std::vector<std::uint8_t>{1, 2, 0x81}));
        assert(wait180.draws == 7);
        assert(wait180.selectedAction == IntroRobotScheduler::kAction6);
        assert(wait180.selectedVariant == 2);
    }

    // Replay the packet's first complete normalized RNG/cadence prefix. This
    // covers both observed 0x82 -> 1 parities, exact helper transition order,
    // distinct action/variant draws, and every call before the final action
    // entry without treating successful packet births as kernel events.
    {
        LowByteQueue queue({157, 18, 25, 246, 0, 30, 222, 121, 75, 97, 218,
                            43, 30, 42, 184, 100, 52, 171, 194, 192, 188});
        const IntroRobotScheduler::RngLowProvider rng =
            [&queue]() { return queue.next(); };
        IntroRobotScheduler scheduler;
        std::vector<int> helperTicks;
        std::vector<std::uint8_t> helperStates;
        std::vector<int> entryTicks;
        std::vector<std::uint8_t> entryActions;
        std::vector<std::uint8_t> entryVariants;
        std::vector<int> callTicks;
        std::vector<std::size_t> callCounts;
        auto previousHelper = scheduler.helperState();

        for (int tick = 1; tick <= 1141; ++tick) {
            const auto step = scheduler.update(rng);
            if (scheduler.helperState() != previousHelper) {
                helperTicks.push_back(tick);
                helperStates.push_back(scheduler.helperState());
                previousHelper = scheduler.helperState();
            }
            if (step.enteredAction) {
                entryTicks.push_back(tick);
                entryActions.push_back(scheduler.action());
                entryVariants.push_back(scheduler.variant());
            }
            if (step.callCount != 0) {
                callTicks.push_back(tick);
                callCounts.push_back(step.callCount);
                for (std::size_t i = 0; i < step.callCount; ++i) {
                    assert(step.calls[i].action == scheduler.action());
                    assert(step.calls[i].variant == scheduler.variant());
                }
                if (step.calls[0].action == IntroRobotScheduler::kAction4) {
                    assert(step.callCount == 1);
                    assert(step.calls[0].callIndex == 1);
                } else {
                    assert(step.callCount == 2);
                    assert(step.calls[0].callIndex == 1);
                    assert(step.calls[1].callIndex == 2);
                }
            }
        }

        assert((helperTicks ==
                std::vector<int>{60, 211, 271, 465, 616, 676, 827, 1021,
                                 1081, 1141}));
        assert((helperStates == std::vector<std::uint8_t>{
                                    0x81, 1, 0x81, 0x82, 1,
                                    0x81, 0x82, 1, 2, 0x81}));
        assert((entryTicks ==
                std::vector<int>{60, 271, 465, 676, 827, 1141}));
        assert((entryActions == std::vector<std::uint8_t>{6, 4, 6, 6, 4, 4}));
        assert((entryVariants ==
                std::vector<std::uint8_t>{2, 6, 3, 6, 4, 4}));
        assert((callTicks ==
                std::vector<int>{140, 351, 394, 545, 756, 907, 950}));
        assert((callCounts ==
                std::vector<std::size_t>{2, 1, 1, 2, 2, 1, 1}));
        assert(queue.index == queue.values.size());
    }

    return 0;
}
