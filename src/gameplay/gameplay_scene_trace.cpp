// gameplay_scene_trace.cpp - JSON serialization helpers for gameplay traces.

#include "gameplay/gameplay_scene.h"
#include "gameplay/gameplay_scene_trace.h"

#include <cstdio>

namespace mmx::gameplay_scene_trace {

std::string jsonQuoted(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (unsigned char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buffer[7];
                std::snprintf(
                    buffer, sizeof(buffer), "\\u%04x",
                    static_cast<unsigned int>(c));
                out += buffer;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    out.push_back('"');
    return out;
}

void writeStringArray(FILE* file, const std::vector<std::string>& values) {
    std::fprintf(file, "[");
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) std::fprintf(file, ",");
        const std::string quoted = jsonQuoted(values[i]);
        std::fprintf(file, "%s", quoted.c_str());
    }
    std::fprintf(file, "]");
}

void writeSourceObjForegroundTrace(
    FILE* file, const CpSourceObjForegroundRecord* record) {
    if (!record) {
        std::fprintf(file, "{\"active\":false}");
        return;
    }

    const std::string target = jsonQuoted(record->target);
    const std::string stageId = jsonQuoted(record->stageId);
    const std::string cameraSection = jsonQuoted(record->activeCameraSection);
    const std::string visualSection = jsonQuoted(record->activeVisualSection);
    const std::string evidencePath = jsonQuoted(record->evidencePath);
    std::fprintf(
        file,
        "{\"active\":true,\"target\":%s,\"stageId\":%s,"
        "\"sourceFrame\":%d,\"engineFrame\":%d,\"enginePhase\":%d,"
        "\"activeCameraSection\":%s,\"activeVisualSection\":%s,"
        "\"coordinateBasis\":\"source-native-screen-pixels\","
        "\"atlas\":{\"page\":%d,\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d},"
        "\"screenRect\":{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d},"
        "\"sourceObjAlphaPx\":%d,\"framepackSourceForegroundPx\":%d,"
        "\"framepackObjOverlapPx\":%d,\"framepackNoObjResidualPx\":%d,"
        "\"hideRuntimePlayer\":%s,\"evidencePath\":%s}",
        target.c_str(), stageId.c_str(),
        record->sourceFrame, record->engineFrame, record->enginePhase,
        cameraSection.c_str(), visualSection.c_str(),
        record->atlasPage, record->atlasX, record->atlasY,
        record->width, record->height,
        record->screenX, record->screenY, record->width, record->height,
        record->sourceObjAlphaPx, record->framepackSourceForegroundPx,
        record->framepackObjOverlapPx, record->framepackNoObjResidualPx,
        record->hideRuntimePlayer ? "true" : "false", evidencePath.c_str());
}

void writeIntArray(FILE* file, const std::vector<int>& values) {
    std::fprintf(file, "[");
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) std::fprintf(file, ",");
        std::fprintf(file, "%d", values[i]);
    }
    std::fprintf(file, "]");
}

} // namespace mmx::gameplay_scene_trace

namespace mmx {

const char* GameplayScene::stateToString(PlayerState s) const {
    switch (s) {
        case PlayerState::Idle:      return "IDLE";
        case PlayerState::Run:       return "RUN";
        case PlayerState::Jump:      return "JUMP";
        case PlayerState::Fall:      return "FALL";
        case PlayerState::WallSlide: return "WALL_SLIDE";
        case PlayerState::WallJump:  return "WALL_JUMP";
        case PlayerState::Dash:      return "DASH";
        case PlayerState::DashJump:  return "DASH_JUMP";
        case PlayerState::Hurt:      return "HURT";
        case PlayerState::Die:       return "DIE";
        default: return "???";
    }
}

} // namespace mmx
