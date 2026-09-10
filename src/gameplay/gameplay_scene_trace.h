// gameplay_scene_trace.h - small JSON writers used by gameplay trace output.
// Boundary: serialization only; it does not own gameplay state or promotion.

#pragma once

#include "gameplay/cp_source_obj_foreground.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace mmx::gameplay_scene_trace {

std::string jsonQuoted(std::string_view value);
void writeStringArray(FILE* file, const std::vector<std::string>& values);
void writeSourceObjForegroundTrace(
    FILE* file, const CpSourceObjForegroundRecord* record);
void writeIntArray(FILE* file, const std::vector<int>& values);

} // namespace mmx::gameplay_scene_trace
