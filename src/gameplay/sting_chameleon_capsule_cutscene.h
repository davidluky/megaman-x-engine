// sting_chameleon_capsule_cutscene.h - source-timed Sting body capsule model.
// Owns FU7 dialog/lifecycle facts and T25D1 source presentation selection;
// no stage, GameplayScene, render-call, audio-playback, or persistence wiring.

#pragma once

#include "gameplay/capsule_cutscene.h"

#include <array>
#include <string>

namespace mmx {

struct StingChameleonCapsuleDialogLine {
    int lineIndex = 0;
    int tilemapRow = 0;
    const char* text = "";
    int sourceCompleteFrame = 0;
    int localCompleteFrame = 0;
};

struct StingChameleonCapsuleDialogPage {
    int pageIndex = 0;
    int sourceFirstCharacterFrame = 0;
    int localFirstCharacterFrame = 0;
    int sourceCompleteFrame = 0;
    int localCompleteFrame = 0;
    int sourcePromptFirstFrame = 0;
    int localPromptFirstFrame = 0;
    int sourcePromptLastFrame = 0;
    int localPromptLastFrame = 0;
    int promptVisibleFrameCount = 0;
    int sourceAdvanceInputFrame = 0;
    int localAdvanceInputFrame = 0;
};

struct StingChameleonCapsuleScrollTransition {
    int inputSourceFrame = 0;
    int inputLocalFrame = 0;
    int firstSourceFrame = 0;
    int firstLocalFrame = 0;
    int firstValue = 0;
    int lastSourceFrame = 0;
    int lastLocalFrame = 0;
    int lastValue = 0;
    int frameCount = 0;
};

struct StingChameleonCapsuleTextBlipGroup {
    int groupIndex = 0;
    int sourceFirstFrame = 0;
    int sourceLastFrame = 0;
    int localFirstFrame = 0;
    int localLastFrame = 0;
    int commandCount = 0;
};

struct StingChameleonCapsuleRawMailboxCommand {
    int sourceFrame = 0;
    int localFrame = 0;
    int bank = 0;
    int command = 0;
    std::array<int, 3> parameters{};
    int parameterCount = 0;
};

struct StingChameleonCapsulePhaseWindow {
    const char* name = "";
    int sourceFirstFrame = 0;
    int sourceLastFrame = 0;
    int localFirstFrame = 0;
    int localLastFrame = 0;
};

struct StingChameleonCapsuleDialogState {
    int localFrame = 0;
    int sourceFrame = 0;
    int page = 0;
    int bg3VScroll = 0;
    int completedLineCount = 0;
    bool textTilemapNonempty = false;
    bool characterRevealWindow = false;
    int textBlipGroupWindow = 0;
    bool promptObservationWindow = false;
};

struct StingChameleonCapsuleVisualRegion {
    int atlasPage = -1;
    int atlasX = 0;
    int atlasY = 0;
    int width = 0;
    int height = 0;
    int screenX = 0;
    int screenY = 0;

    bool visible() const {
        return atlasPage >= 0 && width > 0 && height > 0;
    }
};

struct StingChameleonCapsuleVisualFrame {
    int localFrame = 0;
    int sourceFrame = 0;
    const char* phase = "";
    StingChameleonCapsuleVisualRegion character;
    StingChameleonCapsuleVisualRegion object;
    StingChameleonCapsuleVisualRegion enhancement;
    StingChameleonCapsuleVisualRegion bg3;
    StingChameleonCapsuleVisualRegion portrait;
    bool textBlipEvent = false;
    int promptPageIndex = -1;
    bool promptVisible = false;
};

struct StingChameleonCapsuleArenaBackdrop {
    const char* atlasPath = "";
    int atlasX = 0;
    int width = 0;
    int height = 0;
    int screenX = 0;
    int screenY = 0;
};

struct StingChameleonCapsuleDialogWindow {
    bool visible = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

class StingChameleonCapsuleCutscene {
public:
    struct TickEvents {
        bool grantBody = false;
        bool releaseControls = false;
    };

    static constexpr int kFirstSourceRouteFrame = 270;
    static constexpr int kLastSourceRouteFrame = 1729;
    static constexpr int kFirstLocalFrame = 0;
    static constexpr int kLastLocalFrame = 1459;
    static constexpr int kSourceFrameCount = 1460;
    static constexpr int kBodyGrantSourceFrame = 1523;
    static constexpr int kBodyGrantFrame = 1253;
    static constexpr int kControlReleaseSourceFrame = 1729;
    static constexpr int kControlReleaseFrame = 1459;
    static constexpr int kFirstPostReleaseMotionSourceFrame = 1730;
    static constexpr int kFirstPostReleaseMotionFrame = 1460;
    static constexpr int kLastProvenPostReleaseSourceFrame = 1800;

