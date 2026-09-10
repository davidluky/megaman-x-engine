// gameplay_warp_in_assets.h - declares warp-in visual source assets.
// Boundary: asset lookup only; playback timing stays in gameplay_warp_in.

#pragma once

#include "gameplay/gameplay_warp_in.h"
#include "systems/asset_cache.h"

namespace mmx::gameplay_warp_in {

inline void loadAssets(State& state) {
    state.capsuleTexture = AssetCache::loadTexture("content/x1/sprites/x_warp_capsule.png");
    state.burstTexture = AssetCache::loadTexture("content/x1/sprites/x_warp_burst.png");
}

} // namespace mmx::gameplay_warp_in
