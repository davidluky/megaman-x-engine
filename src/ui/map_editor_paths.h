// map_editor_paths.h - user-map path policy for the map editor.

#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <string>

namespace mmx::map_editor {

inline bool isReservedWindowsMapSlug(const std::string& slug) {
    std::string upper;
    upper.reserve(slug.size());
    for (const unsigned char ch : slug) {
        upper.push_back(static_cast<char>(std::toupper(ch)));
    }
    if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL") {
        return true;
    }
    return upper.size() == 4 &&
           (upper.rfind("COM", 0) == 0 || upper.rfind("LPT", 0) == 0) &&
           upper[3] >= '1' && upper[3] <= '9';
}

inline std::string slugifyMapName(const std::string& name) {
    std::string slug;
    bool pendingSeparator = false;
    for (const unsigned char ch : name) {
        if (std::isalnum(ch)) {
            if (pendingSeparator && !slug.empty()) {
                slug.push_back('-');
            }
            slug.push_back(static_cast<char>(std::tolower(ch)));
            pendingSeparator = false;
        } else if (!slug.empty()) {
            pendingSeparator = true;
        }
    }

    if (slug.size() > 64) {
        slug.resize(64);
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    if (slug.empty()) {
        slug = "untitled-stage";
    }
    if (isReservedWindowsMapSlug(slug)) {
        slug = "map-" + slug;
    }
    return slug;
}

inline std::string displayMapNameOrDefault(const std::string& name) {
    std::size_t first = 0;
    while (first < name.size() && std::isspace(static_cast<unsigned char>(name[first]))) {
        ++first;
    }

    std::size_t last = name.size();
    while (last > first && std::isspace(static_cast<unsigned char>(name[last - 1]))) {
        --last;
    }

    if (first == last) {
        return "Untitled Stage";
    }
    return name.substr(first, last - first);
}

inline std::filesystem::path userMapRoot() {
    return std::filesystem::path("saves") / "maps";
}

inline std::filesystem::path userMapPathForName(const std::string& name) {
    return userMapRoot() / (slugifyMapName(name) + ".json");
}

inline std::filesystem::path defaultUserMapPath() {
    return userMapPathForName("Untitled Stage");
}

inline std::filesystem::path editorTestMapPath() {
    return userMapRoot() / "editor-test.json";
}

inline std::filesystem::path legacySingleSlotPath() {
    return std::filesystem::path("content") / "x1" / "stages" / "custom" / "stage_custom.json";
}

} // namespace mmx::map_editor
