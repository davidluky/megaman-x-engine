// asset_cache.h - declares shared asset cache entry points.
// Owns: texture cache load/clear API for reusable runtime assets.

#pragma once

#include "systems/raylib_resource.h"
#include <string>

namespace mmx {
namespace AssetCache {

// Loads once per normalized path and returns a borrowed pointer to the cached
// owner. The pointer remains valid until AssetCache::clear().
const TextureResource* loadTexture(const std::string& path);

// Releases cached resources before the raylib window/context closes.
void clear();

} // namespace AssetCache
} // namespace mmx
