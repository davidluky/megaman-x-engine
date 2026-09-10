// capsule_cutscene.h - shared, data-defined capsule lifecycle state machine.
// Boundary: owns timing/event state only; capsule-specific source visuals stay
// in their evidence-backed adapters.

#pragma once

#include <string>
#include <utility>

namespace mmx {

enum class ArmorUpgradePart {
    Boots,
    Helmet,
    Body,
    Buster,
};

struct CapsuleCutsceneDefinition {
    ArmorUpgradePart upgradePart = ArmorUpgradePart::Boots;
    int grantFrame = 0;
    int releaseFrame = 0;
};

class CapsuleCutscene {
public:
    struct TickEvents {
        bool grantUpgrade = false;
        bool releaseControls = false;
        ArmorUpgradePart upgradePart = ArmorUpgradePart::Boots;
    };

    explicit CapsuleCutscene(CapsuleCutsceneDefinition definition)
        : definition_(definition) {}

    void start(std::string persistentPickupId) {
        active_ = true;
        sourceFrame_ = 0;
        persistentPickupId_ = std::move(persistentPickupId);
    }

    void reset() {
        active_ = false;
        sourceFrame_ = 0;
        persistentPickupId_.clear();
    }

    TickEvents tick() {
        TickEvents events;
        events.upgradePart = definition_.upgradePart;
        if (!active_) {
            return events;
        }

        ++sourceFrame_;
        events.grantUpgrade = sourceFrame_ == definition_.grantFrame;
        events.releaseControls = sourceFrame_ == definition_.releaseFrame;
        if (events.releaseControls) {
            active_ = false;
        }
        return events;
    }

    const CapsuleCutsceneDefinition& definition() const { return definition_; }
    bool active() const { return active_; }
    bool controlsLocked() const { return active_; }
    int frame() const { return sourceFrame_; }
    const std::string& persistentPickupId() const { return persistentPickupId_; }

private:
    CapsuleCutsceneDefinition definition_;
    bool active_ = false;
    int sourceFrame_ = 0;
    std::string persistentPickupId_;
};

} // namespace mmx
