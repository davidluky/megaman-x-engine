// cp_capsule_cutscene.h - declares the Chill Penguin capsule cutscene model.
// Owns: public cutscene state, frame events, and source-pose contracts.

#pragma once

#include "gameplay/capsule_cutscene.h"

#include <optional>
#include <string>

namespace mmx {

struct CpCapsulePoint {
    int x = 0;
    int y = 0;
};

struct CpCapsuleDialogLine {
    int row = 0;
    const char* text = "";
};

struct CpCapsuleDialogSnapshot {
    int sourceFrame = 0;
    int movieFrame = 0;
    const CpCapsuleDialogLine* lines = nullptr;
    int lineCount = 0;
};

struct CpCapsuleSourcePlane {
    int sourceFrame = 0;
    int movieFrame = 0;
    const char* dialogWindowMaskPath = "";
    const char* bg3TextLayerPath = "";
    const char* portraitBoxPath = "";
};

struct CpCapsuleSourceObjComposite {
    int sourceFrame = 0;
    int movieFrame = 0;
    int atlasPage = 0;
    int atlasX = 0;
    int atlasY = 0;
    int width = 0;
    int height = 0;
    int screenX = 0;
    int screenY = 0;
};

class CpCapsuleCutscene {
public:
    struct TickEvents {
        bool grantBoots = false;
        bool releaseControls = false;
    };

    // Source-local frames from U244's accepted CP capsule trace.
    static constexpr int kPreMovementAnchorStartFrame = 1;
    static constexpr int kPreMovementAnchorEndFrame = 1522;
    static constexpr int kFirstPlayerMotionFrame = 1523;
    static constexpr int kPedestalLockStartFrame = 2177;
    static constexpr int kPedestalLockEndFrame = 2718;
    static constexpr int kBootsGrantFrame = 2496;
    static constexpr int kFirstPostGrantMotionFrame = 2719;

    static constexpr CpCapsulePoint kPreMovementAnchor{2488, 657};
    static constexpr CpCapsulePoint kPedestalAnchor{2528, 639};
    static constexpr CpCapsulePoint kPostGrantMotionAnchor{2529, 639};

    void start(std::string persistentPickupId);
    void reset();
    TickEvents tick();

    bool active() const { return lifecycle_.active(); }
    bool controlsLocked() const { return lifecycle_.controlsLocked(); }
    int frame() const { return lifecycle_.frame(); }
    const std::string& persistentPickupId() const {
        return lifecycle_.persistentPickupId();
    }

    std::optional<CpCapsulePoint> forcedPlayerAnchor() const;

    static std::optional<CpCapsulePoint> playerAnchorForFrame(int sourceFrame);
    static std::optional<int> playerAnimByteForFrame(int sourceFrame);
    static const CpCapsuleDialogSnapshot& dialogSnapshotForFrame(int sourceFrame);
    static const CpCapsuleSourcePlane& sourcePlaneForFrame(int sourceFrame);
    static const CpCapsuleSourceObjComposite& sourceObjCompositeForFrame(int sourceFrame);
    static const char* sourceObjAtlasPath(int atlasPage);
    const CpCapsuleDialogSnapshot& dialogSnapshot() const;
    const CpCapsuleSourcePlane& sourcePlane() const;
    const CpCapsuleSourceObjComposite& sourceObjComposite() const;

private:
    CapsuleCutscene lifecycle_{CapsuleCutsceneDefinition{
        ArmorUpgradePart::Boots,
        kBootsGrantFrame,
        kFirstPostGrantMotionFrame,
    }};
};

} // namespace mmx
