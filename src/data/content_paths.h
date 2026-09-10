// content_paths.h - exposes the content-root path jail for runtime assets.
// Boundary: callers pass authored paths or safe stems, never trusted filenames.

#pragma once

#include <optional>
#include <string>

namespace mmx::content_paths {

// Resolve a JSON-authored runtime asset path. The returned path is normalized
// for raylib and guaranteed to remain under the runtime content/ tree.
std::optional<std::string> resolveAssetPath(const std::string& path);

// Resolve engine-owned MMX1 content paths from paths relative to content/x1/.
// These helpers keep hardcoded runtime lookups behind the same content jail as
// JSON-authored asset paths.
std::optional<std::string> resolveX1Path(const std::string& relativePath);
std::optional<std::string> x1SpritePath(const std::string& relativePath);
std::optional<std::string> x1PalettePath(const std::string& relativePath);
std::optional<std::string> x1AudioBgmManifestPath();
std::optional<std::string> x1BgmTrackPath(const std::string& stem,
                                          const std::string& extension);
std::optional<std::string> x1ApuSfxPath(int commandByte);
std::optional<std::string> x1SfxPath(const std::string& stem);

// Developer-only extension point used by --stage-file for build candidates.
// Normal content loads should not call this; they stay restricted to content/.
// Extra roots accept repo-relative paths already under the root and paths
// relative to the root itself; absolute paths and root escapes stay rejected.
void allowAdditionalAssetRoot(const std::string& root);
void clearAdditionalAssetRoots();

// BGM/SFX entries are track stems, not paths. Keep them narrow so callers
// cannot smuggle directory separators into audio path construction.
bool isSafeStem(const std::string& stem);

} // namespace mmx::content_paths
