// ride_armor_scene_adapter.cpp - maps scene collision signals to the model.

#include "gameplay/ride_armor_scene_adapter.h"

#include <algorithm>

namespace mmx {

void RideArmorSceneAdapter::spawn(int worldXQ8_8, int worldYQ8_8) {
    model_.park(worldXQ8_8, worldYQ8_8);
    pilotAttached_ = false;
    punchTargetsHit_.clear();
}

RideArmorSceneFrame RideArmorSceneAdapter::tick(const RideArmorInput& input,
                                                bool onGround,
                                                bool mountContact) {
    RideArmorSceneFrame result;
    if (mountContact && !pilotAttached_ &&
        model_.phase() == RideArmorPhase::Parked) {
        result.mountStarted = model_.beginMount();
        pilotAttached_ = result.mountStarted;
    }

    result.model = model_.tick(input, onGround);
    if (result.model.punchStarted) {
        punchTargetsHit_.clear();
    }
    if (result.model.dismounted || result.model.objectRemoved) {
        result.pilotReleased = pilotAttached_;
        pilotAttached_ = false;
        punchTargetsHit_.clear();
    }

    result.pilotSynchronized = pilotAttached_;
    result.pilotXQ8_8 = model_.worldXQ8_8();
    result.pilotYQ8_8 = model_.worldYQ8_8();
    return result;
}

void RideArmorSceneAdapter::resolveCollision(int worldXQ8_8,
                                             int worldYQ8_8) {
    model_.resolveWorldPosition(worldXQ8_8, worldYQ8_8);
}

RideArmorDamageResult
RideArmorSceneAdapter::routeIncomingDamage(int amount) {
    auto result = model_.takeIncomingDamage(amount);
    if (result.pilotEjected) {
        pilotAttached_ = false;
        punchTargetsHit_.clear();
    }
    return result;
}

bool RideArmorSceneAdapter::targetAlreadyHit(int targetSerial) const {
    return std::find(punchTargetsHit_.begin(), punchTargetsHit_.end(),
                     targetSerial) != punchTargetsHit_.end();
}

RideArmorPunchContact
RideArmorSceneAdapter::routePunchContact(int targetSerial,
                                         bool overlapsAttackObject,
                                         bool targetDamageable) {
    RideArmorPunchContact result;
    result.targetSerial = targetSerial;
    if (!pilotAttached_ || !model_.punchActive() ||
        !overlapsAttackObject || !targetDamageable ||
        targetAlreadyHit(targetSerial)) {
        return result;
    }

    punchTargetsHit_.push_back(targetSerial);
    result.accepted = true;
    result.damage = RideArmorModel::kPunchDamage;
    return result;
}

} // namespace mmx
