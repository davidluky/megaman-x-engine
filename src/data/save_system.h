// save_system.h - declares persistent progress data and save-state accessors.
// Boundary: systems read/write through SaveSystem rather than owning save files.

#pragma once

#include "data/game_ids.h"
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

namespace mmx {

struct WeaponInventory;
struct WeaponInventoryState;

struct SaveData {
    int maxHealth = 16;

    bool armorBoots = false;
    bool armorHelmet = false;
    bool armorBody = false;
    bool armorBuster = false;

    struct SubTankData {
        bool collected = false;
        int health = 0;
    };
    SubTankData subTanks[4] = {};

    std::vector<std::string> collectedPickups;
    std::vector<std::string> completedStages;
    std::vector<std::string> defeatedBosses;

    struct WeaponData {
        std::string id;
    };
    std::vector<WeaponData> weapons;
    std::vector<int> ammo;
    int currentWeaponIndex = 0;

    // Stats
    int totalDeaths = 0;
    int totalPlayFrames = 0;   // Total frames spent in gameplay
    int enemiesDefeated = 0;
    int bossesDefeated = 0;
    bool sigmaDefeated = false;
};

struct SaveSummary {
    bool exists = false;
    int maxHealth = 16;
    int stagesCleared = 0;
    int totalDeaths = 0;
    int totalPlayFrames = 0;
    int enemiesDefeated = 0;
    int bossesDefeated = 0;
    bool sigmaDefeated = false;
};
struct SaveSystemState {
    std::vector<std::string> completedStages;
    std::vector<std::string> defeatedBosses;
    std::unordered_map<std::string, int> bestTimes;
    int totalDeaths = 0;
    int totalPlayFrames = 0;
    int enemiesDefeated = 0;
    int bossesDefeated = 0;
    bool sigmaDefeated = false;
    int activeSlotIndex = 1;
};

class SaveSlot {
public:
    static SaveSlot autosave();
    static std::optional<SaveSlot> numbered(int index);
    static std::optional<SaveSlot> named(const std::string& name);

    const std::filesystem::path& path() const { return path_; }
    std::string pathString() const { return path_.generic_string(); }

private:
    explicit SaveSlot(std::filesystem::path path) : path_(std::move(path)) {}

    std::filesystem::path path_;
};

class SaveSystem {
public:
    static constexpr int kSaveSlotCount = 3;
    static constexpr int kSaveFormatVersion = 1;

    static bool setActiveSlotIndex(int index);
    static int activeSlotIndex();
    static SaveSlot activeSlot();

    static bool save();
    static bool save(const SaveSlot& slot);
    static bool load();
    static bool load(const SaveSlot& slot);
    static void importData(const SaveData& data);
    static SaveData exportData();
    static bool exists();
    static bool exists(const SaveSlot& slot);
    static void deleteSave();
    static void deleteSave(const SaveSlot& slot);
    static std::optional<SaveSummary> summarize(const SaveSlot& slot);
    static bool anySaveExists();
    static void resetRuntimeState();
    static void resetProgressionForNewRun();

    static void markStageCompleted(StageId stageId);
    static bool isStageCompleted(StageId stageId);
    static const std::vector<std::string>& completedStages();

    static void markBossDefeated(BossId bossId);
    static bool isBossDefeated(BossId bossId);
    static const std::vector<std::string>& defeatedBosses();

    static void bindState(SaveSystemState& state);
    static void useFallbackState();

    static void bindWeaponInventoryState(WeaponInventoryState& state);
    static void useFallbackWeaponInventoryState();

    // Stats
    static int totalDeaths();
    static int totalPlayFrames();
    static int enemiesDefeated();
    static int bossesDefeated();
    static bool sigmaDefeated();
    static void setTotalDeaths(int value);
    static void setTotalPlayFrames(int value);
    static void setEnemiesDefeated(int value);
    static void setBossesDefeated(int value);
    static void setSigmaDefeated(bool defeated);
    static void incrementDeaths();
    static void incrementEnemiesDefeated();
    static void incrementBossesDefeated();
    static void addPlayFrames(int frames);
    // Best times per stage (frames). Returns 0 if no record exists.
    static int getBestTime(StageId stageId);
    static void recordTime(StageId stageId, int frames);

private:
    static void resetLoadedState();
    static SaveSystemState& state();
    static WeaponInventory weaponInventory();

    static SaveSystemState fallbackState_;
    static SaveSystemState* activeState_;
    static WeaponInventoryState* activeWeaponInventoryState_;
};

} // namespace mmx
