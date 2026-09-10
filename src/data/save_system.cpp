// save_system.cpp - persists player progress, unlocks, stats, and inventory.
// Owns: save-file schema sanitation and the active in-memory save binding.

#include "data/save_system.h"
#include "data/json_io.h"
#include "entities/player.h"
#include "systems/weapon.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <optional>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace mmx {

SaveSystemState SaveSystem::fallbackState_;
SaveSystemState* SaveSystem::activeState_ = &SaveSystem::fallbackState_;
WeaponInventoryState* SaveSystem::activeWeaponInventoryState_ = nullptr;
static const fs::path& saveRoot() {
    static const fs::path root = fs::path("saves");
    return root;
}

static std::optional<Weapon> weaponFromId(const std::string& id) {
    return weapons::makeById(WeaponId::fromString(id));
}

static int clampAmmoForWeapon(int ammo, const Weapon& weapon) {
    return std::clamp(ammo, 0, weapon.maxAmmo);
}

static int clampNonNegative(int value) {
    return std::max(0, value);
}

static int sanitizedIntField(const json& root, const char* key, int fallback, bool* changed) {
    const auto it = root.find(key);
    if (it == root.end()) {
        return fallback;
    }
    if (!it->is_number_integer()) {
        *changed = true;
        return fallback;
    }
    return it->get<int>();
}

static bool sanitizedBoolField(const json& root, const char* key, bool fallback, bool* changed) {
    const auto it = root.find(key);
    if (it == root.end()) {
        return fallback;
    }
    if (!it->is_boolean()) {
        *changed = true;
        return fallback;
    }
    return it->get<bool>();
}

static bool readSupportedFormatVersion(
    const json& root,
    const char* key,
    int currentVersion,
    int* outVersion
) {
    *outVersion = 0;
    const auto it = root.find(key);
    if (it == root.end()) {
        return true;
    }
    if (!it->is_number_integer()) {
        return false;
    }
    const int version = it->get<int>();
    if (version < 0 || version > currentVersion) {
        return false;
    }
    *outVersion = version;
    return true;
}

