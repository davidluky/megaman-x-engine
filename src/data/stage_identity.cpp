// stage_identity.cpp - maps stage paths and ids to canonical stage labels.
// Boundary: infers identity from paths only; stage content stays elsewhere.

#include "data/stage_identity.h"
#include <algorithm>
#include <cctype>

namespace mmx {
namespace stage_identity {

StageId fromPath(const std::string& stagePath) {
    std::string normalized = stagePath;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    if (normalized.empty()) return {};

    const auto lastSlash = normalized.find_last_of('/');
    const std::string filename = (lastSlash == std::string::npos)
        ? normalized
        : normalized.substr(lastSlash + 1);

    if ((filename == "stage_final.json" || filename == "stage.json") &&
        lastSlash != std::string::npos) {
        const std::string parent = normalized.substr(0, lastSlash);
        const auto prevSlash = parent.find_last_of('/');
        return StageId::fromString(
            (prevSlash == std::string::npos) ? parent : parent.substr(prevSlash + 1)
        );
    }

    const auto dot = filename.find_last_of('.');
    return StageId::fromString(
        (dot == std::string::npos) ? filename : filename.substr(0, dot)
    );
}

std::string displayName(StageId stageId) {
    if (stageId.empty()) return "UNKNOWN STAGE";

    std::string name = stageId.str();
    for (char& c : name) {
        if (c == '-' || c == '_') {
            c = ' ';
        } else {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    return name;
}

} // namespace stage_identity
} // namespace mmx
