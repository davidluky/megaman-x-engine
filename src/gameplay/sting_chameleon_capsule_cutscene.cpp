// sting_chameleon_capsule_cutscene.cpp - generated FU7-T25B model adapter.

#include "gameplay/sting_chameleon_capsule_cutscene.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace mmx {
namespace {

#include "generated/sting_chameleon_capsule/sting_chameleon_capsule_timeline.inc"
#include "generated/sting_chameleon_capsule/sting_chameleon_capsule_hardware_visuals.inc"
#include "generated/sting_chameleon_capsule/sting_chameleon_capsule_arena_backdrop.inc"

template <typename T, std::size_t N>
constexpr int tableSize(const T (&)[N]) {
    return static_cast<int>(N);
}

template <typename T, std::size_t N>
const T* tableItem(const T (&table)[N], int index) {
    return index >= 0 && index < static_cast<int>(N) ? &table[index] : nullptr;
}

template <std::size_t N>
const char* atlasPath(const char* const (&paths)[N], int index) {
    return index >= 0 && index < static_cast<int>(N) ? paths[index] : "";
}

int clampLocalFrame(int localFrame) {
    return std::clamp(
        localFrame,
        StingChameleonCapsuleCutscene::kFirstLocalFrame,
        StingChameleonCapsuleCutscene::kLastLocalFrame);
}

} // namespace

void StingChameleonCapsuleCutscene::start(std::string persistentPickupId) {
    lifecycle_.start(std::move(persistentPickupId));
}

void StingChameleonCapsuleCutscene::reset() {
    lifecycle_.reset();
}

StingChameleonCapsuleCutscene::TickEvents
StingChameleonCapsuleCutscene::tick() {
    const auto events = lifecycle_.tick();
    return {events.grantUpgrade, events.releaseControls};
}

int StingChameleonCapsuleCutscene::sourceRouteFrameForLocalFrame(int localFrame) {
    return kFirstSourceRouteFrame + clampLocalFrame(localFrame);
}

int StingChameleonCapsuleCutscene::localFrameForSourceRouteFrame(int sourceFrame) {
    return std::clamp(sourceFrame, kFirstSourceRouteFrame, kLastSourceRouteFrame)
        - kFirstSourceRouteFrame;
}

const char* StingChameleonCapsuleCutscene::phaseForFrame(int localFrame) {
    const int frame = clampLocalFrame(localFrame);
    for (const auto& phase : kStingPhaseWindows) {
        if (frame >= phase.localFirstFrame && frame <= phase.localLastFrame) {
            return phase.name;
        }
    }
    return "";
}

StingChameleonCapsuleDialogState
StingChameleonCapsuleCutscene::dialogStateForFrame(int localFrame) {
    const int frame = clampLocalFrame(localFrame);
    const int sourceFrame = sourceRouteFrameForLocalFrame(frame);
    StingChameleonCapsuleDialogState state;
    state.localFrame = frame;
    state.sourceFrame = sourceFrame;
    state.textTilemapNonempty = sourceFrame >= 409 && sourceFrame <= 1140;

    if (state.textTilemapNonempty) {
        state.page = sourceFrame <= 728 ? 1 : 2;
    }

    if (sourceFrame >= 662 && sourceFrame <= 697) {
        state.bg3VScroll = (sourceFrame - 661) * 2;
    } else if (sourceFrame >= 698 && sourceFrame <= 1021) {
        state.bg3VScroll = 72;
    } else if (sourceFrame >= 1022 && sourceFrame <= 1049) {
        state.bg3VScroll = 72 + (sourceFrame - 1021) * 2;
    } else if (sourceFrame >= 1050) {
        state.bg3VScroll = 128;
    }

    for (const auto& line : kStingDialogLines) {
        if (sourceFrame >= line.sourceCompleteFrame) {
            ++state.completedLineCount;
        }
    }

    for (const auto& page : kStingDialogPages) {
        if (sourceFrame >= page.sourceFirstCharacterFrame
            && sourceFrame <= page.sourceCompleteFrame) {
            state.characterRevealWindow = true;
        }
        if (sourceFrame >= page.sourcePromptFirstFrame
            && sourceFrame <= page.sourcePromptLastFrame) {
            state.promptObservationWindow = true;
        }
    }

    for (const auto& group : kStingTextBlipGroups) {
        if (sourceFrame >= group.sourceFirstFrame
            && sourceFrame <= group.sourceLastFrame) {
            state.textBlipGroupWindow = group.groupIndex;
            break;
        }
    }
    return state;
}

const StingChameleonCapsuleVisualFrame&
StingChameleonCapsuleCutscene::visualStateForFrame(int localFrame) {
    return kStingChameleonCapsuleVisualFrames[clampLocalFrame(localFrame)];
}

int StingChameleonCapsuleCutscene::arenaBackdropRegionCount() {
    return kStingChameleonCapsuleArenaBackdropRegionCount;
}