    // FU7-T25E1a: the ROM names X by an object anchor while the engine names
    // the 70x70 logical sprite top-left. Independent wall/floor collision
    // surfaces prove this conversion; the 8.8 release values are ROM rows.
    static constexpr int kSourceAnchorToEngineTopLeftX = -37;
    static constexpr int kSourceAnchorToEngineTopLeftY = -24;
    static constexpr int kReleaseSourceXRaw = 448621;
    static constexpr int kReleaseSourceYRaw = 106370;

    static constexpr int kInteractionPlayerX = 1713;
    static constexpr int kInteractionPlayerY = 431;
    static constexpr int kPedestalX = 1752;
    static constexpr int kPedestalY = 415;
    // The hidden capsule arena renders with a STATIC source camera: all 12
    // committed hardware stills (f0408..f1729) register against the decoded
    // stage main layer at exactly this window
    // (docs/evidence/2026-08-09-fu7-sting-capsule-camera-window).
    static constexpr int kArenaCameraX = 1536;
    static constexpr int kArenaCameraY = 257;
    static constexpr int kArenaCameraW = 256;
    static constexpr int kArenaCameraH = 224;
    static constexpr int kPedestalLockFrame = 934;
    static constexpr int kEnhancementFirstFrame = 1189;
    static constexpr int kEnhancementLastFrame = 1380;

    void start(std::string persistentPickupId);
    void reset();
    TickEvents tick();

    bool active() const { return lifecycle_.active(); }
    bool controlsLocked() const { return lifecycle_.controlsLocked(); }
    int frame() const { return lifecycle_.frame(); }
    const std::string& persistentPickupId() const {
        return lifecycle_.persistentPickupId();
    }

    static int sourceRouteFrameForLocalFrame(int localFrame);
    static int localFrameForSourceRouteFrame(int sourceFrame);
    static const char* phaseForFrame(int localFrame);
    static StingChameleonCapsuleDialogState dialogStateForFrame(int localFrame);
    static const StingChameleonCapsuleVisualFrame& visualStateForFrame(int localFrame);
    static const char* characterAtlasPath(int atlasPage);
    static const char* objectAtlasPath(int atlasPage);
    static const char* enhancementAtlasPath(int atlasPage);
    static const char* bg3AtlasPath(int atlasPage);
    static const char* portraitAtlasPath(int atlasPage);
    // Arena backdrop: the source scene's crumbling rock wall, revealed sky,
    // and black dialog window, derived from the 12 committed hardware stills
    // (keyframe timing bounded by the stills; per-frame fall timing and the
    // pre-dialog state of the window-covered mid region are unclaimed).
    static int arenaBackdropRegionCount();
    // Returns an inactive draw (width 0) when the region is not yet active
    // at this local frame.
    static StingChameleonCapsuleArenaBackdrop arenaBackdropForFrame(
        int regionIndex, int localFrame);
    static StingChameleonCapsuleDialogWindow dialogWindowForFrame(int localFrame);
    static bool textBlipEventForFrame(int localFrame);
    static bool promptVisibleForFrame(int localFrame);
    static int postReleaseSourceXRaw(int sourceFrame);
    static int postReleaseDeltaXRaw(int sourceFrame);
    static float postReleaseEngineX(int sourceFrame);
    static float postReleaseEngineY();

    static int dialogLineCount();
    static const StingChameleonCapsuleDialogLine* dialogLine(int index);
    static int dialogPageCount();
    static const StingChameleonCapsuleDialogPage* dialogPage(int index);
    static int scrollTransitionCount();
    static const StingChameleonCapsuleScrollTransition* scrollTransition(int index);
    static int textBlipGroupCount();
    static const StingChameleonCapsuleTextBlipGroup* textBlipGroup(int index);
    static constexpr int textBlipCommandCount() { return 96; }
    static constexpr int textBlipBank() { return 0x00; }
    static constexpr int textBlipCommand() { return 0x0B; }
    static constexpr int textBlipParameter() { return 0x00; }
    static constexpr bool individualTextBlipFramesAvailable() { return true; }
    static int rawMailboxCommandCount();
    static const StingChameleonCapsuleRawMailboxCommand* rawMailboxCommand(int index);

private:
    CapsuleCutscene lifecycle_{CapsuleCutsceneDefinition{
        ArmorUpgradePart::Body,
        kBodyGrantFrame,
        kControlReleaseFrame,
    }};
};

} // namespace mmx
