// content_paths.cpp - resolves runtime-authored content and audio asset paths.
// Boundary: rejects absolute paths and escapes outside approved content roots.

#include "data/content_paths.h"
#include "data/path_utils.h"

#include "raylib.h"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;
using mmx::path_utils::normalizedPath;
using mmx::path_utils::pathIsWithin;
using mmx::path_utils::withForwardSlashes;

namespace mmx::content_paths {
namespace {

std::vector<fs::path>& additionalAssetRoots() {
    static std::vector<fs::path> roots;
    return roots;
}

bool isReservedRepoRoot(const fs::path& raw) {
    if (raw.empty()) return false;
    const auto first = raw.begin();
    if (first == raw.end()) return false;
    const std::string head = first->generic_string();
    return head == "content" || head == "docs" || head == "src" ||
           head == "tests" || head == "tmp" || head == "tools";
}

std::optional<std::string> resolveX1SubPath(const std::string& subdir,
                                            const std::string& relativePath) {
    if (relativePath.empty()) {
        return std::nullopt;
    }

    const fs::path raw(relativePath);
    if (raw.is_absolute() || raw.has_root_name() || raw.has_root_directory()) {
        TraceLog(LOG_WARNING, "ContentPaths: rejecting absolute x1 subpath: %s", relativePath.c_str());
        return std::nullopt;
    }

    const fs::path subRoot = normalizedPath(fs::path("content/x1") / subdir);
    const fs::path candidate = normalizedPath(subRoot / raw);
    if (!pathIsWithin(subRoot, candidate)) {
        TraceLog(LOG_WARNING, "ContentPaths: rejecting x1 subpath escape: %s/%s",
                 subdir.c_str(), relativePath.c_str());
        return std::nullopt;
    }
    const fs::path authored = (fs::path("content/x1") / subdir / raw).lexically_normal();
    return resolveAssetPath(withForwardSlashes(authored));
}

} // namespace

std::optional<std::string> resolveAssetPath(const std::string& path) {
    if (path.empty()) {
        return std::nullopt;
    }

    const fs::path raw(path);
    if (raw.is_absolute() || raw.has_root_name() || raw.has_root_directory()) {
        TraceLog(LOG_WARNING, "ContentPaths: rejecting absolute asset path: %s", path.c_str());
        return std::nullopt;
    }

    const fs::path root = normalizedPath("content");
    const fs::path candidate = normalizedPath(raw);
    if (!pathIsWithin(root, candidate)) {
        for (const auto& extraRoot : additionalAssetRoots()) {
            if (pathIsWithin(extraRoot, candidate)) {
                return withForwardSlashes(candidate);
            }

            if (!isReservedRepoRoot(raw)) {
                const fs::path rootRelativeCandidate = normalizedPath(extraRoot / raw);
                if (pathIsWithin(extraRoot, rootRelativeCandidate)) {
                    return withForwardSlashes(rootRelativeCandidate);
                }
            }
        }
        TraceLog(LOG_WARNING, "ContentPaths: rejecting asset path outside content root: %s", path.c_str());
        return std::nullopt;
    }

    return raw.lexically_normal().generic_string();
}

std::optional<std::string> resolveX1Path(const std::string& relativePath) {
    if (relativePath.empty()) {
        return std::nullopt;
    }

    const fs::path raw(relativePath);
    if (raw.is_absolute() || raw.has_root_name() || raw.has_root_directory()) {
        TraceLog(LOG_WARNING, "ContentPaths: rejecting absolute x1 path: %s", relativePath.c_str());
        return std::nullopt;
    }

    const fs::path x1Root = normalizedPath("content/x1");
    const fs::path candidate = normalizedPath(x1Root / raw);
    if (!pathIsWithin(x1Root, candidate)) {
        TraceLog(LOG_WARNING, "ContentPaths: rejecting x1 path outside content/x1: %s", relativePath.c_str());
        return std::nullopt;
    }
    const fs::path authored = (fs::path("content/x1") / raw).lexically_normal();
    return resolveAssetPath(withForwardSlashes(authored));
}

std::optional<std::string> x1SpritePath(const std::string& relativePath) {
    return resolveX1SubPath("sprites", relativePath);
}

std::optional<std::string> x1PalettePath(const std::string& relativePath) {
    return resolveX1SubPath("palettes", relativePath);
}

std::optional<std::string> x1AudioBgmManifestPath() {
    return resolveX1SubPath("audio", "bgm.json");
}

std::optional<std::string> x1BgmTrackPath(const std::string& stem,
                                          const std::string& extension) {
    if (!isSafeStem(stem)) {
        return std::nullopt;
    }
    if (extension != ".ogg" && extension != ".wav") {
        TraceLog(LOG_WARNING, "ContentPaths: rejecting unsupported BGM extension: %s", extension.c_str());
        return std::nullopt;
    }
    return resolveX1SubPath("audio/bgm", stem + extension);
}

std::optional<std::string> x1ApuSfxPath(int commandByte) {
    if (commandByte < 0 || commandByte > 0xff) {
        return std::nullopt;
    }

    char name[32];
    std::snprintf(name, sizeof(name), "apu_%02x", commandByte);
    return x1SfxPath(name);
}

std::optional<std::string> x1SfxPath(const std::string& stem) {
    if (!isSafeStem(stem)) {
        return std::nullopt;
    }
    return resolveX1SubPath("audio/sfx", stem + ".wav");
}

void allowAdditionalAssetRoot(const std::string& root) {
    if (root.empty()) {
        return;
    }
    additionalAssetRoots().push_back(normalizedPath(root));
}

void clearAdditionalAssetRoots() {
    additionalAssetRoots().clear();
}

bool isSafeStem(const std::string& stem) {
    if (stem.empty() || stem.size() > 96) {
        return false;
    }
    for (unsigned char c : stem) {
        if (!std::isalnum(c) && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

} // namespace mmx::content_paths