StingChameleonCapsuleArenaBackdrop
StingChameleonCapsuleCutscene::arenaBackdropForFrame(
    int regionIndex, int localFrame) {
    StingChameleonCapsuleArenaBackdrop result;
    if (regionIndex < 0 ||
        regionIndex >= kStingChameleonCapsuleArenaBackdropRegionCount) {
        return result;
    }
    const auto& region = *kStingChameleonCapsuleArenaBackdropRegions[regionIndex];
    if (localFrame < region.firstLocalFrame) {
        return result;
    }
    int cell = region.keys[0].cellIndex;
    for (int i = 0; i < region.keyCount; ++i) {
        if (localFrame < region.keys[i].fromLocalFrame) {
            break;
        }
        cell = region.keys[i].cellIndex;
    }
    result.atlasPath = region.atlasPath;
    result.atlasX = cell * region.width;
    result.width = region.width;
    result.height = region.height;
    result.screenX = region.screenX;
    result.screenY = region.screenY;
    return result;
}

StingChameleonCapsuleDialogWindow
StingChameleonCapsuleCutscene::dialogWindowForFrame(int localFrame) {
    StingChameleonCapsuleDialogWindow window;
    window.visible =
        localFrame >= kStingChameleonCapsuleDialogWindowFirstLocalFrame &&
        localFrame <= kStingChameleonCapsuleDialogWindowLastLocalFrame;
    window.x = kStingChameleonCapsuleDialogWindowX;
    window.y = kStingChameleonCapsuleDialogWindowY;
    window.width = kStingChameleonCapsuleDialogWindowW;
    window.height = kStingChameleonCapsuleDialogWindowH;
    return window;
}

const char* StingChameleonCapsuleCutscene::characterAtlasPath(int atlasPage) {
    return atlasPath(kStingChameleonCapsuleCharacterAtlasPages, atlasPage);
}

const char* StingChameleonCapsuleCutscene::objectAtlasPath(int atlasPage) {
    return atlasPath(kStingChameleonCapsuleObjectAtlasPages, atlasPage);
}

const char* StingChameleonCapsuleCutscene::enhancementAtlasPath(int atlasPage) {
    return atlasPath(kStingChameleonCapsuleEnhancementAtlasPages, atlasPage);
}

const char* StingChameleonCapsuleCutscene::bg3AtlasPath(int atlasPage) {
    return atlasPath(kStingChameleonCapsuleBg3AtlasPages, atlasPage);
}

const char* StingChameleonCapsuleCutscene::portraitAtlasPath(int atlasPage) {
    return atlasPath(kStingChameleonCapsulePortraitAtlasPages, atlasPage);
}

bool StingChameleonCapsuleCutscene::textBlipEventForFrame(int localFrame) {
    return visualStateForFrame(localFrame).textBlipEvent;
}

bool StingChameleonCapsuleCutscene::promptVisibleForFrame(int localFrame) {
    return visualStateForFrame(localFrame).promptVisible;
}

int StingChameleonCapsuleCutscene::postReleaseSourceXRaw(int sourceFrame) {
    const int frame = std::clamp(
        sourceFrame,
        kControlReleaseSourceFrame,
        kLastProvenPostReleaseSourceFrame);
    if (frame <= 1733) {
        return kReleaseSourceXRaw
            + (frame - kControlReleaseSourceFrame) * 256;
    }
    if (frame == 1734) return 449645;
    if (frame <= 1742) return 449645 + (frame - 1734) * 376;
    return 452773;
}

int StingChameleonCapsuleCutscene::postReleaseDeltaXRaw(int sourceFrame) {
    if (sourceFrame <= kControlReleaseSourceFrame) return 0;
    return postReleaseSourceXRaw(sourceFrame)
        - postReleaseSourceXRaw(sourceFrame - 1);
}

float StingChameleonCapsuleCutscene::postReleaseEngineX(int sourceFrame) {
    return static_cast<float>(postReleaseSourceXRaw(sourceFrame)) / 256.0f
        + static_cast<float>(kSourceAnchorToEngineTopLeftX);
}

float StingChameleonCapsuleCutscene::postReleaseEngineY() {
    return static_cast<float>(
        kPedestalY + kSourceAnchorToEngineTopLeftY);
}

int StingChameleonCapsuleCutscene::dialogLineCount() {
    return tableSize(kStingDialogLines);
}

const StingChameleonCapsuleDialogLine*
StingChameleonCapsuleCutscene::dialogLine(int index) {
    return tableItem(kStingDialogLines, index);
}

int StingChameleonCapsuleCutscene::dialogPageCount() {
    return tableSize(kStingDialogPages);
}

const StingChameleonCapsuleDialogPage*
StingChameleonCapsuleCutscene::dialogPage(int index) {
    return tableItem(kStingDialogPages, index);
}

int StingChameleonCapsuleCutscene::scrollTransitionCount() {
    return tableSize(kStingScrollTransitions);
}

const StingChameleonCapsuleScrollTransition*
StingChameleonCapsuleCutscene::scrollTransition(int index) {
    return tableItem(kStingScrollTransitions, index);
}

int StingChameleonCapsuleCutscene::textBlipGroupCount() {
    return tableSize(kStingTextBlipGroups);
}

const StingChameleonCapsuleTextBlipGroup*
StingChameleonCapsuleCutscene::textBlipGroup(int index) {
    return tableItem(kStingTextBlipGroups, index);
}

int StingChameleonCapsuleCutscene::rawMailboxCommandCount() {
    return tableSize(kStingRawMailboxCommands);
}

const StingChameleonCapsuleRawMailboxCommand*
StingChameleonCapsuleCutscene::rawMailboxCommand(int index) {
    return tableItem(kStingRawMailboxCommands, index);
}

} // namespace mmx
