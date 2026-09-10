// weapon_get_presentation.h - what the post-boss weapon-get sequence puts on
// screen and into the APU on each tick (GC2.1b).
//
// Oracle: knowledge_base/mmx1/story/weapon_get_sequence.json, reduced from
// David's 2026-07-26 Chill Penguin and Storm Eagle playthrough movies. Ticks
// are relative to the victory cue, which the source puts at movie frame f17819
// for Chill Penguin.
//
// weapon_get_timeline.h answers "which phase is this"; this header answers
// "so what is drawn and heard". Both are pure - no engine types, no raylib - so
// the whole sequence is checkable without standing up a renderer, and the
// raylib call site stays a thin consumer.
//
// SCOPE, because it is easy to overstate: the warp ladders, the 4-tick text
// cadence, and the demo-is-the-weapon's-own-SFX rule were cross-checked on two
// bosses. The fixed cue bytes ($2D/$0F/$F6/$0E) and the palette cycle were
// measured on Chill Penguin ONLY - the Storm Eagle pass covered warp, text, and
// demo. Six Mavericks are untested either way.
//
// GC2.1d-T1 decodes the literal BG3 rows for the two recorded bosses.
// GC2.1d-T3 proves the cycle does not own those text pixels; the exact cycling
// source art and full-screen pixel parity remain open. The current renderer
// still uses engine fonts and layout.

#pragma once

#include "gameplay/weapon_get_timeline.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace mmx::weapon_get_presentation {

// GC2.1d-T3 source ownership. Two independent captures per boss prove that
// states 3 and 4 leave the BG3 text region pixel-identical. The source also
// has neither the engine's centered colour swatch nor its demo caption.
inline constexpr bool kSourceTextCyclesColour = false;
inline constexpr bool kSourceHasCenteredSwatch = false;
inline constexpr bool kSourceHasDemoCaption = false;

// Native weapon-get previews reuse the gameplay projectile renderer for every
// Maverick weapon. Keep the eligibility rule in the pure presentation model so
// adding a weapon cannot silently require a second screen-only whitelist.
inline constexpr std::string_view kGameplayProjectileWeapons[] = {
    "shotgun-ice",
    "storm-tornado",
    "fire-wave",
    "electric-spark",
    "rolling-shield",
    "homing-torpedo",
    "boomerang-cutter",
    "chameleon-sting",
};

inline bool hasGameplayProjectileVisual(std::string_view weaponId) {
    for (const std::string_view candidate : kGameplayProjectileWeapons) {
        if (candidate == weaponId) return true;
    }
    return false;
}

struct DemoPattern {
    int shotCount = 3;
    int shotStepTicks = 40;
    bool measured = false;
};

// Only Chill Penguin and Storm Eagle have source recordings for the demo
// cadence. Every other reward intentionally inherits Chill's shape as a
// labeled fallback until its own source run is captured.
inline DemoPattern demoPatternForWeapon(std::string_view weaponId) {
    if (weaponId == "storm-tornado") return {2, 100, true};
    if (weaponId == "shotgun-ice") return {3, 40, true};
    return {};
}

// GC2.1d-T1 source rows, decoded from BG3 tilemap words rather than inferred
// from the audio count:
//   YOU GET / SHOTGUN / ICE
//   YOU GET / STORM / TORNADO
// The remaining Mavericks are not yet decoded, so splitting each weapon-name
// word onto its own row is a bounded two-boss generalization, not pixel parity.
inline std::string sourceText(std::string_view weaponName) {
    std::string out = "YOU GET\n";
    for (const char ch : weaponName) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        out.push_back(ch == ' ' ? '\n' : static_cast<char>(std::toupper(byte)));
    }
    return out;
}

inline int glyphCount(std::string_view text) {
    return static_cast<int>(std::count_if(
        text.begin(), text.end(), [](char ch) { return ch != '\n'; }));
}

