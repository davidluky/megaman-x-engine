// kb_paths.cpp - resolves development knowledge-base paths for runtime loaders.
// Boundary: keeps KB-relative paths inside the knowledge_base root.

#include "data/kb_paths.h"
#include "data/path_utils.h"

#include <cstdio>
#include <filesystem>
#include <cctype>
#include <vector>

namespace fs = std::filesystem;
using mmx::path_utils::normalizedPath;
using mmx::path_utils::pathIsWithin;
using mmx::path_utils::withForwardSlashes;

namespace mmx::kb_paths {
namespace {

std::vector<fs::path> rootCandidates() {
    // Dev/test runs usually execute from build/, where ../knowledge_base is
    // the source tree. Packaged runs execute beside their own knowledge_base/.
    return {fs::path("..") / "knowledge_base", fs::path("knowledge_base")};
}

} // namespace

bool isSafeIdSegment(const std::string& value) {
    if (value.empty() || value == "." || value == "..") {
        return false;
    }

    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_') {
            continue;
        }
        return false;
    }
    return true;
}

std::optional<std::string> resolveKBPath(const std::string& relativePath) {
    if (relativePath.empty()) {
        return std::nullopt;
    }

    const fs::path raw(relativePath);
    if (raw.is_absolute() || raw.has_root_name() || raw.has_root_directory()) {
        fprintf(stderr, "kb_paths: rejecting absolute KB path: %s\n", relativePath.c_str());
        return std::nullopt;
    }

    std::optional<fs::path> fallback;
    for (const auto& rootCandidate : rootCandidates()) {
        const fs::path root = normalizedPath(rootCandidate);
        std::error_code ec;
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
            continue;
        }

        const fs::path candidate = normalizedPath(rootCandidate / raw);
        if (!pathIsWithin(root, candidate)) {
            fprintf(stderr, "kb_paths: rejecting KB path outside root: %s\n", relativePath.c_str());
            return std::nullopt;
        }

        if (!fallback.has_value()) {
            fallback = rootCandidate / raw;
        }
        if (fs::exists(candidate, ec)) {
            return withForwardSlashes((rootCandidate / raw).lexically_normal());
        }
    }

    if (fallback.has_value()) {
        return withForwardSlashes(fallback->lexically_normal());
    }

    return withForwardSlashes((fs::path("knowledge_base") / raw).lexically_normal());
}

} // namespace mmx::kb_paths
