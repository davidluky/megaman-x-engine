// kb_paths.h - exposes safe knowledge-base path and id-segment helpers.
// Boundary: KB callers provide relative paths or path segments only.

#pragma once

#include <optional>
#include <string>

namespace mmx::kb_paths {

// KB entity ids are path segments, not paths. Keep this helper close to the
// path jail so all loaders share the same segment contract.
bool isSafeIdSegment(const std::string& value);

// Resolve a KB-relative path (e.g. "mmx1/bosses/chill-penguin/boss.json")
// into a normalized path under the knowledge_base/ root. Rejects absolute
// paths, traversal attempts, and anything that escapes the jail.
std::optional<std::string> resolveKBPath(const std::string& relativePath);

} // namespace mmx::kb_paths
