// storm_eagle_capsule_cutscene.h - source-timed Storm Eagle helmet capsule model.
// Owns: FU7 timeline access and helmet lifecycle events; no stage/render wiring.

#pragma once

#include "gameplay/capsule_cutscene.h"

#include <optional>
#include <string>

namespace mmx {

struct StormEagleCapsulePoint {
    int x = 0;
    int y = 0;
};

struct StormEagleCapsuleSourceFrame {
    int sourceFrame = 0;
    int globalFrame = 0;
    const char* phase = "";
    int playerWorldX = 0;
    int playerWorldY = 0;
    int playerAnimByte = -1;
    bool helmetOwned = false;
    const char* characterFrameId = "";
    const char* objectCompositeId = "";
    const char* bg3LayerId = "";
    const char* portraitFrameId = "";
};

struct StormEagleCapsuleVisualRegion {
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

struct StormEagleCapsuleVisualFrame {
    int sourceFrame = 0;
    int globalFrame = 0;
    StormEagleCapsuleVisualRegion character;
    StormEagleCapsuleVisualRegion object;
    StormEagleCapsuleVisualRegion bg3;
    StormEagleCapsuleVisualRegion portrait;
};

struct StormEagleCapsuleHardwareVisualFrame {
    int sourceFrame = 0;
    int runtimeGlobalFrame = 0;
    int materialGlobalFrame = 0;
    StormEagleCapsuleVisualRegion composite;
};

class StormEagleCapsuleCutscene {
public:
    struct TickEvents {
        bool grantHelmet = false;
        bool releaseControls = false;
    };

    static constexpr int kFirstGlobalFrame = 7165;
    static constexpr int kLastGlobalFrame = 8303;
    static constexpr int kSourceFrameCount = 1139;
    static constexpr int kHelmetGrantGlobalFrame = 8080;
    static constexpr int kHelmetGrantFrame = 916;
    static constexpr int kFirstPostGrantMotionGlobalFrame = 8303;
    static constexpr int kFirstPostGrantMotionFrame = 1139;

    void start(std::string persistentPickupId);
    void reset();
    TickEvents tick();

    bool active() const { return lifecycle_.active(); }
    bool controlsLocked() const { return lifecycle_.controlsLocked(); }
    int frame() const { return lifecycle_.frame(); }
    const std::string& persistentPickupId() const {
        return lifecycle_.persistentPickupId();
    }

    static const StormEagleCapsuleSourceFrame& sourceStateForFrame(int sourceFrame);
    static const StormEagleCapsuleVisualFrame& visualStateForFrame(int sourceFrame);
    static const char* characterAtlasPath(int atlasPage);
    static const char* objectAtlasPath(int atlasPage);
    static const char* bg3AtlasPath(int atlasPage);
    static const char* portraitAtlasPath(int atlasPage);
    static const StormEagleCapsuleHardwareVisualFrame*
        hardwareVisualForFrame(int sourceFrame);
    static const char* hardwareObjAtlasPath(int atlasPage);
    static std::optional<StormEagleCapsulePoint> playerAnchorForFrame(int sourceFrame);
    static std::optional<int> playerAnimByteForFrame(int sourceFrame);
    std::optional<StormEagleCapsulePoint> forcedPlayerAnchor() const;
    const StormEagleCapsuleSourceFrame& sourceState() const;
    const StormEagleCapsuleVisualFrame& visualState() const;

private:
    CapsuleCutscene lifecycle_{CapsuleCutsceneDefinition{
        ArmorUpgradePart::Helmet,
        kHelmetGrantFrame,
        kFirstPostGrantMotionFrame,
    }};
};

} // namespace mmx