// Reveal source glyphs while retaining silent row breaks. Newlines are layout,
// not glyphs: neither recorded boss emits an APU $0B for a line transition.
inline std::string revealText(std::string_view text, int visibleGlyphs) {
    std::string out;
    int remaining = std::max(0, visibleGlyphs);
    for (const char ch : text) {
        if (ch == '\n') {
            if (!out.empty() && remaining > 0) out.push_back(ch);
            continue;
        }
        if (remaining == 0) break;
        out.push_back(ch);
        --remaining;
    }
    while (!out.empty() && out.back() == '\n') out.pop_back();
    return out;
}

// Fixed APU cues, all from the Chill Penguin movie's audioCues list. The frame
// comments are the artifact's numbers; the constants are those minus 17819.
inline constexpr int kVictoryCue = 0x2D;
inline constexpr int kVictoryCueTick = 0;         // f17819
inline constexpr int kSecondVictoryCueTick = 274; // f18093
inline constexpr int kWarpOutCue = 0x0F;
inline constexpr int kWarpOutCueTick = 57;        // f17876, the warp-out start
inline constexpr int kScreenCue = 0xF6;
inline constexpr int kPostWarpOutCueTick = 81;    // f17900, one tick after it ends
inline constexpr int kTextBlipCue = 0x0B;         // FA3 has the authentic sample
inline constexpr int kWarpInCue = 0x0E;
inline constexpr int kWarpInCueTick = 555;        // f18374
inline constexpr int kDemoEndCueTick = 812;       // f18631, $F6 again
inline constexpr int kReturnCue = 0x0F;
inline constexpr int kReturnCueTick = 821;        // f18640

// Spec/text screen palette cycle. CGRAM was sampled every 4th frame, so each
// boundary below is resolved only to +/-4 ticks and the artifact says so.
inline constexpr int kPaletteStateOneTick = 215;   // f18034, the 120-frame hold
inline constexpr int kPaletteAlternateTick = 335;  // f18154, states 2<->1
inline constexpr int kPalettePairBTick = 463;      // f18282, states 3<->4
inline constexpr int kPaletteFinalHoldTick = 535;  // f18354, stops alternating
inline constexpr int kPaletteHoldTicks = 8;

// GC2.1d-T5: the native fallback follows the source framebuffer's screen
// envelope even when no complete source atlas exists for the awarded weapon.
// The CP dense atlas measures the first panel pixels at 273, a white flash at
// 303..334, and the stable panel at 355. Keeping this timing pure makes the
// gameplay draw gate independently testable and prevents the live X preview
// from leaking into the source's blackout/flash states.
inline constexpr int kNativeFallbackFadeStartTick = 273;
inline constexpr int kNativeFallbackWhiteStartTick = 303;
inline constexpr int kNativeFallbackWhiteEndTick = 335;
inline constexpr int kNativeFallbackStableTick = 355;

inline bool nativeFallbackPanelVisibleAtTick(int tick) {
    return tick >= kNativeFallbackFadeStartTick &&
           !(tick >= kNativeFallbackWhiteStartTick &&
             tick < kNativeFallbackWhiteEndTick);
}

enum class Screen {
    None,      // outside the sequence
    Arena,     // the stage itself: victory pose, then X warps out of it
    SpecText,  // the source's first no-live-actor run; the name types here
    Demo,      // X warps back in and fires the new weapon
    Handoff,   // the second no-live-actor run: back to stage select
};

struct Params {
    // textBlipCount is the source GLYPH count, excluding silent row breaks:
    // 17 for YOU GET / SHOTGUN / ICE and 19 for YOU GET / STORM / TORNADO.
    // The 4-tick cadence is the shared law and is fixed.
    weapon_get_timeline::Params timing;

    // The demo fires the weapon's OWN sound. The movie showed $01 for Shotgun
    // Ice and $63 for Storm Tornado; KB weapon.json sfx.fire independently says
    // 1 and 99 for exactly those two weapons, which is what makes this a rule
    // rather than two coincidences. -1 = measured silent (Fire Wave).
    int demoApuCommand = -1;

    // False when the demo shot count/step is Chill Penguin's default standing in
    // for an unrecorded Maverick. Six of eight are in that position.
    bool demoPatternMeasured = false;
};

