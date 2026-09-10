#pragma once

// ============================================================================
// constants.h — Global engine constants
//
// These define the "shape" of the game: resolution, physics rate, and scaling.
// The SNES ran at 256x224 for NTSC. We render to an internal framebuffer at
// that resolution, then scale it up to the window size. This gives us crisp
// pixel art at any window size without changing game logic.
// ============================================================================

namespace mmx {

// Internal render resolution (matches SNES)
constexpr int INTERNAL_WIDTH  = 256;
constexpr int INTERNAL_HEIGHT = 224;

// Default window size (4x SNES resolution)
constexpr int DEFAULT_WINDOW_SCALE = 4;
constexpr int DEFAULT_WINDOW_WIDTH  = INTERNAL_WIDTH * DEFAULT_WINDOW_SCALE;
constexpr int DEFAULT_WINDOW_HEIGHT = INTERNAL_HEIGHT * DEFAULT_WINDOW_SCALE;

// Physics timestep — the heartbeat of the engine.
// All gameplay timing (dash duration, i-frames, charge times) is defined
// in physics frames, not seconds. This makes behavior identical regardless
// of render framerate. 60Hz matches the SNES NTSC refresh rate.
constexpr float PHYSICS_HZ = 60.0f;
constexpr float PHYSICS_DT = 1.0f / PHYSICS_HZ;

// Tile size in pixels (SNES standard)
constexpr int TILE_SIZE = 16;

// Max physics steps per frame — safety valve.
// If the game hitches (debugger pause, window drag), this prevents the
// physics from trying to simulate 500 frames at once and spiraling.
constexpr int MAX_PHYSICS_STEPS_PER_FRAME = 5;

} // namespace mmx
