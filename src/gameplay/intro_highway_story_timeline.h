// intro_highway_story_timeline.h - pure source-clock choreography for GC3.1.
//
// Oracle: knowledge_base/mmx1/story/intro_highway_story_timeline.json.
// Input-lock scope:
// knowledge_base/mmx1/story/intro_highway_input_lock.json.
// Post-7080 gameplay/dialogue control split:
// knowledge_base/mmx1/story/intro_highway_dialogue_controls.json.
// Exact first-dialogue held-action acceleration:
// knowledge_base/mmx1/story/intro_highway_dialogue_acceleration.json.
// Exact bounded second-dialogue held-action acceleration:
// knowledge_base/mmx1/story/intro_highway_second_dialogue_acceleration.json.
// Second-dialogue hold/release/repress law:
// knowledge_base/mmx1/story/intro_highway_second_dialogue_page_controls.json.
// Late second-dialogue gameplay lock/action acceleration:
// knowledge_base/mmx1/story/intro_highway_late_dialogue_controls.json.
// Final Zero-exit/X-walk/X-warp input lock:
// knowledge_base/mmx1/story/intro_highway_final_input_lock.json.
// Rescue-phase input lock:
// knowledge_base/mmx1/story/intro_highway_rescue_input_lock.json.
// Rescue attack/camera input lock:
// knowledge_base/mmx1/story/intro_highway_attack_input_lock.json.
// Post-attack input lock:
// knowledge_base/mmx1/story/intro_highway_post_attack_input_lock.json.
// Vile-escape input lock:
// knowledge_base/mmx1/story/intro_highway_vile_escape_input_lock.json.
// Second-dialogue opening input lock:
// knowledge_base/mmx1/story/intro_highway_second_dialogue_open_input_lock.json.
// Kinematics:
// knowledge_base/mmx1/story/intro_highway_story_kinematics.json.
// Horizontal camera bridge:
// knowledge_base/mmx1/story/intro_highway_story_camera.json.
// Vertical/OAM anchor bridge:
// knowledge_base/mmx1/story/intro_highway_story_vertical.json.
// Isolated reference composites:
// knowledge_base/mmx1/story/intro_highway_story_composites.json.
// Additional presentation-state composites:
// knowledge_base/mmx1/story/intro_highway_story_composite_states.json.
// Dialogue chrome:
// knowledge_base/mmx1/story/intro_highway_story_dialogue_chrome.json.
// Dialogue text and raster-visible close:
// knowledge_base/mmx1/story/intro_highway_story_dialogue_text.json.
// First dialogue text and raster presentation:
// knowledge_base/mmx1/story/intro_highway_story_first_dialogue.json.
// Shared authenticated dialogue font:
// knowledge_base/mmx1/story/intro_highway_story_dialogue_font.json.
//
// This module owns no rendering, physics, audio, or scene state. It reduces
// source frames into a data-defined plan so the GameplayScene adapter can stay
// thin and later story scenes can reuse the same phase/cue boundary pattern.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mmx::intro_highway_story_timeline {

inline constexpr int kSourceStartFrame = 6350;
inline constexpr int kSourceEndFrame = 8849;
inline constexpr int kInputLockStartFrame = 6901;
inline constexpr int kInputLockEndFrame = 8160;
inline constexpr int kProvenDialogueActionStartFrame = 7094;
inline constexpr int kProvenDialogueActionEndFrame = 7140;
inline constexpr int kProvenSecondDialogueActionStartFrame = 8223;
inline constexpr int kProvenSecondDialogueActionEndFrame = 8250;
inline constexpr int kProvenSecondDialogueHeldPageActionStartFrame = 8300;
inline constexpr int kProvenSecondDialogueHeldPageActionEndFrame = 8349;
inline constexpr int kProvenLateGameplayControlLockStartFrame = 8461;
inline constexpr int kProvenLateGameplayControlLockEndFrame = 8849;
inline constexpr int kProvenLateDialogueActionStartFrame = 8488;
inline constexpr int kProvenLateDialogueActionEndFrame = 8518;
inline constexpr int kDialogueHeldActionRateMultiplier = 4;

enum class Phase : std::uint8_t {
    Inactive,
    VileArrival,
    VileAttack,
    FirstDialogue,
    ZeroRescue,
    RescuePause,
    TwoSpeakerDialogue,
    DialogueClose,
    ZeroExit,
    XWalk,
    XWarpOut,
};

