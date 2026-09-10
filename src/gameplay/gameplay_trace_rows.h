// gameplay_trace_rows.h - declares structured gameplay trace row payloads.
// Boundary: schema definitions only; writers own emission timing.

#pragma once

#include <cstdio>

namespace mmx::gameplay_trace {

inline constexpr const char* kProjectileTraceHeader =
    "tick,kind,serial,weapon,charged,fragment,x,y,vx,vy\n";

inline void writeProjectileRow(FILE* trace, long tick, const char* kind,
                               int serial, const char* weapon,
                               int charged, int fragment,
                               float x, float y, float vx, float vy) {
    std::fprintf(trace, "%ld,%s,%d,%s,%d,%d,%.6f,%.6f,%.6f,%.6f\n",
                 tick, kind, serial, weapon ? weapon : "-", charged, fragment,
                 x, y, vx, vy);
}

inline void writeParityRow(FILE* trace, long tick, const char* kind, int serial,
                           const char* id, int state, int hp,
                           float x, float y, float vx, float vy,
                           int a, int b, int c, int d,
                           int e = 0, int f = 0, int g = 0, int h = 0) {
    std::fprintf(trace,
                 "%ld,%s,%d,%s,%d,%d,%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%d,%d,%d,%d,%d\n",
                 tick, kind, serial, id ? id : "-",
                 state, hp, x, y, vx, vy, a, b, c, d, e, f, g, h);
}

} // namespace mmx::gameplay_trace
