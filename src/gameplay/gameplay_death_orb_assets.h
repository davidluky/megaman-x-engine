// gameplay_death_orb_assets.h - declares death-orb visual asset handles.
// Boundary: asset handles only; burst timing lives in the death-orb lane.

#pragma once

#include "gameplay/gameplay_death_orbs.h"

#include <string>

namespace mmx::gameplay_death_orbs {

void loadSpriteMetadata(SpriteMetadata& meta, const std::string& metaPath);
void loadSpriteAssets(const TextureResource*& sheet, SpriteMetadata& meta);
void loadSpriteAssets(State& state);

} // namespace mmx::gameplay_death_orbs