enum class Actor : std::uint8_t {
    None = 0,
    X = 1 << 0,
    Vile = 1 << 1,
    Zero = 1 << 2,
};

enum class CompositeState : std::uint8_t {
    DefeatedHeld,
    ReturnWalk,
    PreRescueSparks,
    HitReaction,
    DialogueStand,
    ExitBeam,
};

constexpr Actor operator|(Actor lhs, Actor rhs) {
    return static_cast<Actor>(
        static_cast<std::uint8_t>(lhs) |
        static_cast<std::uint8_t>(rhs));
}

constexpr bool hasActor(Actor actors, Actor actor) {
    return (
        static_cast<std::uint8_t>(actors) &
        static_cast<std::uint8_t>(actor)) != 0;
}

enum class DialogueOverlay : std::uint8_t {
    None = 0,
    FirstSpeaker = 1 << 0,
    XPortrait = 1 << 1,
    ZeroPortrait = 1 << 2,
};

constexpr DialogueOverlay operator|(
    DialogueOverlay lhs, DialogueOverlay rhs) {
    return static_cast<DialogueOverlay>(
        static_cast<std::uint8_t>(lhs) |
        static_cast<std::uint8_t>(rhs));
}

enum class Cue : std::uint8_t {
    VileAppears,
    VileSettled,
    VileAttackBegins,
    XHeldByVile,
    FirstDialogueObjectBegins,
    FirstDialoguePanelAppears,
    FirstDialoguePanelFull,
    FirstDialoguePortraitAppears,
    FirstDialoguePortraitEnds,
    FirstDialoguePanelCloseBegins,
    FirstDialoguePanelClears,
    ZeroAppears,
    ZeroAttackWindup,
    VileHitReaction,
    ZeroApproach,
    VileEscape,
    VileDisappears,
    SecondDialogueBegins,
    XPortraitText,
    ZeroPortraitText,
    DialogueObjectLifetimeEnds,
    DialoguePortraitRasterEnds,
    DialoguePanelCloseBegins,
    DialoguePanelClears,
    ZeroExitBegins,
    ZeroDisappears,
    XWalkBegins,
    XWarpOutBegins,
    XDisappears,
};

struct Interval {
    int first = 0;
    int last = -1;

    constexpr bool contains(int frame) const {
        return frame >= first && frame <= last;
    }
};

struct PhaseInterval {
    Interval frames;
    Phase phase = Phase::Inactive;
};

struct CuePoint {
    int frame = 0;
    Cue cue = Cue::VileAppears;
};

struct Position {
    int x = 0;
    int y = 0;

    constexpr bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }
};

struct ActorStateInterval {
    Interval frames;
    Actor actor = Actor::None;
    std::uint8_t action1 = 0;
    std::uint8_t action2 = 0;
    Position start;
    Position end;
};

struct XKinematicAnchor {
    int frame = 0;
    Position position;
    std::uint8_t animation = 0;
};

enum class MotionPolicy : std::uint8_t {
    ExactLinearEverySourceFrame,
    MonotonicSourceAnchors,
};

struct MotionSegment {
    Interval frames;
    Actor actor = Actor::None;
    Position start;
    Position end;
    MotionPolicy policy = MotionPolicy::MonotonicSourceAnchors;
    Position exactStep;
};

struct HorizontalCameraSegment {
    Interval frames;
    int startX = 0;
    int stepX = 0;
};

struct RelativeBounds {
    int left = 0;
    int top = 0;
    int rightExclusive = 0;
    int bottomExclusive = 0;

    constexpr bool operator==(const RelativeBounds& other) const {
        return left == other.left && top == other.top &&
               rightExclusive == other.rightExclusive &&
               bottomExclusive == other.bottomExclusive;
    }
};

struct ReferenceComposite {
    Actor actor = Actor::None;
    int sourceFrame = 0;
    std::uint8_t palette = 0;
    RelativeBounds opaqueBounds;
};

struct PresentationComposite {
    Actor actor = Actor::None;
    CompositeState state = CompositeState::DefeatedHeld;
    int sourceFrame = 0;
    std::uint8_t palette = 0;
    RelativeBounds opaqueBounds;
    bool includesSamePaletteEffect = false;
};

struct ScreenRectangle {
    int left = 0;
    int top = 0;
    int rightExclusive = 0;
    int bottomExclusive = 0;

    constexpr bool operator==(const ScreenRectangle& other) const {
        return left == other.left && top == other.top &&
               rightExclusive == other.rightExclusive &&
               bottomExclusive == other.bottomExclusive;
    }
};

