// screen_transform_contract_test.cpp - contract for window->internal mapping.
//
// Proves (against the declared contract in src/app/screen_transform.h, with
// internal resolution taken from the in-repo src/app/constants.h
// INTERNAL_WIDTH=256, INTERNAL_HEIGHT=224):
//   1. invalid window dimensions (zero/negative) yield an empty viewport and
//      a not-inside point without opening a window;
//   2. displayAspect is exactly 4:3 (aspect43=true) or the native 256:224
//      ratio (aspect43=false);
//   3. windows matching the target aspect map the full window with zero
//      margins (640x480 under 4:3, 1024x896 under the native ratio);
//   4. windows wider than the target pillarbox: height fills, width is
//      centered, and margin points map outside the internal framebuffer;
//   5. windows taller than the target letterbox: width fills, height is
//      centered, and margin points map outside the internal framebuffer;
//   6. a native-resolution window (256x224) maps identity with half-open
//      bounds [0, W) x [0, H);
//   7. the two aspect targets differ on the same window: 640x480 pillarboxes
//      under the native ratio even though it is an exact 4:3 window;
//   8. interior points scale linearly into the internal framebuffer.
//
// Does NOT prove: any original-game fidelity, input hardware behavior, or
// raylib rendering. Pure math over constants; raylib-free by construction.

#include "app/screen_transform.h"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

constexpr bool kClose(float a, float b, float epsilon = 1e-4f) {
    const float diff = a - b;
    return diff <= epsilon && diff >= -epsilon;
}

void testInvalidWindowDimensionsYieldEmptyViewportAndNoInsidePoint() {
    const mmx::screen_transform::InternalViewport zero =
        mmx::screen_transform::internalViewportForWindow(0.0f, 0.0f, true);
    assert(zero.width == 0.0f);
    assert(zero.height == 0.0f);

    const mmx::screen_transform::InternalViewport negative =
        mmx::screen_transform::internalViewportForWindow(
            -320.0f, -240.0f, false);
    assert(negative.width == 0.0f);
    assert(negative.height == 0.0f);

    const mmx::screen_transform::InternalPoint rejected =
        mmx::screen_transform::screenToInternal(128.0f, 112.0f, 0.0f, 0.0f, true);
    assert(!rejected.inside);
    assert(rejected.x == -1.0f);
    assert(rejected.y == -1.0f);
}

void testDisplayAspectMatchesDeclaredTargets() {
    assert(kClose(mmx::screen_transform::displayAspect(true), 4.0f / 3.0f));
    assert(kClose(
        mmx::screen_transform::displayAspect(false),
        static_cast<float>(mmx::INTERNAL_WIDTH) /
            static_cast<float>(mmx::INTERNAL_HEIGHT)));
}

void testExactAspectWindowsFillWithNoMargins() {
    // 640x480 is exactly 4:3, so no margins under the 4:3 target.
    const mmx::screen_transform::InternalViewport wide =
        mmx::screen_transform::internalViewportForWindow(640.0f, 480.0f, true);
    assert(kClose(wide.x, 0.0f));
    assert(kClose(wide.y, 0.0f));
    assert(kClose(wide.width, 640.0f));
    assert(kClose(wide.height, 480.0f));

    // 1024x896 is exactly 256:224, so no margins under the native target.
    const mmx::screen_transform::InternalViewport native =
        mmx::screen_transform::internalViewportForWindow(1024.0f, 896.0f, false);
    assert(kClose(native.x, 0.0f));
    assert(kClose(native.y, 0.0f));
    assert(kClose(native.width, 1024.0f));
    assert(kClose(native.height, 896.0f));
}

