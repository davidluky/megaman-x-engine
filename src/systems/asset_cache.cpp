// asset_cache.cpp - caches shared texture resources by normalized path.
// Owns: process-local texture cache lifetime and path normalization.

#include "systems/asset_cache.h"
#include <algorithm>
#include <unordered_map>

namespace mmx {
namespace AssetCache {

namespace {

std::unordered_map<std::string, TextureResource> textureCache;

std::string normalizePath(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

} // namespace

const TextureResource* loadTexture(const std::string& path) {
    const std::string key = normalizePath(path);
    auto it = textureCache.find(key);
    if (it != textureCache.end()) {
        return it->second.valid() ? &it->second : nullptr;
    }

    TextureResource texture;
    if (!texture.load(key)) {
        return nullptr;
    }

    auto inserted = textureCache.emplace(key, std::move(texture));
    return &inserted.first->second;
}

void clear() {
    textureCache.clear();
}

} // namespace AssetCache
} // namespace mmx