struct DialogueChromeReference {
    Actor portraitActor = Actor::None;
    int sourceFrame = 0;
    std::uint8_t palette = 0;
    ScreenRectangle portrait;
};

struct Plan {
    int sourceFrame = 0;
    Phase phase = Phase::Inactive;
    Actor actors = Actor::None;
    DialogueOverlay dialogue = DialogueOverlay::None;
    bool controlsLocked = false;
    bool heldActionAcceleratesDialogue = false;
    bool dialoguePanelVisible = false;
    bool xVisible = false;
    bool xWarpOut = false;
};

inline constexpr std::array<PhaseInterval, 10> kPhases{{
    {{6350, 6776}, Phase::VileArrival},
    {{6777, 6998}, Phase::VileAttack},
    {{6999, 7229}, Phase::FirstDialogue},
    {{7230, 7943}, Phase::ZeroRescue},
    {{7944, 8125}, Phase::RescuePause},
    {{8126, 8528}, Phase::TwoSpeakerDialogue},
    {{8529, 8618}, Phase::DialogueClose},
    {{8619, 8645}, Phase::ZeroExit},
    {{8646, 8734}, Phase::XWalk},
    {{8735, 8849}, Phase::XWarpOut},
}};

inline constexpr std::array<CuePoint, 29> kCues{{
    {6350, Cue::VileAppears},
    {6458, Cue::VileSettled},
    {6777, Cue::VileAttackBegins},
    {6851, Cue::XHeldByVile},
    {6999, Cue::FirstDialogueObjectBegins},
    {7004, Cue::FirstDialoguePanelAppears},
    {7091, Cue::FirstDialoguePanelFull},
    {7094, Cue::FirstDialoguePortraitAppears},
    {7142, Cue::FirstDialoguePortraitEnds},
    {7143, Cue::FirstDialoguePanelCloseBegins},
    {7182, Cue::FirstDialoguePanelClears},
    {7230, Cue::ZeroAppears},
    {7453, Cue::ZeroAttackWindup},
    {7473, Cue::VileHitReaction},
    {7553, Cue::ZeroApproach},
    {7863, Cue::VileEscape},
    {7944, Cue::VileDisappears},
    {8126, Cue::SecondDialogueBegins},
    {8218, Cue::XPortraitText},
    {8258, Cue::ZeroPortraitText},
    {8529, Cue::DialogueObjectLifetimeEnds},
    {8531, Cue::DialoguePortraitRasterEnds},
    {8532, Cue::DialoguePanelCloseBegins},
    {8579, Cue::DialoguePanelClears},
    {8619, Cue::ZeroExitBegins},
    {8646, Cue::ZeroDisappears},
    {8648, Cue::XWalkBegins},
    {8735, Cue::XWarpOutBegins},
    {8850, Cue::XDisappears},
}};

// Maximal source action1/action2 intervals. Positions are exact endpoint
// observations; they do not imply interpolation between endpoints.
inline constexpr std::array<ActorStateInterval, 26> kActorStates{{
    {{6350, 6350}, Actor::Vile, 0, 0, {7432, 256}, {7432, 256}},
    {{6351, 6434}, Actor::Vile, 2, 0, {7432, 256}, {7432, 297}},
    {{6435, 6457}, Actor::Vile, 2, 2, {7432, 298}, {7432, 314}},
    {{6458, 6775}, Actor::Vile, 2, 36, {7432, 313}, {7432, 414}},
    {{6776, 6776}, Actor::Vile, 2, 4, {7432, 414}, {7432, 414}},
    {{6777, 6805}, Actor::Vile, 2, 14, {7432, 414}, {7322, 414}},
    {{6806, 6816}, Actor::Vile, 2, 24, {7322, 414}, {7323, 408}},
    {{6817, 6817}, Actor::Vile, 2, 4, {7323, 408}, {7323, 408}},
    {{6818, 6818}, Actor::Vile, 2, 6, {7323, 408}, {7323, 408}},
    {{6819, 6823}, Actor::Vile, 2, 20, {7323, 408}, {7329, 414}},
    {{6824, 6838}, Actor::Vile, 2, 22, {7329, 414}, {7329, 414}},
    {{6839, 6839}, Actor::Vile, 2, 4, {7329, 414}, {7329, 414}},
    {{6840, 6850}, Actor::Vile, 2, 6, {7329, 414}, {7315, 414}},
    {{6851, 7472}, Actor::Vile, 2, 26, {7313, 414}, {7313, 414}},
    {{7473, 7862}, Actor::Vile, 2, 28, {7313, 414}, {7408, 414}},
    {{7863, 7883}, Actor::Vile, 2, 30, {7408, 414}, {7408, 314}},
    {{7884, 7900}, Actor::Vile, 2, 32, {7408, 310}, {7408, 297}},
    {{7901, 7942}, Actor::Vile, 2, 34, {7408, 297}, {7408, 256}},
    {{7943, 7943}, Actor::Vile, 4, 34, {7408, 255}, {7408, 255}},
    {{7230, 7452}, Actor::Zero, 2, 0, {7100, 431}, {7100, 423}},
    {{7453, 7552}, Actor::Zero, 2, 10, {7100, 423}, {7100, 423}},
    {{7553, 7660}, Actor::Zero, 2, 2, {7100, 423}, {7292, 423}},
    {{7661, 7883}, Actor::Zero, 2, 0, {7292, 423}, {7292, 423}},
    {{7884, 8618}, Actor::Zero, 2, 6, {7292, 423}, {7292, 423}},
    {{8619, 8644}, Actor::Zero, 2, 8, {7292, 423}, {7292, 231}},
    {{8645, 8645}, Actor::Zero, 4, 8, {7292, 221}, {7292, 221}},
}};

