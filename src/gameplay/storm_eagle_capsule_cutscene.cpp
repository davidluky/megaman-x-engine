// storm_eagle_capsule_cutscene.cpp - generated FU7 timeline adapter.

#include "gameplay/storm_eagle_capsule_cutscene.h"

#include <utility>

namespace mmx {
namespace {

#include "generated/storm_eagle_capsule/storm_eagle_capsule_timeline.inc"
#include "generated/storm_eagle_capsule/storm_eagle_capsule_visuals.inc"
#include "generated/storm_eagle_capsule/storm_eagle_capsule_hardware_visuals.inc"

} // namespace

void StormEagleCapsuleCutscene::start(std::string persistentPickupId) {
    lifecycle_.start(std::move(persistentPickupId));
}

void StormEagleCapsuleCutscene::reset() {
    lifecycle_.reset();
}

StormEagleCapsuleCutscene::TickEvents StormEagleCapsuleCutscene::tick() {
    const auto lifecycleEvents = lifecycle_.tick();
    return {
        lifecycleEvents.grantUpgrade,
        lifecycleEvents.releaseControls,
    };
}

const StormEagleCapsuleSourceFrame&
StormEagleCapsuleCutscene::sourceStateForFrame(int sourceFrame) {
    if (sourceFrame <= 1) {
        return kStormEagleCapsuleSourceFrames[0];
    }
    if (sourceFrame >= kSourceFrameCount) {
        return kStormEagleCapsuleSourceFrames[kSourceFrameCount - 1];
    }
    return kStormEagleCapsuleSourceFrames[sourceFrame - 1];
}

const StormEagleCapsuleVisualFrame&
StormEagleCapsuleCutscene::visualStateForFrame(int sourceFrame) {
    if (sourceFrame <= 1) {
        return kStormEagleCapsuleVisualFrames[0];
    }
    if (sourceFrame >= kSourceFrameCount) {
        return kStormEagleCapsuleVisualFrames[kSourceFrameCount - 1];
    }
    return kStormEagleCapsuleVisualFrames[sourceFrame - 1];
}

const char* StormEagleCapsuleCutscene::characterAtlasPath(int atlasPage) {
    const int count = static_cast<int>(
        sizeof(kStormEagleCapsuleCharacterAtlasPages) /
        sizeof(kStormEagleCapsuleCharacterAtlasPages[0]));
    return atlasPage >= 0 && atlasPage < count
        ? kStormEagleCapsuleCharacterAtlasPages[atlasPage]
        : "";
}

const char* StormEagleCapsuleCutscene::objectAtlasPath(int atlasPage) {
    const int count = static_cast<int>(
        sizeof(kStormEagleCapsuleObjectAtlasPages) /
        sizeof(kStormEagleCapsuleObjectAtlasPages[0]));
    return atlasPage >= 0 && atlasPage < count
        ? kStormEagleCapsuleObjectAtlasPages[atlasPage]
        : "";
}

const char* StormEagleCapsuleCutscene::bg3AtlasPath(int atlasPage) {
    const int count = static_cast<int>(
        sizeof(kStormEagleCapsuleBg3AtlasPages) /
        sizeof(kStormEagleCapsuleBg3AtlasPages[0]));
    return atlasPage >= 0 && atlasPage < count
        ? kStormEagleCapsuleBg3AtlasPages[atlasPage]
        : "";
}

const char* StormEagleCapsuleCutscene::portraitAtlasPath(int atlasPage) {
    const int count = static_cast<int>(
        sizeof(kStormEagleCapsulePortraitAtlasPages) /
        sizeof(kStormEagleCapsulePortraitAtlasPages[0]));
    return atlasPage >= 0 && atlasPage < count
        ? kStormEagleCapsulePortraitAtlasPages[atlasPage]
        : "";
}

const StormEagleCapsuleHardwareVisualFrame*
StormEagleCapsuleCutscene::hardwareVisualForFrame(int sourceFrame) {
    const int index = sourceFrame - kStormEagleCapsuleHardwareFirstSourceFrame;
    if (index < 0 || index >= kStormEagleCapsuleHardwareVisualFrameCount) {
        return nullptr;
    }
    return &kStormEagleCapsuleHardwareVisualFrames[index];
}

const char* StormEagleCapsuleCutscene::hardwareObjAtlasPath(int atlasPage) {
    const int count = static_cast<int>(
        sizeof(kStormEagleCapsuleHardwareObjAtlasPages) /
        sizeof(kStormEagleCapsuleHardwareObjAtlasPages[0]));
    return atlasPage >= 0 && atlasPage < count
        ? kStormEagleCapsuleHardwareObjAtlasPages[atlasPage]
        : "";
}

std::optional<StormEagleCapsulePoint>
StormEagleCapsuleCutscene::playerAnchorForFrame(int sourceFrame) {
    const auto& state = sourceStateForFrame(sourceFrame);
    return StormEagleCapsulePoint{state.playerWorldX, state.playerWorldY};
}

std::optional<int> StormEagleCapsuleCutscene::playerAnimByteForFrame(int sourceFrame) {
    return sourceStateForFrame(sourceFrame).playerAnimByte;
}

std::optional<StormEagleCapsulePoint>
StormEagleCapsuleCutscene::forcedPlayerAnchor() const {
    if (!active()) {
        return std::nullopt;
    }
    return playerAnchorForFrame(frame());
}

const StormEagleCapsuleSourceFrame& StormEagleCapsuleCutscene::sourceState() const {
    return sourceStateForFrame(frame());
}

const StormEagleCapsuleVisualFrame& StormEagleCapsuleCutscene::visualState() const {
    return visualStateForFrame(frame());
}

} // namespace mmx
