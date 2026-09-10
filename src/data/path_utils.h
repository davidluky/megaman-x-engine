// path_utils.h - provides small filesystem normalization and containment helpers.
// Boundary: shared by path jails; it does not decide which roots are allowed.

#pragma once

#include <filesystem>
#include <system_error>

namespace mmx::path_utils {

inline std::filesystem::path normalizedPath(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::path normalized = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        normalized = std::filesystem::absolute(path, ec);
        if (ec) {
            normalized = path;
        }
        normalized = normalized.lexically_normal();
    }
    return normalized;
}

inline bool pathIsWithin(const std::filesystem::path& root,
                         const std::filesystem::path& candidate) {
    const auto normalizedRoot = normalizedPath(root);
    const auto normalizedCandidate = normalizedPath(candidate);

    auto rootIt = normalizedRoot.begin();
    auto candidateIt = normalizedCandidate.begin();
    for (; rootIt != normalizedRoot.end(); ++rootIt, ++candidateIt) {
        if (candidateIt == normalizedCandidate.end() || *candidateIt != *rootIt) {
            return false;
        }
    }
    return true;
}

inline std::string withForwardSlashes(const std::filesystem::path& path) {
    return path.generic_string();
}

} // namespace mmx::path_utils