inline constexpr std::array<XKinematicAnchor, 15> kXKinematicAnchors{{
    {6777, {7258, 431}, 239},
    {6804, {7291, 431}, 63},
    {6808, {7291, 425}, 111},
    {6851, {7276, 431}, 160},
    {6987, {7268, 423}, 160},
    {6993, {7272, 417}, 200},
    {7474, {7272, 417}, 79},
    {7521, {7256, 431}, 160},
    {8647, {7256, 431}, 158},
    {8648, {7258, 431}, 158},
    {8693, {7324, 431}, 182},
    {8735, {7324, 431}, 69},
    {8758, {7324, 186}, 69},
    {8849, {7324, 186}, 69},
    {8850, {0, 0}, 0},
}};

// Only the first three segments have source-proven per-frame linear steps.
// The remaining endpoints are exact and monotonic, but interpolation is open.
inline constexpr std::array<MotionSegment, 6> kMotionSegments{{
    {{7553, 7601}, Actor::Zero, {7100, 423}, {7292, 423},
     MotionPolicy::ExactLinearEverySourceFrame, {4, 0}},
    {{7863, 7883}, Actor::Vile, {7408, 414}, {7408, 314},
     MotionPolicy::ExactLinearEverySourceFrame, {0, -5}},
    {{7901, 7943}, Actor::Vile, {7408, 297}, {7408, 255},
     MotionPolicy::ExactLinearEverySourceFrame, {0, -1}},
    {{8626, 8645}, Actor::Zero, {7292, 423}, {7292, 221},
     MotionPolicy::MonotonicSourceAnchors, {0, 0}},
    {{8647, 8693}, Actor::X, {7256, 431}, {7324, 431},
     MotionPolicy::MonotonicSourceAnchors, {0, 0}},
    {{8735, 8758}, Actor::X, {7324, 431}, {7324, 186},
    MotionPolicy::MonotonicSourceAnchors, {0, 0}},
}};

inline constexpr std::array<HorizontalCameraSegment, 5>
    kHorizontalCameraSegments{{
        {{6300, 6852}, 7226, 0},
        {{6852, 6891}, 7226, -2},
        {{6891, 7601}, 7148, 0},
        {{7601, 7625}, 7148, 2},
        {{7625, 8849}, 7196, 0},
    }};

// These are pose-local source facts, not universal collision or draw boxes.
inline constexpr std::array<ReferenceComposite, 3> kReferenceComposites{{
    {Actor::X, 6360, 1, {-14, -17, 16, 17}},
    {Actor::Vile, 6720, 4, {-26, -38, 29, 34}},
    {Actor::Zero, 7950, 6, {-16, -18, 12, 25}},
}};
inline constexpr std::array<PresentationComposite, 6>
    kPresentationComposites{{
        {Actor::X, CompositeState::DefeatedHeld, 6900, 1,
         {-11, -10, 14, 17}, false},
        {Actor::X, CompositeState::ReturnWalk, 8730, 1,
         {-14, -17, 16, 17}, false},
        {Actor::Vile, CompositeState::PreRescueSparks, 7230, 4,
         {-57, -39, 27, 34}, true},
        {Actor::Vile, CompositeState::HitReaction, 7500, 4,
         {-32, -39, 18, 34}, true},
        {Actor::Zero, CompositeState::DialogueStand, 8220, 6,
         {-12, -19, 16, 25}, false},
        {Actor::Zero, CompositeState::ExitBeam, 8640, 6,
         {-4, 5, 4, 45}, true},
    }};

