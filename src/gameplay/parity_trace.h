// parity_trace.h - declares parity trace record structures for gameplay probes.
// Boundary: trace schema only; collection is opt-in diagnostic plumbing.

#pragma once

namespace mmx::parity_trace {

inline constexpr const char* kHeader =
    "tick,kind,serial,id,state,hp,x,y,vx,vy,a,b,c,d,e,f,g,h";

inline constexpr const char* kRequiredKinds[] = {
    "camera",
    "player",
    "player_hp",
    "audio_apu",
    "audio_sfx",
    "projectile",
    "enemy",
    "boss",
    "stage_object",
    "pickup",
    "fx",
    "transition",
};

} // namespace mmx::parity_trace
