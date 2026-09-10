// cp_source_obj_foreground.h - declares source OBJ foreground diagnostics.
// Boundary: exposes proven overlay records without promoting content.

#pragma once

namespace mmx {

struct CpSourceObjForegroundRecord {
    const char* target = "";
    const char* stageId = "";
    int sourceFrame = 0;
    int engineFrame = 0;
    int enginePhase = 0;
    const char* activeCameraSection = "";
    const char* activeVisualSection = "";
    int atlasPage = 0;
    int atlasX = 0;
    int atlasY = 0;
    int width = 0;
    int height = 0;
    int screenX = 0;
    int screenY = 0;
    int sourceObjAlphaPx = 0;
    int framepackSourceForegroundPx = 0;
    int framepackObjOverlapPx = 0;
    int framepackNoObjResidualPx = 0;
    bool hideRuntimePlayer = false;
    const char* evidencePath = "";
};

const CpSourceObjForegroundRecord* cpSourceObjForegroundRecordForTarget(const char* target);
const CpSourceObjForegroundRecord* cpSourceObjForegroundRecordForFrame(const char* target,
                                                                        int engineFrame);
const char* cpSourceObjForegroundAtlasPath(int atlasPage);

} // namespace mmx