inline constexpr ScreenRectangle kDialoguePanel{40, 32, 217, 128};
inline constexpr int kDialogueFontChrBytes = 4096;
inline constexpr int kDialogueFontTilemapBaseBytes = 4096;
inline constexpr std::array<DialogueChromeReference, 2>
    kDialogueChromeReferences{{
        {Actor::X, 8220, 1, {48, 88, 80, 120}},
        {Actor::Zero, 8520, 6, {176, 88, 208, 120}},
    }};

inline constexpr Interval kXVisible{6350, 8849};
inline constexpr Interval kVileVisible{6350, 7943};
inline constexpr Interval kZeroVisible{7230, 8645};
inline constexpr Interval kFirstSpeakerOverlay{7094, 7141};
inline constexpr Interval kProvenFirstDialoguePanelVisible{7004, 7181};
inline constexpr Interval kXPortraitOverlay{8126, 8257};
inline constexpr Interval kZeroPortraitOverlay{8126, 8530};
inline constexpr Interval kProvenSecondDialoguePanelVisible{8220, 8578};
inline constexpr Interval kProvenInputLock{
    kInputLockStartFrame,
    kInputLockEndFrame,
};
inline constexpr Interval kProvenDialogueActionWindow{
    kProvenDialogueActionStartFrame,
    kProvenDialogueActionEndFrame,
};
inline constexpr Interval kProvenSecondDialogueActionWindow{
    kProvenSecondDialogueActionStartFrame,
    kProvenSecondDialogueActionEndFrame,
};
inline constexpr Interval kProvenSecondDialogueHeldPageActionWindow{
    kProvenSecondDialogueHeldPageActionStartFrame,
    kProvenSecondDialogueHeldPageActionEndFrame,
};
inline constexpr Interval kProvenLateGameplayControlLock{
    kProvenLateGameplayControlLockStartFrame,
    kProvenLateGameplayControlLockEndFrame,
};
inline constexpr Interval kProvenLateDialogueActionWindow{
    kProvenLateDialogueActionStartFrame,
    kProvenLateDialogueActionEndFrame,
};

constexpr bool cueAt(int sourceFrame, Cue cue) {
    for (const auto& point : kCues) {
        if (point.frame == sourceFrame && point.cue == cue) return true;
    }
    return false;
}

constexpr const ActorStateInterval* actorStateAt(
    int sourceFrame,
    Actor actor) {
    for (const auto& state : kActorStates) {
        if (state.actor == actor && state.frames.contains(sourceFrame)) {
            return &state;
        }
    }
    return nullptr;
}

constexpr const XKinematicAnchor* xAnchorAt(int sourceFrame) {
    for (const auto& anchor : kXKinematicAnchors) {
        if (anchor.frame == sourceFrame) return &anchor;
    }
    return nullptr;
}

constexpr bool exactLinearPositionAt(
    const MotionSegment& segment,
    int sourceFrame,
    Position& position) {
    if (segment.policy != MotionPolicy::ExactLinearEverySourceFrame ||
        !segment.frames.contains(sourceFrame)) {
        return false;
    }
    const int offset = sourceFrame - segment.frames.first;
    position = {
        segment.start.x + segment.exactStep.x * offset,
        segment.start.y + segment.exactStep.y * offset,
    };
    return true;
}

constexpr bool horizontalCameraXAt(int sourceFrame, int& cameraX) {
    for (const auto& segment : kHorizontalCameraSegments) {
        if (!segment.frames.contains(sourceFrame)) continue;
        cameraX = segment.startX +
                  segment.stepX * (sourceFrame - segment.frames.first);
        return true;
    }
    return false;
}

constexpr int horizontalScreenX(int worldX, int cameraX) {
    return worldX - cameraX;
}

constexpr int verticalScreenY(int worldY, int cameraY) {
    return static_cast<std::uint8_t>(worldY - cameraY);
}

