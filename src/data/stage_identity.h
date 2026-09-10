// stage_identity.h - declares helpers for canonical stage ids and labels.
// Boundary: display names are derived from ids, not localized UI strings.

#pragma once

#include "data/game_ids.h"
#include <string>

namespace mmx {
namespace stage_identity {

// Returns the canonical stage slot id for known engine stage paths.
// Examples:
//   content/x1/stages/ripped/chill-penguin/stage_final.json -> chill-penguin
//   content/x1/stages/intro-highway.json                    -> intro-highway
StageId fromPath(const std::string& stagePath);

// Converts a canonical id to the compact in-game display label.
std::string displayName(StageId stageId);

} // namespace stage_identity
} // namespace mmx
