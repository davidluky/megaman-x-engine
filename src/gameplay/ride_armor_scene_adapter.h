// ride_armor_scene_adapter.h - collision-port adapter for the Ride Armor.
// Geometry remains scene-owned: callers submit contact booleans and feed
// resolved world positions back after terrain collision.

#pragma once

#include "entities/ride_armor_model.h"

#include <vector>

namespace mmx {

struct RideArmorSceneFrame {
    RideArmorFrameResult model;
    bool mountStarted = false;
    bool pilotSynchronized = false;
    bool pilotReleased = false;
    int pilotXQ8_8 = 0;
    int pilotYQ8_8 = 0;
};

struct RideArmorPunchContact {
    bool accepted = false;
    int targetSerial = -1;
    int damage = 0;
};

class RideArmorSceneAdapter {
public:
    void spawn(int worldXQ8_8, int worldYQ8_8);

    // mountContact, onGround, and punch overlap are deliberate scene inputs.
    // The adapter does not manufacture collision rectangles from the
    // retracted 52x56 temporary-hazard claim.
    RideArmorSceneFrame tick(const RideArmorInput& input,
                             bool onGround,
                             bool mountContact);
    void resolveCollision(int worldXQ8_8, int worldYQ8_8);
    RideArmorDamageResult routeIncomingDamage(int amount);
    RideArmorPunchContact routePunchContact(int targetSerial,
                                            bool overlapsAttackObject,
                                            bool targetDamageable);

    const RideArmorModel& model() const { return model_; }
    bool pilotAttached() const { return pilotAttached_; }
    bool active() const { return model_.active(); }

private:
    bool targetAlreadyHit(int targetSerial) const;

    RideArmorModel model_;
    bool pilotAttached_ = false;
    std::vector<int> punchTargetsHit_;
};

} // namespace mmx