void testWideWindowsPillarboxWithEqualHorizontalMargins() {
    // 800x480 is 5:3 (> 4:3): height fills, width = 480*4/3 = 640,
    // centered => (800-640)/2 = 80 px pillarbox on each side.
    const mmx::screen_transform::InternalViewport pillar =
        mmx::screen_transform::internalViewportForWindow(800.0f, 480.0f, true);
    assert(kClose(pillar.x, 80.0f));
    assert(kClose(pillar.y, 0.0f));
    assert(kClose(pillar.width, 640.0f));
    assert(kClose(pillar.height, 480.0f));

    // A point one pixel left of the viewport maps just outside; the
    // coordinate stays meaningful (how far outside) while inside is false.
    const mmx::screen_transform::InternalPoint leftMargin =
        mmx::screen_transform::screenToInternal(79.0f, 240.0f, 800.0f, 480.0f, true);
    assert(!leftMargin.inside);
    assert(kClose(leftMargin.x, -0.4f)); // (79 - 80) * 256 / 640
    assert(kClose(leftMargin.y, 112.0f));

    // The half-open right edge maps to exactly the framebuffer width.
    const mmx::screen_transform::InternalPoint rightEdge =
        mmx::screen_transform::screenToInternal(720.0f, 240.0f, 800.0f, 480.0f, true);
    assert(!rightEdge.inside);
    assert(kClose(rightEdge.x, 256.0f));

    // First inside column on the right: x=719 -> 255.6.
    const mmx::screen_transform::InternalPoint firstInside =
        mmx::screen_transform::screenToInternal(719.0f, 240.0f, 800.0f, 480.0f, true);
    assert(firstInside.inside);
    assert(kClose(firstInside.x, 255.6f));

    // 640x400 has aspect 1.6 (> 4:3): pillarbox, height fills,
    // width = 400*4/3 = 533.3333, centered => (640-533.3333)/2 = 53.3333.
    const mmx::screen_transform::InternalViewport shorter =
        mmx::screen_transform::internalViewportForWindow(640.0f, 400.0f, true);
    assert(kClose(shorter.x, 53.3333f));
    assert(kClose(shorter.y, 0.0f));
    assert(kClose(shorter.width, 533.3333f));
    assert(kClose(shorter.height, 400.0f));
}

void testTallWindowsLetterboxWithEqualVerticalMargins() {
    // 640x600 has aspect 1.0667 (< 4:3): letterbox, width fills,
    // height = 640*3/4 = 480, centered => (600-480)/2 = 60 px top offset.
    const mmx::screen_transform::InternalViewport letter =
        mmx::screen_transform::internalViewportForWindow(640.0f, 600.0f, true);
    assert(kClose(letter.x, 0.0f));
    assert(kClose(letter.y, 60.0f));
    assert(kClose(letter.width, 640.0f));
    assert(kClose(letter.height, 480.0f));

    // 640x800, 4:3 target: y margins (800-480)/2 = 160 on top and bottom.
    const mmx::screen_transform::InternalViewport tall =
        mmx::screen_transform::internalViewportForWindow(640.0f, 800.0f, true);
    assert(kClose(tall.x, 0.0f));
    assert(kClose(tall.y, 160.0f));
    assert(kClose(tall.width, 640.0f));
    assert(kClose(tall.height, 480.0f));

    // A point one row above the viewport maps outside.
    const mmx::screen_transform::InternalPoint topMargin =
        mmx::screen_transform::screenToInternal(320.0f, 159.0f, 640.0f, 800.0f, true);
    assert(!topMargin.inside);

    // First inside row: y=161 -> (161-160)*224/480 = 0.4666667.
    const mmx::screen_transform::InternalPoint firstInside =
        mmx::screen_transform::screenToInternal(320.0f, 161.0f, 640.0f, 800.0f, true);
    assert(firstInside.inside);
    assert(kClose(firstInside.x, 128.0f));
    assert(kClose(firstInside.y, 0.4666667f));

    // The half-open bottom edge maps to exactly the framebuffer height.
    const mmx::screen_transform::InternalPoint bottomEdge =
        mmx::screen_transform::screenToInternal(320.0f, 640.0f, 640.0f, 800.0f, true);
    assert(!bottomEdge.inside);
    assert(kClose(bottomEdge.y, 224.0f));
}

