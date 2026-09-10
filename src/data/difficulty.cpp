// difficulty.cpp - stores the process-wide difficulty selection.
// Boundary: gameplay reads multipliers through DifficultySettings helpers.

#include "data/difficulty.h"

namespace mmx {

Difficulty DifficultySettings::current = Difficulty::Normal;

} // namespace mmx
