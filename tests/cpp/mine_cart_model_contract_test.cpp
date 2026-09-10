#include "entities/mine_cart_model.h"

#include <array>
#include <cassert>
#include <cstdio>

namespace {

void testLifecycleAndMeasuredAcceleration() {
    mmx::MineCartModel cart;
    cart.reset(5847 * 256, 1101 * 256);
    assert(cart.phase() == mmx::MineCartPhase::Dormant);
    assert(cart.worldXQ8_8() == 5847 * 256);
    assert(cart.worldYQ8_8() == 1101 * 256);
    assert(!cart.beginMotion(mmx::MineCartDirection::Right));
    assert(cart.wake());
    assert(!cart.wake());
    assert(cart.beginMotion(mmx::MineCartDirection::Right));
    assert(
        cart.horizontalVelocityQ8_8() ==
        mmx::MineCartModel::kInitialHorizontalVelocityQ8_8
    );

    int expected = mmx::MineCartModel::kInitialHorizontalVelocityQ8_8;
    for (; expected <= mmx::MineCartModel::kHorizontalVelocityCeilingQ8_8;
         expected += mmx::MineCartModel::kHorizontalAccelerationQ8_8) {
        const auto result = cart.tick({}, {});
        assert(result.cartDeltaXQ8_8 == expected);
        assert(result.cartDeltaYQ8_8 == 0);
    }
    assert(
        cart.horizontalVelocityQ8_8() ==
        mmx::MineCartModel::kHorizontalVelocityCeilingQ8_8
    );
    assert(cart.tick({}, {}).cartDeltaXQ8_8 == 1280);

    assert(cart.turnAtTrackBoundary(mmx::MineCartDirection::Left));
    mmx::MineCartTrackStep climb;
    climb.verticalDeltaQ8_8 = -338;
    const auto reversed = cart.tick({}, climb);
    assert(reversed.cartDeltaXQ8_8 == -1280);
    assert(reversed.cartDeltaYQ8_8 == -338);
}

void testAccelerationCadenceRemainsAnAdapterDecision() {
    mmx::MineCartModel cart;
    cart.reset(0, 0);
    assert(cart.wake());
    assert(cart.beginMotion(mmx::MineCartDirection::Right));

    mmx::MineCartTrackStep held;
    held.advanceHorizontalAcceleration = false;
    for (int tick = 0; tick < 7; ++tick) {
        assert(cart.tick({}, held).cartDeltaXQ8_8 == 256);
        assert(cart.horizontalVelocityQ8_8() == 256);
    }
    assert(cart.tick({}, {}).cartDeltaXQ8_8 == 256);
    assert(cart.horizontalVelocityQ8_8() == 288);
    assert(cart.tick({}, {}).cartDeltaXQ8_8 == 288);
}

void testRiderControlsDoNotSteerTheCart() {
    std::array<mmx::MineCartModel, 3> carts;
    for (auto& cart : carts) {
        cart.reset(5847 * 256, 1101 * 256);
        assert(cart.wake());
        assert(cart.beginMotion(mmx::MineCartDirection::Right));
    }

    mmx::MineCartRiderInput left;
    left.leftHeld = true;
    mmx::MineCartRiderInput right;
    right.rightHeld = true;
    mmx::MineCartRiderInput jump;
    jump.jumpPressed = true;
    const std::array<mmx::MineCartRiderInput, 3> inputs = {
        left, right, jump
    };

    for (int tick = 0; tick < 40; ++tick) {
        mmx::MineCartTrackStep track;
        track.verticalDeltaQ8_8 = tick < 20 ? 0 : 13;
        std::array<mmx::MineCartFrameResult, 3> results;
        for (std::size_t index = 0; index < carts.size(); ++index) {
            results[index] = carts[index].tick(inputs[index], track);
        }
        assert(
            results[0].cartDeltaXQ8_8 ==
            results[1].cartDeltaXQ8_8
        );
        assert(
            results[1].cartDeltaXQ8_8 ==
            results[2].cartDeltaXQ8_8
        );
        assert(
            results[0].cartDeltaYQ8_8 ==
            results[2].cartDeltaYQ8_8
        );
        assert(carts[0].worldXQ8_8() == carts[1].worldXQ8_8());
        assert(carts[1].worldXQ8_8() == carts[2].worldXQ8_8());
        assert(!results[0].cartSteeredByRider);
        assert(!results[1].cartSteeredByRider);
        assert(!results[2].cartSteeredByRider);
    }
    assert(carts[0].worldYQ8_8() == carts[2].worldYQ8_8());

    const auto leftIntent = carts[0].tick(left, {});
    const auto rightIntent = carts[1].tick(right, {});
    const auto jumpIntent = carts[2].tick(jump, {});
    assert(leftIntent.riderHorizontalIntent == -1);
    assert(rightIntent.riderHorizontalIntent == 1);
    assert(jumpIntent.riderHorizontalIntent == 0);
    assert(jumpIntent.riderJumpRequested);
}

void testLifecycleClosureIsCauseNeutralNotDestruction() {
    mmx::MineCartModel cart;
    cart.reset(0, 0);
    assert(cart.wake());
    assert(cart.beginMotion(mmx::MineCartDirection::Right));
    assert(mmx::MineCartModel::kObservedCullRelativeXMin == 323);
    assert(mmx::MineCartModel::kObservedCullRelativeXMax == 325);
    assert(cart.closeOffscreen());
    assert(!cart.active());
    assert(cart.phase() == mmx::MineCartPhase::Closed);
    assert(
        cart.closureCause() == mmx::MineCartClosureCause::Offscreen
    );
    assert(cart.horizontalVelocityQ8_8() == 0);
    assert(!cart.closeUnknown());
    assert(cart.tick({}, {}).cartDeltaXQ8_8 == 0);

    cart.reset(0, 0);
    assert(cart.closeUnknown());
    assert(
        cart.closureCause() == mmx::MineCartClosureCause::Unknown
    );
}

} // namespace

int main() {
    testLifecycleAndMeasuredAcceleration();
    testAccelerationCadenceRemainsAnAdapterDecision();
    testRiderControlsDoNotSteerTheCart();
    testLifecycleClosureIsCauseNeutralNotDestruction();
    std::printf("mine-cart model contract: OK\n");
    return 0;
}