static bool isSafeIdToken(const std::string& value) {
    if (value.empty() || value.size() > 128) return false;
    for (unsigned char c : value) {
        if (!std::isalnum(c) && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

static bool isSafePickupToken(const std::string& value) {
    if (value.empty() || value.size() > 512) return false;
    if (value.find("..") != std::string::npos) return false;
    for (unsigned char c : value) {
        if (!std::isalnum(c) && c != '-' && c != '_' && c != ':' && c != '/' && c != '.') {
            return false;
        }
    }
    return true;
}

static bool isReservedWindowsDeviceName(const std::string& value) {
    const std::string baseName = value.substr(0, value.find('.'));
    std::string upper;
    upper.reserve(baseName.size());
    for (unsigned char c : baseName) {
        upper.push_back(static_cast<char>(std::toupper(c)));
    }

    if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL") {
        return true;
    }
    if (upper.size() == 4 &&
        (upper.rfind("COM", 0) == 0 || upper.rfind("LPT", 0) == 0) &&
        upper[3] >= '1' && upper[3] <= '9') {
        return true;
    }
    return false;
}

static bool isSafeSaveSlotName(const std::string& value) {
    return !value.empty() && value.size() <= 64 && isSafeIdToken(value) && !isReservedWindowsDeviceName(value);
}

SaveSlot SaveSlot::autosave() {
    return SaveSlot(saveRoot() / "autosave.json");
}

std::optional<SaveSlot> SaveSlot::numbered(int index) {
    if (index < 1 || index > SaveSystem::kSaveSlotCount) {
        return std::nullopt;
    }
    if (index == 1) {
        return autosave();
    }
    return named("slot" + std::to_string(index));
}

std::optional<SaveSlot> SaveSlot::named(const std::string& name) {
    if (!isSafeSaveSlotName(name)) {
        return std::nullopt;
    }
    return SaveSlot(saveRoot() / (name + ".json"));
}

static std::vector<std::string> sanitizedStringArray(
    const json& root,
    const char* key,
    bool* changed,
    size_t maxEntries = 512,
    bool allowPickupPathToken = false
) {
    std::vector<std::string> result;
    if (!root.contains(key)) return result;
    if (!root[key].is_array()) {
        *changed = true;
        return result;
    }

    for (const auto& value : root[key]) {
        if (result.size() >= maxEntries) {
            *changed = true;
            break;
        }
        if (!value.is_string()) {
            *changed = true;
            continue;
        }
        const std::string id = value.get<std::string>();
        const bool safe = allowPickupPathToken ? isSafePickupToken(id) : isSafeIdToken(id);
        if (!safe) {
            *changed = true;
            continue;
        }
        if (std::find(result.begin(), result.end(), id) != result.end()) {
            *changed = true;
            continue;
        }
        result.push_back(id);
    }
    return result;
}

static std::vector<std::string> sanitizedStringVector(
    const std::vector<std::string>& values,
    size_t maxEntries,
    bool allowPickupPathToken = false
) {
    std::vector<std::string> result;
    for (const std::string& value : values) {
        if (result.size() >= maxEntries) {
            break;
        }
        const bool safe = allowPickupPathToken ? isSafePickupToken(value) : isSafeIdToken(value);
        if (!safe) {
            continue;
        }
        if (std::find(result.begin(), result.end(), value) != result.end()) {
            continue;
        }
        result.push_back(value);
    }
    return result;
}

void SaveSystem::bindState(SaveSystemState& newState) {
    activeState_ = &newState;
}

void SaveSystem::useFallbackState() {
    activeState_ = &fallbackState_;
}

void SaveSystem::bindWeaponInventoryState(WeaponInventoryState& newState) {
    activeWeaponInventoryState_ = &newState;
}

void SaveSystem::useFallbackWeaponInventoryState() {
    activeWeaponInventoryState_ = nullptr;
}

WeaponInventory SaveSystem::weaponInventory() {
    if (activeWeaponInventoryState_) {
        return WeaponInventory(*activeWeaponInventoryState_);
    }
    return WeaponInventory();
}

SaveSystemState& SaveSystem::state() {
    return *activeState_;
}
void SaveSystem::resetLoadedState() {
    Player::resetPersistentState();
    state().completedStages.clear();
    state().defeatedBosses.clear();
    state().bestTimes.clear();
    state().totalDeaths = 0;
    state().totalPlayFrames = 0;
    state().enemiesDefeated = 0;
    state().bossesDefeated = 0;
    state().sigmaDefeated = false;
    WeaponInventory inv = weaponInventory();
    inv.init();
}

bool SaveSystem::setActiveSlotIndex(int index) {
    if (!SaveSlot::numbered(index).has_value()) {
        return false;
    }
    state().activeSlotIndex = index;
    return true;
}

int SaveSystem::activeSlotIndex() {
    return state().activeSlotIndex;
}

SaveSlot SaveSystem::activeSlot() {
    auto slot = SaveSlot::numbered(state().activeSlotIndex);
    if (!slot.has_value()) {
        state().activeSlotIndex = 1;
        return SaveSlot::autosave();
    }
    return *slot;
}

bool SaveSystem::save() {
    return save(activeSlot());
}

bool SaveSystem::load() {
    return load(activeSlot());
}

void SaveSystem::importData(const SaveData& data) {
    const int activeSlot = state().activeSlotIndex;
    resetLoadedState();
    state().activeSlotIndex = activeSlot;

    PlayerProgress playerProgress;
    playerProgress.maxHealth = std::clamp(data.maxHealth, 1, 32);
    playerProgress.armorBoots = data.armorBoots;
    playerProgress.armorHelmet = data.armorHelmet;
    playerProgress.armorBody = data.armorBody;
    playerProgress.armorBuster = data.armorBuster;
    for (int i = 0; i < 4; ++i) {
        playerProgress.subTanks[i].collected = data.subTanks[i].collected;
        playerProgress.subTanks[i].health =
            std::clamp(data.subTanks[i].health, 0, playerProgress.maxHealth);
    }
    playerProgress.collectedPickups =
        sanitizedStringVector(data.collectedPickups, 512, true);
    playerProgress.persistentStateInitialized = true;

    state().completedStages = sanitizedStringVector(data.completedStages, 64);
    state().defeatedBosses = sanitizedStringVector(data.defeatedBosses, 64);
    state().totalDeaths = clampNonNegative(data.totalDeaths);
    state().totalPlayFrames = clampNonNegative(data.totalPlayFrames);
    state().enemiesDefeated = clampNonNegative(data.enemiesDefeated);
    state().bossesDefeated = clampNonNegative(data.bossesDefeated);
    state().sigmaDefeated = data.sigmaDefeated;
    state().bestTimes.clear();

    WeaponInventory inv = weaponInventory();
    inv.init();
    const auto& importWeapons = data.*(&SaveData::weapons);
    const auto& importAmmo = data.*(&SaveData::ammo);
    for (size_t i = 1; i < importWeapons.size(); ++i) {
        const std::string& weaponId = importWeapons[i].id;
        if (!isSafeIdToken(weaponId) || weaponId == "buster") {
            continue;
        }
        auto weapon = weaponFromId(weaponId);
        if (!weapon) {
            continue;
        }
        const size_t before = inv.weaponCount();
        inv.addWeapon(*weapon);
        if (inv.weaponCount() == before) {
            continue;
        }
        const size_t weaponIndex = inv.weaponCount() - 1;
        int ammo = weapon->maxAmmo;
        if (i < importAmmo.size()) {
            ammo = importAmmo[i];
        }
        inv.setAmmo(weaponIndex, ammo);
    }

    Player::applyProgress(playerProgress);
}

SaveData SaveSystem::exportData() {
    SaveData data;
    const PlayerProgress playerProgress = Player::captureProgress();
    data.maxHealth = playerProgress.maxHealth;
    data.armorBoots = playerProgress.armorBoots;
    data.armorHelmet = playerProgress.armorHelmet;
    data.armorBody = playerProgress.armorBody;
    data.armorBuster = playerProgress.armorBuster;
    for (int i = 0; i < 4; ++i) {
        data.subTanks[i] = {
            playerProgress.subTanks[i].collected,
            playerProgress.subTanks[i].health,
        };
    }
    data.collectedPickups = playerProgress.collectedPickups;
    data.completedStages = state().completedStages;
    data.defeatedBosses = state().defeatedBosses;

    WeaponInventory inventoryView = weaponInventory();
    for (std::size_t i = 0; i < inventoryView.weaponCount(); ++i) {
        data.weapons.push_back({inventoryView.weaponAt(i).id});
        data.ammo.push_back(inventoryView.ammoAt(i));
    }
    data.currentWeaponIndex = inventoryView.currentIndex;
    data.totalDeaths = state().totalDeaths;
    data.totalPlayFrames = state().totalPlayFrames;
    data.enemiesDefeated = state().enemiesDefeated;
    data.bossesDefeated = state().bossesDefeated;
    data.sigmaDefeated = state().sigmaDefeated;
    return data;
}

bool SaveSystem::exists() {
    return exists(activeSlot());
}

void SaveSystem::deleteSave() {
    deleteSave(activeSlot());
}

void SaveSystem::resetRuntimeState() {
    resetLoadedState();
}

void SaveSystem::resetProgressionForNewRun() {
    state().completedStages.clear();
    state().defeatedBosses.clear();
    state().bestTimes.clear();
    state().totalDeaths = 0;
    state().totalPlayFrames = 0;
    state().enemiesDefeated = 0;
    state().bossesDefeated = 0;
    state().sigmaDefeated = false;
}

std::optional<SaveSummary> SaveSystem::summarize(const SaveSlot& slot) {
    const fs::path savePath = slot.path();
    const auto read = json_io::readJsonObjectFromFile(savePath);
    if (!read.ok) {
        if (read.error != json_io::ReadError::Open) {
            TraceLog(LOG_WARNING, "SaveSystem: %s", read.message.c_str());
        }
        return std::nullopt;
    }
    const json& j = read.value;
    int saveFormatVersion = 0;
    if (!readSupportedFormatVersion(j, "schemaVersion", SaveSystem::kSaveFormatVersion,
                                    &saveFormatVersion)) {
        TraceLog(LOG_WARNING, "SaveSystem: unsupported schemaVersion in %s",
                 slot.pathString().c_str());
        return std::nullopt;
    }

    bool changed = false;
    SaveSummary summary;
    summary.exists = true;

    const int loadedMaxHealth = sanitizedIntField(j, "maxHealth", 16, &changed);
    summary.maxHealth = std::clamp(loadedMaxHealth, 1, 32);

    const auto stages = sanitizedStringArray(j, "completedStages", &changed, 64);
    summary.stagesCleared = static_cast<int>(stages.size());

    if (j.contains("stats") && j["stats"].is_object()) {
        const auto& stats = j["stats"];
        summary.totalDeaths = clampNonNegative(sanitizedIntField(stats, "totalDeaths", 0, &changed));
        summary.totalPlayFrames = clampNonNegative(sanitizedIntField(stats, "totalPlayFrames", 0, &changed));
        summary.enemiesDefeated = clampNonNegative(sanitizedIntField(stats, "enemiesDefeated", 0, &changed));
        summary.bossesDefeated = clampNonNegative(sanitizedIntField(stats, "bossesDefeated", 0, &changed));
        summary.sigmaDefeated = sanitizedBoolField(stats, "sigmaDefeated", false, &changed);
    }

    return summary;
}

bool SaveSystem::anySaveExists() {
    for (int i = 1; i <= kSaveSlotCount; ++i) {
        const auto slot = SaveSlot::numbered(i);
        if (slot.has_value() && summarize(*slot).has_value()) {
            return true;
        }
    }
    return false;
}

bool SaveSystem::save(const SaveSlot& slot) {
    json j;
    const PlayerProgress playerProgress = Player::captureProgress();

    j["schemaVersion"] = kSaveFormatVersion;
    j["maxHealth"] = playerProgress.maxHealth;

    j["armor"] = {
        {"boots",  playerProgress.armorBoots},
        {"helmet", playerProgress.armorHelmet},
        {"body",   playerProgress.armorBody},
        {"buster", playerProgress.armorBuster},
    };

    json tanks = json::array();
    for (int i = 0; i < 4; i++) {
        tanks.push_back({
            {"collected", playerProgress.subTanks[i].collected},
            {"health",    playerProgress.subTanks[i].health},
        });
    }
    j["subTanks"] = tanks;

    j["collectedPickups"] = playerProgress.collectedPickups;
    j["completedStages"] = state().completedStages;
    j["defeatedBosses"] = state().defeatedBosses;

    json weaponList = json::array();
    json ammoList = json::array();
    WeaponInventory inventoryView = weaponInventory();
    for (size_t i = 0; i < inventoryView.weaponCount(); i++) {
        weaponList.push_back(inventoryView.weaponAt(i).id);
        ammoList.push_back(inventoryView.ammoAt(i));
    }
    j["weapons"] = weaponList;
    j["ammo"] = ammoList;

    j["stats"] = {
        {"totalDeaths", state().totalDeaths},
        {"totalPlayFrames", state().totalPlayFrames},
        {"enemiesDefeated", state().enemiesDefeated},
        {"bossesDefeated", state().bossesDefeated},
        {"sigmaDefeated", state().sigmaDefeated},
    };

    json times = json::object();
    for (const auto& [stage, frames] : state().bestTimes) {
        times[stage] = frames;
    }
    j["bestTimes"] = times;

    const fs::path savePath = slot.path();
    return json_io::writeAtomically(savePath, j);
}

bool SaveSystem::load(const SaveSlot& slot) {
    const fs::path savePath = slot.path();
    const std::string savePathText = savePath.generic_string();
    const auto read = json_io::readJsonObjectFromFile(savePath);
    if (!read.ok) {
        if (read.error != json_io::ReadError::Open) {
            TraceLog(LOG_WARNING, "SaveSystem: %s", read.message.c_str());
        }
        if (read.error != json_io::ReadError::Open) {
            resetLoadedState();
        }
        return false;
    }
    const json& j = read.value;
    int saveFormatVersion = 0;
    if (!readSupportedFormatVersion(j, "schemaVersion", kSaveFormatVersion,
                                    &saveFormatVersion)) {
        TraceLog(LOG_WARNING, "SaveSystem: unsupported schemaVersion in %s",
                 savePathText.c_str());
        return false;
    }

    try {
        bool rewriteSanitizedSave = saveFormatVersion < kSaveFormatVersion;
        resetLoadedState();
        const int loadedMaxHealth = sanitizedIntField(j, "maxHealth", 16, &rewriteSanitizedSave);
        PlayerProgress playerProgress;
        playerProgress.maxHealth = std::clamp(loadedMaxHealth, 1, 32);
        if (playerProgress.maxHealth != loadedMaxHealth) {
            rewriteSanitizedSave = true;
        }

        if (j.contains("armor")) {
            auto& a = j["armor"];
            if (a.is_object()) {
                playerProgress.armorBoots =
                    sanitizedBoolField(a, "boots", false, &rewriteSanitizedSave);
                playerProgress.armorHelmet =
                    sanitizedBoolField(a, "helmet", false, &rewriteSanitizedSave);
                playerProgress.armorBody =
                    sanitizedBoolField(a, "body", false, &rewriteSanitizedSave);
                playerProgress.armorBuster =
                    sanitizedBoolField(a, "buster", false, &rewriteSanitizedSave);
            } else {
                rewriteSanitizedSave = true;
            }
        }

        if (j.contains("subTanks") && j["subTanks"].is_array()) {
            for (size_t i = 0; i < 4 && i < j["subTanks"].size(); i++) {
                if (!j["subTanks"][i].is_object()) {
                    rewriteSanitizedSave = true;
                    continue;
                }
                playerProgress.subTanks[i].collected =
                    sanitizedBoolField(j["subTanks"][i], "collected", false, &rewriteSanitizedSave);
                const int loadedTankHealth =
                    sanitizedIntField(j["subTanks"][i], "health", 0, &rewriteSanitizedSave);
                playerProgress.subTanks[i].health =
                    std::clamp(loadedTankHealth, 0, playerProgress.maxHealth);
                if (playerProgress.subTanks[i].health != loadedTankHealth) {
                    rewriteSanitizedSave = true;
                }
            }
            if (j["subTanks"].size() > 4) {
                rewriteSanitizedSave = true;
            }
        }

        playerProgress.collectedPickups =
            sanitizedStringArray(j, "collectedPickups", &rewriteSanitizedSave, 512, true);
        state().completedStages = sanitizedStringArray(j, "completedStages", &rewriteSanitizedSave, 64);

        state().defeatedBosses.clear();
        if (j.contains("defeatedBosses") && j["defeatedBosses"].is_array()) {
            state().defeatedBosses = sanitizedStringArray(j, "defeatedBosses", &rewriteSanitizedSave, 64);
        } else {
            // Backward-compatible migration for saves from before stage slots
            // and defeated bosses were separate progression identities.
            state().defeatedBosses = state().completedStages;
            if (!state().completedStages.empty()) {
                rewriteSanitizedSave = true;
            }
        }

        std::vector<int> savedAmmo;
        if (j.contains("ammo") && j["ammo"].is_array()) {
            for (const auto& ammoValue : j["ammo"]) {
                if (ammoValue.is_number_integer()) {
                    savedAmmo.push_back(ammoValue.get<int>());
                } else {
                    rewriteSanitizedSave = true;
                }
            }
        }

        WeaponInventory inv = weaponInventory();
        inv.init();
        if (!savedAmmo.empty() && savedAmmo[0] != 0) {
            rewriteSanitizedSave = true;
        }
        if (j.contains("weapons") && j["weapons"].is_array()) {
            for (size_t i = 1; i < j["weapons"].size(); i++) {
                if (!j["weapons"][i].is_string()) {
                    rewriteSanitizedSave = true;
                    continue;
                }
                std::string wid = j["weapons"][i].get<std::string>();
                if (wid == "buster") {
                    rewriteSanitizedSave = true;
                    continue;
                }

                auto weapon = weaponFromId(wid);
                if (!weapon) {
                    TraceLog(LOG_WARNING, "SaveSystem: dropping unknown weapon id '%s' from %s",
                             wid.c_str(), savePathText.c_str());
                    rewriteSanitizedSave = true;
                    continue;
                }

                const size_t before = inv.weaponCount();
                inv.addWeapon(*weapon);
                if (inv.weaponCount() == before) {
                    rewriteSanitizedSave = true;
                    continue;
                }

                const size_t weaponIndex = inv.weaponCount() - 1;
                int loadedAmmo = weapon->maxAmmo;
                if (i < savedAmmo.size()) {
                    loadedAmmo = savedAmmo[i];
                } else {
                    rewriteSanitizedSave = true;
                }

                const int clampedAmmo = clampAmmoForWeapon(loadedAmmo, *weapon);
                if (clampedAmmo != loadedAmmo) {
                    rewriteSanitizedSave = true;
                }
                inv.setAmmo(weaponIndex, clampedAmmo);
            }
        }
        if (savedAmmo.size() > inv.weaponCount()) {
            rewriteSanitizedSave = true;
        }

        if (j.contains("stats")) {
            auto& s = j["stats"];
            if (s.is_object()) {
                const int loadedDeaths = sanitizedIntField(s, "totalDeaths", 0, &rewriteSanitizedSave);
                const int loadedFrames = sanitizedIntField(s, "totalPlayFrames", 0, &rewriteSanitizedSave);
                const int loadedEnemies = sanitizedIntField(s, "enemiesDefeated", 0, &rewriteSanitizedSave);
                const int loadedBosses = sanitizedIntField(s, "bossesDefeated", 0, &rewriteSanitizedSave);
                state().totalDeaths = clampNonNegative(loadedDeaths);
                state().totalPlayFrames = clampNonNegative(loadedFrames);
                state().enemiesDefeated = clampNonNegative(loadedEnemies);
                state().bossesDefeated = clampNonNegative(loadedBosses);
                if (state().totalDeaths != loadedDeaths || state().totalPlayFrames != loadedFrames ||
                    state().enemiesDefeated != loadedEnemies || state().bossesDefeated != loadedBosses) {
                    rewriteSanitizedSave = true;
                }
                state().sigmaDefeated = sanitizedBoolField(s, "sigmaDefeated", false, &rewriteSanitizedSave);
            } else {
                rewriteSanitizedSave = true;
            }
        }

        state().bestTimes.clear();
        if (j.contains("bestTimes") && j["bestTimes"].is_object()) {
            for (auto& [key, val] : j["bestTimes"].items()) {
                if (!isSafeIdToken(key) || !val.is_number_integer()) {
                    rewriteSanitizedSave = true;
                    continue;
                }
                const int frames = val.get<int>();
                if (frames <= 0) {
                    rewriteSanitizedSave = true;
                    continue;
                }
                state().bestTimes[key] = frames;
            }
        } else if (j.contains("bestTimes")) {
            rewriteSanitizedSave = true;
        }

        playerProgress.persistentStateInitialized = true;
        Player::applyProgress(playerProgress);
        if (rewriteSanitizedSave && !save(slot)) {
            TraceLog(LOG_WARNING, "SaveSystem: failed to rewrite sanitized save %s", savePathText.c_str());
        }
        return true;
    } catch (const json::exception&) {
        resetLoadedState();
        return false;
    }
}

int SaveSystem::totalDeaths() {
    return state().totalDeaths;
}

int SaveSystem::totalPlayFrames() {
    return state().totalPlayFrames;
}

int SaveSystem::enemiesDefeated() {
    return state().enemiesDefeated;
}

int SaveSystem::bossesDefeated() {
    return state().bossesDefeated;
}

bool SaveSystem::sigmaDefeated() {
    return state().sigmaDefeated;
}

void SaveSystem::setTotalDeaths(int value) {
    state().totalDeaths = value;
}

void SaveSystem::setTotalPlayFrames(int value) {
    state().totalPlayFrames = value;
}

void SaveSystem::setEnemiesDefeated(int value) {
    state().enemiesDefeated = value;
}

void SaveSystem::setBossesDefeated(int value) {
    state().bossesDefeated = value;
}

void SaveSystem::setSigmaDefeated(bool defeated) {
    state().sigmaDefeated = defeated;
}

void SaveSystem::incrementDeaths() {
    state().totalDeaths++;
}

void SaveSystem::incrementEnemiesDefeated() {
    state().enemiesDefeated++;
}

void SaveSystem::incrementBossesDefeated() {
    state().bossesDefeated++;
}

void SaveSystem::addPlayFrames(int frames) {
    state().totalPlayFrames += frames;
}
int SaveSystem::getBestTime(StageId stageId) {
    auto it = state().bestTimes.find(stageId.str());
    return (it != state().bestTimes.end()) ? it->second : 0;
}

void SaveSystem::recordTime(StageId stageId, int frames) {
    auto it = state().bestTimes.find(stageId.str());
    if (it == state().bestTimes.end() || frames < it->second) {
        state().bestTimes[stageId.str()] = frames;
    }
}

bool SaveSystem::exists(const SaveSlot& slot) {
    return fs::exists(slot.path());
}

void SaveSystem::deleteSave(const SaveSlot& slot) {
    fs::remove(slot.path());
    if (slot.path() == activeSlot().path()) {
        resetProgressionForNewRun();
    }
}

void SaveSystem::markStageCompleted(StageId stageId) {
    for (const auto& id : state().completedStages) {
        if (id == stageId.str()) return;
    }
    state().completedStages.push_back(stageId.str());
}

bool SaveSystem::isStageCompleted(StageId stageId) {
    for (const auto& id : state().completedStages) {
        if (id == stageId.str()) return true;
    }
    return false;
}

const std::vector<std::string>& SaveSystem::completedStages() {
    return state().completedStages;
}

void SaveSystem::markBossDefeated(BossId bossId) {
    if (bossId.empty()) return;
    for (const auto& id : state().defeatedBosses) {
        if (id == bossId.str()) return;
    }
    state().defeatedBosses.push_back(bossId.str());
}

bool SaveSystem::isBossDefeated(BossId bossId) {
    for (const auto& id : state().defeatedBosses) {
        if (id == bossId.str()) return true;
    }
    return false;
}

const std::vector<std::string>& SaveSystem::defeatedBosses() {
    return state().defeatedBosses;
}

} // namespace mmx