constexpr const ReferenceComposite* referenceCompositeAt(
    Actor actor,
    int sourceFrame) {
    for (const auto& composite : kReferenceComposites) {
        if (composite.actor == actor &&
            composite.sourceFrame == sourceFrame) {
            return &composite;
        }
    }
    return nullptr;
}

constexpr const DialogueChromeReference* dialogueChromeAt(
    Actor portraitActor,
    int sourceFrame) {
    for (const auto& chrome : kDialogueChromeReferences) {
        if (chrome.portraitActor == portraitActor &&
            chrome.sourceFrame == sourceFrame) {
            return &chrome;
        }
    }
    return nullptr;
}

constexpr bool dialogueClosingPanelAt(
    int sourceFrame,
    ScreenRectangle& panel) {
    if (sourceFrame < 8531 || sourceFrame > 8578) return false;
    const int inset = sourceFrame - 8531;
    panel = {
        kDialoguePanel.left + inset,
        kDialoguePanel.top + inset,
        kDialoguePanel.rightExclusive - inset,
        kDialoguePanel.bottomExclusive - inset,
    };
    return true;
}

constexpr bool firstDialoguePanelAt(
    int sourceFrame,
    ScreenRectangle& panel) {
    if (sourceFrame >= 7004 && sourceFrame <= 7043) {
        const int offset = sourceFrame - 7004;
        panel = {127 - offset, 71 - offset, 130 + offset, 73 + offset};
        return true;
    }
    if (sourceFrame >= 7044 && sourceFrame <= 7091) {
        const int offset = sourceFrame - 7043;
        panel = {88 - offset, 32, 169 + offset, 112};
        return true;
    }
    if (sourceFrame >= 7092 && sourceFrame <= 7142) {
        panel = {40, 32, 217, 112};
        return true;
    }
    if (sourceFrame >= 7143 && sourceFrame <= 7181) {
        const int offset = sourceFrame - 7142;
        panel = {40 + offset, 32 + offset, 217 - offset, 112 - offset};
        return true;
    }
    return false;
}

constexpr std::uint16_t dialogueTileIdForAscii(char character) {
    const auto value = static_cast<unsigned char>(character);
    return value >= 32 && value <= 126 ? value : 0;
}

constexpr bool dialoguePaletteAttribute(
    Actor speaker,
    std::uint8_t& attribute) {
    if (speaker == Actor::X || speaker == Actor::Vile) {
        attribute = 0;
        return true;
    }
    if (speaker == Actor::Zero) {
        attribute = 1;
        return true;
    }
    return false;
}

constexpr Plan evaluate(int sourceFrame) {
    Plan plan;
    plan.sourceFrame = sourceFrame;
    for (const auto& interval : kPhases) {
        if (interval.frames.contains(sourceFrame)) {
            plan.phase = interval.phase;
            break;
        }
    }

    if (kXVisible.contains(sourceFrame)) {
        plan.actors = plan.actors | Actor::X;
        plan.xVisible = true;
    }
    if (kVileVisible.contains(sourceFrame)) {
        plan.actors = plan.actors | Actor::Vile;
    }
    if (kZeroVisible.contains(sourceFrame)) {
        plan.actors = plan.actors | Actor::Zero;
    }

    if (kFirstSpeakerOverlay.contains(sourceFrame)) {
        plan.dialogue = plan.dialogue | DialogueOverlay::FirstSpeaker;
    }
    if (kXPortraitOverlay.contains(sourceFrame)) {
        plan.dialogue = plan.dialogue | DialogueOverlay::XPortrait;
    }
    if (kZeroPortraitOverlay.contains(sourceFrame)) {
        plan.dialogue = plan.dialogue | DialogueOverlay::ZeroPortrait;
    }
    plan.dialoguePanelVisible =
        kProvenFirstDialoguePanelVisible.contains(sourceFrame) ||
        kProvenSecondDialoguePanelVisible.contains(sourceFrame);

    plan.controlsLocked =
        kProvenInputLock.contains(sourceFrame) ||
        kProvenLateGameplayControlLock.contains(sourceFrame);
    plan.heldActionAcceleratesDialogue =
        kProvenDialogueActionWindow.contains(sourceFrame) ||
        kProvenSecondDialogueActionWindow.contains(sourceFrame) ||
        kProvenSecondDialogueHeldPageActionWindow.contains(sourceFrame) ||
        kProvenLateDialogueActionWindow.contains(sourceFrame);
    plan.xWarpOut = plan.phase == Phase::XWarpOut;
    return plan;
}

}  // namespace mmx::intro_highway_story_timeline
