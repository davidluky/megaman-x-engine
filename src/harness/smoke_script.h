// smoke_script.h - declares smoke-test frame input dispatch.
// Boundary: smoke scripts exercise surfaces; they do not assert results.

#pragma once

#include <string_view>

namespace mmx::smoke_script {

void runFrame(std::string_view id, std::string_view profile, int frame);

} // namespace mmx::smoke_script
