// logging.h - declares gameplay-scoped logging entry points.
// Boundary: lightweight diagnostics only; callers own gameplay decisions.

#pragma once

namespace mmx::logging {

// Installs the release rotating-file trace log sink.
//
// PX6A: pass the caller's hidden-automation flag. Hidden runs skip the sink so
// raylib's default stdout logging survives and CTest/subprocess capture can
// still observe automation completion markers.
void installReleaseTraceLog(bool hiddenAutomation);

} // namespace mmx::logging
