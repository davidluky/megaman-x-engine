// autotest_script.h - declares the frame runner for deterministic autotests.
// Boundary: scripts emit input events through core Input only.

#pragma once

#include <functional>
#include <string_view>

namespace mmx::autotest_script {

using SnapFn = std::function<void(int)>;

void runFrame(std::string_view profile, int frame, const SnapFn& snap);

} // namespace mmx::autotest_script
