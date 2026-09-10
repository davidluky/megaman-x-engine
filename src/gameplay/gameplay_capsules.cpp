// gameplay_capsules.cpp - runs armor-capsule lifecycle and source presentation.
// Owns: capsule eligibility, cutscene updates, grants, and capsule render adapters.

#include "gameplay_scene.h"

#include "systems/asset_cache.h"

namespace mmx {
namespace {

const TextureResource* loadPointTexture(const char* path) {
    if (!path || *path == '\0') return nullptr;
    const TextureResource* texture = AssetCache::loadTexture(path);
    if (texture && texture->valid()) {
        texture->setFilter(TEXTURE_FILTER_POINT);
        return texture;
    }
    return nullptr;
}

void drawSourcePlaneTexture(const char* path, float x, float y) {
    const TextureResource* texture = loadPointTexture(path);
    if (!texture) return;
    DrawTexture(texture->get(), static_cast<int>(x), static_cast<int>(y), WHITE);
}

void drawSourceAtlasTexture(const char* path, Rectangle source, float x, float y) {
    const TextureResource* texture = loadPointTexture(path);
    if (!texture) return;
    DrawTextureRec(texture->get(), source, {x, y}, WHITE);
}

} // namespace

bool GameplayScene::capsuleControlsLocked() const {
    return cpCapsule_.controlsLocked() || stormEagleCapsule_.controlsLocked() ||
           stingChameleonCapsule_.controlsLocked();
}

bool GameplayScene::shouldStartCpCapsuleCutscene(const Pickup& pickup) const {
    if (cpCapsule_.active() || stormEagleCapsule_.active() ||
        stingChameleonCapsule_.active()) return false;
    if (pickup.type != PickupType::ArmorCapsule || pickup.armorPart != "boots") {
        return false;
    }

    const std::string stage = activeStageId_.str();
    return stage == "chill-penguin" ||
           stagePath.find("chill-penguin") != std::string::npos;
}

void GameplayScene::startCpCapsuleCutscene(Pickup& pickup) {
    gameplay_cp_capsule::start(cpCapsule_, pickup, player_);
}

void GameplayScene::updateCpCapsuleCutscene(float /*dt*/) {
    const auto result = gameplay_cp_capsule::update(cpCapsule_, player_);

    updateCameraRoom();
    camera_.update(
        playerCameraAnchorX(),
        player_.position.y + player_.spriteHeight / 2,
        player_.facingRight
    );

    hud_.update(player_.health, player_.progressState().maxHealth, player_.lives);
    if (!player_.weaponInventory.isBuster()) {
        const Weapon& w = player_.weaponInventory.current();
        hud_.setWeaponEnergy(player_.weaponInventory.currentAmmo(), w.maxAmmo, w.gaugeColor, w.id);
    } else {
        hud_.hideWeaponEnergy();
    }

    writeProjTrace();
    writeParityTrace();
    gameplay_cp_capsule::finishTrace(cpCapsule_, result);
}

bool GameplayScene::shouldStartStormEagleCapsuleCutscene(const Pickup& pickup) const {
    if (stormEagleCapsule_.active() || cpCapsule_.active() ||
        stingChameleonCapsule_.active()) return false;
    if (pickup.type != PickupType::ArmorCapsule || pickup.armorPart != "helmet") {
        return false;
    }

    const std::string stage = activeStageId_.str();
    return stage == "storm-eagle" ||
           stagePath.find("storm-eagle") != std::string::npos;
}

void GameplayScene::startStormEagleCapsuleCutscene(Pickup& pickup) {
    gameplay_storm_eagle_capsule::start(stormEagleCapsule_, pickup, player_);
}

void GameplayScene::updateStormEagleCapsuleCutscene(float /*dt*/) {
    const auto result = gameplay_storm_eagle_capsule::update(
        stormEagleCapsule_, player_);

    updateCameraRoom();
    camera_.update(
        playerCameraAnchorX(),
        player_.position.y + player_.spriteHeight / 2,
        player_.facingRight
    );

    hud_.update(player_.health, player_.progressState().maxHealth, player_.lives);
    if (!player_.weaponInventory.isBuster()) {
        const Weapon& w = player_.weaponInventory.current();
        hud_.setWeaponEnergy(player_.weaponInventory.currentAmmo(), w.maxAmmo,
                             w.gaugeColor, w.id);
    } else {
        hud_.hideWeaponEnergy();
    }

    writeProjTrace();
    writeParityTrace();
    gameplay_storm_eagle_capsule::finishTrace(stormEagleCapsule_, result);
}

bool GameplayScene::shouldStartStingChameleonCapsuleCutscene(
    const Pickup& pickup) const {
    if (stingChameleonCapsule_.active() || cpCapsule_.active() ||
        stormEagleCapsule_.active()) return false;
    if (pickup.type != PickupType::ArmorCapsule || pickup.armorPart != "body") {
        return false;
    }

    const std::string stage = activeStageId_.str();
    return stage == "sting-chameleon" ||
           stagePath.find("sting-chameleon") != std::string::npos;
}

void GameplayScene::startStingChameleonCapsuleCutscene(Pickup& pickup) {
    gameplay_sting_chameleon_capsule::start(
        stingChameleonCapsule_, pickup, player_);
}

void GameplayScene::updateStingChameleonCapsuleCutscene(float /*dt*/) {
    const auto result = gameplay_sting_chameleon_capsule::update(
        stingChameleonCapsule_, player_);

    updateCameraRoom();
    camera_.update(
        playerCameraAnchorX(),
        player_.position.y + player_.spriteHeight / 2,
        player_.facingRight
    );

    hud_.update(player_.health, player_.progressState().maxHealth, player_.lives);
    if (!player_.weaponInventory.isBuster()) {
        const Weapon& w = player_.weaponInventory.current();
        hud_.setWeaponEnergy(player_.weaponInventory.currentAmmo(), w.maxAmmo,
                             w.gaugeColor, w.id);
    } else {
        hud_.hideWeaponEnergy();
    }

    writeProjTrace();
    writeParityTrace();
    gameplay_sting_chameleon_capsule::finishTrace(
        stingChameleonCapsule_, result);
}

const CpSourceObjForegroundRecord* GameplayScene::activeAutotestSourceObjForegroundRecord() const {
    const auto* record = cpSourceObjForegroundRecordForFrame(
        autotestSourceObjForegroundTarget_.c_str(), stageTimer_);
    if (!record) return nullptr;
    if (activeStageId_.str() != record->stageId) return nullptr;
    if (record->activeCameraSection && *record->activeCameraSection &&
        activeCameraSectionId_ != record->activeCameraSection) {
        return nullptr;
    }
    if (record->activeVisualSection && *record->activeVisualSection &&
        activeVisualSectionId_ != record->activeVisualSection) {
        return nullptr;
    }
    return record;
}

void GameplayScene::renderAutotestSourceObjForeground() const {
    const auto* obj = activeAutotestSourceObjForegroundRecord();
    if (!obj) return;

    const char* atlasPath = cpSourceObjForegroundAtlasPath(obj->atlasPage);
    if (!atlasPath || *atlasPath == '\0' || obj->width <= 0 || obj->height <= 0) {
        return;
    }
    drawSourceAtlasTexture(
        atlasPath,
        Rectangle{
            static_cast<float>(obj->atlasX),
            static_cast<float>(obj->atlasY),
            static_cast<float>(obj->width),
            static_cast<float>(obj->height),
        },
        static_cast<float>(obj->screenX),
        static_cast<float>(obj->screenY));
}
void GameplayScene::renderCpCapsuleSourceObj() const {
    if (!cpCapsule_.active()) return;

    const auto& obj = cpCapsule_.sourceObjComposite();
    const char* atlasPath = CpCapsuleCutscene::sourceObjAtlasPath(obj.atlasPage);
    if (!atlasPath || *atlasPath == '\0' || obj.width <= 0 || obj.height <= 0) {
        return;
    }
    drawSourceAtlasTexture(
        atlasPath,
        Rectangle{
            static_cast<float>(obj.atlasX),
            static_cast<float>(obj.atlasY),
            static_cast<float>(obj.width),
            static_cast<float>(obj.height),
        },
        static_cast<float>(obj.screenX),
        static_cast<float>(obj.screenY));
}

void GameplayScene::renderCpCapsuleCutscene() const {
    if (!cpCapsule_.active()) return;

    const auto& plane = cpCapsule_.sourcePlane();
    drawSourcePlaneTexture(plane.dialogWindowMaskPath, 0.0f, 0.0f);
    drawSourcePlaneTexture(plane.portraitBoxPath, 176.0f, 40.0f);
    drawSourcePlaneTexture(plane.bg3TextLayerPath, 0.0f, 0.0f);
}

void GameplayScene::renderStormEagleCapsuleSourceObj() const {
    if (!stormEagleCapsule_.visualActive()) return;

    const auto* hardwareVisual =
        StormEagleCapsuleCutscene::hardwareVisualForFrame(
            stormEagleCapsule_.sourceVisualFrame);
    if (hardwareVisual) {
        const auto& region = hardwareVisual->composite;
        drawSourceAtlasTexture(
            StormEagleCapsuleCutscene::hardwareObjAtlasPath(region.atlasPage),
            Rectangle{
                static_cast<float>(region.atlasX),
                static_cast<float>(region.atlasY),
                static_cast<float>(region.width),
                static_cast<float>(region.height),
            },
            static_cast<float>(region.screenX),
            static_cast<float>(region.screenY));
        return;
    }

    const auto& visual = gameplay_storm_eagle_capsule::visualState(stormEagleCapsule_);
    auto drawRegion = [](const char* atlasPath,
                         const StormEagleCapsuleVisualRegion& region) {
        if (!region.visible()) return;
        drawSourceAtlasTexture(
            atlasPath,
            Rectangle{
                static_cast<float>(region.atlasX),
                static_cast<float>(region.atlasY),
                static_cast<float>(region.width),
                static_cast<float>(region.height),
            },
            static_cast<float>(region.screenX),
            static_cast<float>(region.screenY));
    };
    drawRegion(
        StormEagleCapsuleCutscene::objectAtlasPath(visual.object.atlasPage),
        visual.object);
    drawRegion(
        StormEagleCapsuleCutscene::characterAtlasPath(visual.character.atlasPage),
        visual.character);
}

void GameplayScene::renderStormEagleCapsuleDialog() const {
    if (!stormEagleCapsule_.visualActive()) return;

    const auto& visual = gameplay_storm_eagle_capsule::visualState(stormEagleCapsule_);
    auto drawRegion = [](const char* atlasPath,
                         const StormEagleCapsuleVisualRegion& region) {
        if (!region.visible()) return;
        drawSourceAtlasTexture(
            atlasPath,
            Rectangle{
                static_cast<float>(region.atlasX),
                static_cast<float>(region.atlasY),
                static_cast<float>(region.width),
                static_cast<float>(region.height),
            },
            static_cast<float>(region.screenX),
            static_cast<float>(region.screenY));
    };
    drawRegion(
        StormEagleCapsuleCutscene::bg3AtlasPath(visual.bg3.atlasPage),
        visual.bg3);
    drawRegion(
        StormEagleCapsuleCutscene::portraitAtlasPath(visual.portrait.atlasPage),
        visual.portrait);
}

void GameplayScene::renderStingChameleonCapsulePresentation() const {
    if (stingChameleonCapsule_.visualActive()) {
        // Source-derived arena backdrop: crumbling rock wall + revealed sky
        // keyframes, then the black dialog window under the bg3 text layer.
        const int localFrame = stingChameleonCapsule_.sourceVisualFrame;
        const int regions =
            StingChameleonCapsuleCutscene::arenaBackdropRegionCount();
        for (int i = 0; i < regions; ++i) {
            const auto backdrop =
                StingChameleonCapsuleCutscene::arenaBackdropForFrame(
                    i, localFrame);
            if (backdrop.width <= 0) continue;
            drawSourceAtlasTexture(
                backdrop.atlasPath,
                Rectangle{
                    static_cast<float>(backdrop.atlasX),
                    0.0f,
                    static_cast<float>(backdrop.width),
                    static_cast<float>(backdrop.height),
                },
                static_cast<float>(backdrop.screenX),
                static_cast<float>(backdrop.screenY));
        }
        const auto window =
            StingChameleonCapsuleCutscene::dialogWindowForFrame(localFrame);
        if (window.visible) {
            DrawRectangle(window.x, window.y, window.width, window.height, BLACK);
        }
    }
    gameplay_sting_chameleon_capsule::renderPresentation(
        stingChameleonCapsule_,
        [](const char* atlasPath,
           const StingChameleonCapsuleVisualRegion& region) {
            if (!region.visible()) return;
            drawSourceAtlasTexture(
                atlasPath,
                Rectangle{
                    static_cast<float>(region.atlasX),
                    static_cast<float>(region.atlasY),
                    static_cast<float>(region.width),
                    static_cast<float>(region.height),
                },
                static_cast<float>(region.screenX),
                static_cast<float>(region.screenY));
        });
}

void GameplayScene::grantArmorCapsule(const Pickup& pickup) {
    if (pickup.armorPart == "boots") {
        player_.grantArmorBoots();
    } else if (pickup.armorPart == "helmet") {
        player_.grantArmorHelmet();
    } else if (pickup.armorPart == "body") {
        player_.grantArmorBody();
    } else if (pickup.armorPart == "buster") {
        player_.grantArmorBuster();
    }
}

} // namespace mmx