void testNativeWindowMapsIdentityWithHalfOpenBounds() {
    const mmx::screen_transform::InternalViewport native =
        mmx::screen_transform::internalViewportForWindow(256.0f, 224.0f, false);
    assert(kClose(native.x, 0.0f));
    assert(kClose(native.y, 0.0f));
    assert(kClose(native.width, 256.0f));
    assert(kClose(native.height, 224.0f));

    const mmx::screen_transform::InternalPoint origin =
        mmx::screen_transform::screenToInternal(0.0f, 0.0f, 256.0f, 224.0f, false);
    assert(origin.inside);
    assert(kClose(origin.x, 0.0f));
    assert(kClose(origin.y, 0.0f));

    // Top-left inclusive, bottom-right exclusive ([0, W) x [0, H)).
    const mmx::screen_transform::InternalPoint lastPixel =
        mmx::screen_transform::screenToInternal(255.0f, 223.0f, 256.0f, 224.0f, false);
    assert(lastPixel.inside);
    assert(kClose(lastPixel.x, 255.0f));
    assert(kClose(lastPixel.y, 223.0f));

    const mmx::screen_transform::InternalPoint rightEdge =
        mmx::screen_transform::screenToInternal(256.0f, 223.0f, 256.0f, 224.0f, false);
    assert(!rightEdge.inside);
    assert(kClose(rightEdge.x, 256.0f));

    const mmx::screen_transform::InternalPoint bottomEdge =
        mmx::screen_transform::screenToInternal(255.0f, 224.0f, 256.0f, 224.0f, false);
    assert(!bottomEdge.inside);
    assert(kClose(bottomEdge.y, 224.0f));
}

void testAspectTargetsDifferOnTheSameWindow() {
    // 640x480 is exactly 4:3 but WIDER than the native 256:224 ratio, so the
    // native target pillarboxes it: width = 480*256/224 = 548.5714,
    // centered => (640-548.5714)/2 = 45.7143.
    const mmx::screen_transform::InternalViewport nativeOn43 =
        mmx::screen_transform::internalViewportForWindow(640.0f, 480.0f, false);
    assert(kClose(nativeOn43.x, 45.7143f));
    assert(kClose(nativeOn43.y, 0.0f));
    assert(kClose(nativeOn43.width, 548.5714f));
    assert(kClose(nativeOn43.height, 480.0f));

    // Same window under 4:3 fills completely (see
    // testExactAspectWindowsFillWithNoMargins), so the two targets disagree.
    const mmx::screen_transform::InternalViewport aspect43OnSame =
        mmx::screen_transform::internalViewportForWindow(640.0f, 480.0f, true);
    assert(!kClose(nativeOn43.width, aspect43OnSame.width));

    // 640x400 under the native ratio: width = 400*256/224 = 457.1429,
    // centered => (640-457.1429)/2 = 91.4286.
    const mmx::screen_transform::InternalViewport nativeOnWide =
        mmx::screen_transform::internalViewportForWindow(640.0f, 400.0f, false);
    assert(kClose(nativeOnWide.x, 91.4286f));
    assert(kClose(nativeOnWide.width, 457.1429f));
}

void testInteriorPointsScaleLinearly() {
    // Window center maps to framebuffer center under 4:3 in 800x480.
    const mmx::screen_transform::InternalPoint center =
        mmx::screen_transform::screenToInternal(400.0f, 240.0f, 800.0f, 480.0f, true);
    assert(center.inside);
    assert(kClose(center.x, 128.0f));
    assert(kClose(center.y, 112.0f));

    // Quarter-window point maps to quarter-internal point at native scale.
    const mmx::screen_transform::InternalPoint quarter =
        mmx::screen_transform::screenToInternal(256.0f, 224.0f, 1024.0f, 896.0f, false);
    assert(quarter.inside);
    assert(kClose(quarter.x, 64.0f));
    assert(kClose(quarter.y, 56.0f));
}

} // namespace

int main() {
    testInvalidWindowDimensionsYieldEmptyViewportAndNoInsidePoint();
    testDisplayAspectMatchesDeclaredTargets();
    testExactAspectWindowsFillWithNoMargins();
    testWideWindowsPillarboxWithEqualHorizontalMargins();
    testTallWindowsLetterboxWithEqualVerticalMargins();
    testNativeWindowMapsIdentityWithHalfOpenBounds();
    testAspectTargetsDifferOnTheSameWindow();
    testInteriorPointsScaleLinearly();
    std::printf("screen transform contract: OK\n");
    return 0;
}