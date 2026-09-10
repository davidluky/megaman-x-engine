// cp_source_obj_foreground.cpp - serves source OBJ foreground overlay records.
// Boundary: diagnostic overlays only; shipped stage content stays data-owned.

#include "gameplay/cp_source_obj_foreground.h"

#include <cstring>

namespace mmx {
namespace {

#include "generated/cp_source_obj_foreground/cp_f1091_source_obj_foreground.inc"

} // namespace

const CpSourceObjForegroundRecord* cpSourceObjForegroundRecordForTarget(const char* target) {
    if (!target || *target == '\0') return nullptr;
    for (const auto& record : kCpSourceObjForegroundRecords) {
        if (std::strcmp(record.target, target) == 0) {
            return &record;
        }
    }
    return nullptr;
}

const CpSourceObjForegroundRecord* cpSourceObjForegroundRecordForFrame(const char* target,
                                                                        int engineFrame) {
    const auto* record = cpSourceObjForegroundRecordForTarget(target);
    if (!record || record->engineFrame != engineFrame) return nullptr;
    return record;
}

const char* cpSourceObjForegroundAtlasPath(int atlasPage) {
    if (atlasPage < 0) return "";
    const int count = static_cast<int>(sizeof(kCpSourceObjForegroundAtlasPages) /
                                       sizeof(kCpSourceObjForegroundAtlasPages[0]));
    if (atlasPage >= count) return "";
    return kCpSourceObjForegroundAtlasPages[atlasPage];
}

} // namespace mmx