struct Plan {
    weapon_get_timeline::Phase phase = weapon_get_timeline::Phase::Inactive;
    Screen screen = Screen::None;
    bool drawActor = false;   // X is on screen this tick
    int actorOffsetY = 0;     // px from the phase's anchor; negative is up
    int revealedChars = 0;    // characters of the typed string that are visible
    int paletteState = 0;     // 0..4, the spec-screen cycle
    int apuCue = -1;          // command byte to dispatch this tick, or -1
    bool handoffReady = false;
};

// Reproduces all 28 measured runs. The first hold is 120 ticks, then two pairs
// alternate on 8-tick holds, and the last state stops flipping at f18354 and
// holds until the screen ends - that final long hold is measured, not assumed.
inline int paletteState(int tick) {
    if (tick < kPaletteStateOneTick) return 0;
    if (tick < kPaletteAlternateTick) return 1;
    if (tick >= kPaletteFinalHoldTick) return 4;
    if (tick < kPalettePairBTick) {
        const int step = (tick - kPaletteAlternateTick) / kPaletteHoldTicks;
        return (step % 2 == 0) ? 2 : 1;
    }
    const int step = (tick - kPalettePairBTick) / kPaletteHoldTicks;
    return (step % 2 == 0) ? 3 : 4;
}

inline Screen screenForTick(int tick) {
    if (tick < 0) return Screen::None;
    if (tick < weapon_get_timeline::kSpecScreenTick) return Screen::Arena;
    if (tick < weapon_get_timeline::kWarpInStartTick) return Screen::SpecText;
    if (tick < weapon_get_timeline::kReturnTick) return Screen::Demo;
    return Screen::Handoff;
}

// The absolute source Y values are per-boss (Chill Penguin warps out from 431,
// Storm Eagle from 207), so only the LADDER generalises. Everything here is an
// offset from the phase's own anchor, which keeps the exact per-tick steps
// without pretending the source coordinate space is bridged to ours.
inline int actorOffsetY(int tick, weapon_get_timeline::Phase phase) {
    using weapon_get_timeline::Phase;
    if (phase == Phase::WarpOut) {
        return weapon_get_timeline::warpOutY(tick) -
               weapon_get_timeline::kWarpOutStartY;
    }
    if (phase == Phase::WarpIn) {
        return weapon_get_timeline::warpInY(tick) -
               weapon_get_timeline::kWarpInEndY;
    }
    return 0;
}

inline int fixedCueForTick(int tick) {
    if (tick == kVictoryCueTick || tick == kSecondVictoryCueTick) return kVictoryCue;
    if (tick == kWarpOutCueTick) return kWarpOutCue;
    if (tick == kPostWarpOutCueTick || tick == kDemoEndCueTick) return kScreenCue;
    if (tick == kWarpInCueTick) return kWarpInCue;
    if (tick == kReturnCueTick) return kReturnCue;
    return -1;
}

inline Plan plan(int tick, const Params& params, int textLength) {
    using weapon_get_timeline::Phase;

    Plan out;
    if (tick < 0) return out;

    const weapon_get_timeline::Frame frame =
        weapon_get_timeline::evaluate(tick, params.timing);

    out.phase = frame.phase;
    out.screen = screenForTick(tick);
    out.paletteState = paletteState(tick);
    out.handoffReady = tick >= weapon_get_timeline::kReturnTick;

    out.drawActor = frame.phase == Phase::VictoryPose ||
                    frame.phase == Phase::WarpOut ||
                    frame.phase == Phase::WarpIn ||
                    frame.phase == Phase::Demo;
    out.actorOffsetY = actorOffsetY(tick, frame.phase);

    const int length = std::max(0, textLength);
    if (tick >= weapon_get_timeline::kTextStartTick && length > 0) {
        const int typed = (tick - weapon_get_timeline::kTextStartTick) /
                              weapon_get_timeline::kTextStepTicks + 1;
        out.revealedChars = std::min(length, typed);
    }

    out.apuCue = fixedCueForTick(tick);
    if (out.apuCue < 0 && frame.emitTextBlip) out.apuCue = kTextBlipCue;
    if (out.apuCue < 0 && frame.emitDemoShot) out.apuCue = params.demoApuCommand;

    return out;
}

}  // namespace mmx::weapon_get_presentation
