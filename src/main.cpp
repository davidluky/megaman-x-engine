#include "app/game.h"
#include "gameplay/logging.h"
#include "harness/autotest_profiles.h"
#include "gameplay/gameplay_scene.h"
#include "data/content_pack.h"
#include "data/content_paths.h"
#include "data/save_system.h"
#include "data/settings.h"
#include "ui/title_scene.h"
#include "ui/stage_select_scene.h"
#include "ui/boss_intro_scene.h"
#include "ui/extras_scene.h"
#include "ui/boss_rush_scene.h"
#include "ui/bloody_palace_scene.h"
#include "ui/map_editor_scene.h"
#include "systems/randomizer.h"
#include <cerrno>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

// --stage <stage-id> boots directly into the named stage for interactive play.
//
// --autotest <stage-id>[:profile] boots directly into the named stage and
// runs one of the registered autotest profiles (default walk), captures screenshots at
// profile-defined moments, and exits.
//
// --spawn-at <x>,<y> overrides the stage's default player spawn. It is valid
// with --autotest for harness runs; direct --stage launches require the
// player-facing Practice Mode setting so debug spawns stay opt-in.
//
// --stage-file <path> overrides the stage JSON used by --stage/--autotest.
// The stage id still comes from --stage/--autotest, so audio, save identity,
// and autotest screenshot naming remain stable while testing build candidates.
//
// --list-stages and --help are data-only commands that exit before creating
// the game window.
//
// EN10B-covered runtime launchers scan this exact marker before starting an
// override executable. It is printed by a pre-window CLI route so the linker
// must retain it in every EN10-capable build.
static constexpr char kRuntimeMasterMuteAttestation[] =
    "MMX-EN10-HIDDEN-MASTER-MUTE:v1";
static constexpr char kRuntimeNoAudioDeviceAttestation[] =
    "MMX-EN10E-NO-AUDIO-DEVICE:v1";

static bool isSafeStageId(const std::string& stageId) {
    if (stageId.empty()) return false;
    for (unsigned char c : stageId) {
        if (!std::isalnum(c) && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

static bool isTestStageAlias(const std::string& stageId) {
    return stageId == "test-stage";
}

static std::string runtimeStageIdFor(const std::string& stageId) {
    return isTestStageAlias(stageId) ? "flame-mammoth" : stageId;
}

static std::string stagePathFor(const std::string& stageId) {
    if (!isSafeStageId(stageId)) return {};
    if (isTestStageAlias(stageId)) {
        return "content/x1/stages/tiles/flame-mammoth_full.json";
    }

    namespace fs = std::filesystem;
    const std::string decoded = "content/x1/stages/tiles/" + stageId + "_full.json";
    if (fs::exists(decoded)) return decoded;

    const std::string x1 = "content/x1/stages/ripped/" + stageId + "/stage_final.json";
    if (fs::exists(x1)) return x1;
    return "content/extras/stages/" + stageId + "/stage_final.json";
}

static void addTestStageEnemies(mmx::GameplayScene& gameplay) {
    gameplay.addExtraEnemySpawn({"enemy", "walker", 304.0f, 640.0f});
}

static void printHelp(const char* exe) {
    const std::string profiles = mmx::autotest_profiles::helpText();
    std::printf(
        "megaman-x - C++/raylib fan engine for Mega Man X (SNES, 1993)\n"
        "\n"
        "Usage:\n"
        "  %s                              Launch the title screen.\n"
        "  %s --boss-rush                  Boss Rush: fight all 8 Mavericks.\n"
        "  %s --bloody-palace              Bloody Palace: 30-wave combat arena.\n"
        "  %s --randomizer [seed]          Randomizer: shuffle enemies/bosses.\n"
        "  %s --stage-select               Boot directly to the boss select menu.\n"
        "  %s --map-editor                 Boot directly to the custom map editor.\n"
        "  %s --stage <id>                 Boot directly into a stage for play.\n"
        "                                  Use test-stage for the default TEST STAGE.\n"
        "  %s --extras                     Launch the Extras menu (fork stages).\n"
        "  %s --autotest <id>[:profile]    Boot directly into a stage, run a\n"
        "                                  scripted profile, save screenshots,\n"
        "                                  and exit. Profiles: %s.\n"
        "  %s --stage-file <json>          Use a specific stage JSON with\n"
        "                                  --stage or --autotest. Assets may\n"
        "                                  resolve only under content/ or that\n"
        "                                  stage file's allowed asset root.\n"
        "  %s --spawn-at x,y               Override the stage's default player\n"
        "                                  spawn. Autotest only unless\n"
        "                                  Practice Mode is ON for --stage.\n"
        "  %s --list-stages                Print the stage inventory from both\n"
        "                                  content packs (x1 + extras) and exit.\n"
        "  %s --runtime-attestation        Print the automation audio-isolation\n"
        "                                  contract marker and exit pre-window.\n"
        "  %s --help                       Show this help.\n",
        exe, exe, exe, exe, exe, exe, exe, exe, exe, profiles.c_str(),
        exe, exe, exe, exe, exe);
}

static void listStages() {
    auto dump = [](const char* manifest, const char* label) {
        mmx::ContentPack pack;
        if (!pack.loadFromFile(manifest)) {
            std::printf("  (could not load %s)\n", manifest);
            return;
        }
        std::printf("%s (%s, v%s, %zu stages):\n",
                    label, pack.name.c_str(), pack.version.c_str(),
                    pack.stages.size());
        for (const auto& s : pack.stages) {
            const char* avail = s.available ? "[ok]  " : "[--]  ";
            std::printf("  %s %-20s  %s", avail, s.id.c_str(), s.name.c_str());
            if (!s.origin.empty()) std::printf("  <%s>", s.origin.c_str());
            if (!s.boss.empty())   std::printf("  boss=%s", s.boss.c_str());
            std::printf("\n");
        }
    };
    dump("content/x1/manifest.json", "Main pack (x1)");
    std::printf("\n");
    dump("content/extras/manifest.json", "Extras pack");
}

struct CliOptions {
    std::string directStage;
    std::string autotestStage;
    std::string autotestProfile = "walk";
    std::string stageFileOverride;
    bool hasSpawnOverride = false;
    bool hasAutotestRenderCameraOverride = false;
    float autotestRenderCameraX = 0.0f;
    float autotestRenderCameraY = 0.0f;
    bool launchExtras = false;
    bool launchBossRush = false;
    bool launchBloodyPalace = false;
    bool launchRandomizer = false;
    bool launchStageSelect = false;
    bool launchMapEditor = false;
    uint64_t randomizerSeed = 0;
    float spawnX = 0.0f;
    float spawnY = 0.0f;
};

static bool isKnownAutotestProfile(const std::string& profile) {
    return mmx::autotest_profiles::isKnown(profile);
}

static bool failCli(std::string& error, const std::string& message) {
    error = message;
    return false;
}

static bool parseFloatLiteral(const std::string& value, float& out) {
    if (value.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(value.c_str(), &end);
    if (errno == ERANGE || end == value.c_str() || *end != '\0' || !std::isfinite(parsed)) {
        return false;
    }
    out = parsed;
    return true;
}

static bool parseSpawnAt(const std::string& value, float& x, float& y) {
    const auto comma = value.find(',');
    if (comma == std::string::npos || comma == 0 || comma + 1 >= value.size()) {
        return false;
    }
    return parseFloatLiteral(value.substr(0, comma), x) &&
           parseFloatLiteral(value.substr(comma + 1), y);
}

static bool parseHexSeed(const std::string& value, uint64_t& seed) {
    if (value.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(value.c_str(), &end, 16);
    if (errno == ERANGE || end == value.c_str() || *end != '\0') {
        return false;
    }
    seed = static_cast<uint64_t>(parsed);
    return true;
}

static int launchModeCount(const CliOptions& options) {
    int count = 0;
    if (!options.directStage.empty()) ++count;
    if (!options.autotestStage.empty()) ++count;
    if (options.launchExtras) ++count;
    if (options.launchBossRush) ++count;
    if (options.launchBloodyPalace) ++count;
    if (options.launchRandomizer) ++count;
    if (options.launchStageSelect) ++count;
    if (options.launchMapEditor) ++count;
    return count;
}

static bool parseCli(int argc, char** argv, CliOptions& options, bool& exitNow, int& exitCode, std::string& error) {
    exitNow = false;
    exitCode = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printHelp(argv[0]);
            exitNow = true;
            return true;
        }
        if (arg == "--list-stages") {
            listStages();
            exitNow = true;
            return true;
        }
        if (arg == "--runtime-attestation") {
            std::printf("%s\n%s\n",
                        kRuntimeMasterMuteAttestation,
                        kRuntimeNoAudioDeviceAttestation);
            exitNow = true;
            return true;
        }
        if (arg == "--extras") {
            options.launchExtras = true;
            continue;
        }
        if (arg == "--boss-rush") {
            options.launchBossRush = true;
            continue;
        }
        if (arg == "--bloody-palace") {
            options.launchBloodyPalace = true;
            continue;
        }
        if (arg == "--randomizer") {
            options.launchRandomizer = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                if (!parseHexSeed(argv[i + 1], options.randomizerSeed)) {
                    return failCli(error, "Invalid --randomizer seed: expected a full hexadecimal value");
                }
                ++i;
            } else {
                options.randomizerSeed = static_cast<uint64_t>(std::time(nullptr));
            }
            continue;
        }
        if (arg == "--stage-select") {
            options.launchStageSelect = true;
            continue;
        }
        if (arg == "--map-editor") {
            options.launchMapEditor = true;
            continue;
        }
        if (arg == "--stage") {
            if (i + 1 >= argc) {
                return failCli(error, "Missing operand for --stage");
            }
            options.directStage = argv[++i];
            if (!isSafeStageId(options.directStage)) {
                return failCli(error, "Invalid --stage id: " + options.directStage);
            }
            continue;
        }
        if (arg == "--autotest") {
            if (i + 1 >= argc) {
                return failCli(error, "Missing operand for --autotest");
            }

            const std::string value = argv[++i];
            const auto colon = value.find(':');
            if (colon == std::string::npos) {
                options.autotestStage = value;
            } else {
                options.autotestStage = value.substr(0, colon);
                options.autotestProfile = value.substr(colon + 1);
            }
            if (!isSafeStageId(options.autotestStage)) {
                return failCli(error, "Invalid --autotest stage id: " + options.autotestStage);
            }
            if (!isKnownAutotestProfile(options.autotestProfile)) {
                return failCli(error, "Invalid --autotest profile: " + options.autotestProfile);
            }
            continue;
        }
        if (arg == "--stage-file") {
            if (i + 1 >= argc) {
                return failCli(error, "Missing operand for --stage-file");
            }
            options.stageFileOverride = argv[++i];
            continue;
        }
        if (arg == "--spawn-at") {
            if (i + 1 >= argc) {
                return failCli(error, "Missing operand for --spawn-at");
            }
            if (!parseSpawnAt(argv[++i], options.spawnX, options.spawnY)) {
                return failCli(error, "Invalid --spawn-at value: expected x,y numeric coordinates");
            }
            options.hasSpawnOverride = true;
            continue;
        }

        return failCli(error, "Unknown argument: " + arg);
    }

    if (launchModeCount(options) > 1) {
        return failCli(error, "Conflicting launch modes: choose only one of --stage, --autotest, --extras, --boss-rush, --bloody-palace, --stage-select, --map-editor, or --randomizer");
    }
    if (options.hasSpawnOverride && options.autotestStage.empty() && options.directStage.empty()) {
        return failCli(error, "--spawn-at requires --stage or --autotest");
    }
    if (!options.stageFileOverride.empty() && options.autotestStage.empty() && options.directStage.empty()) {
        return failCli(error, "--stage-file requires --stage or --autotest");
    }

    return true;
}

// --stage-file is for local candidate stages. A candidate under build/ may use
// sibling generated assets anywhere under the nearest build/ ancestor; a file
// outside build/ is limited to its containing directory.
static std::filesystem::path assetRootForStageFile(const std::filesystem::path& stageFile) {
    std::filesystem::path current = stageFile;
    if (current.has_filename()) {
        current = current.parent_path();
    }
    for (auto cursor = current; !cursor.empty(); cursor = cursor.parent_path()) {
        if (cursor.filename() == "build") {
            return cursor;
        }
        if (cursor == cursor.root_path()) {
            break;
        }
    }
    return current;
}

static bool envFlagEnabled(const char* value) {
    return value && *value != '\0' && std::string(value) != "0";
}

static bool applyBossRoomSpawnFromStageFile(const std::string& stagePath, CliOptions& cli, std::string& error) {
    std::ifstream sf(stagePath);
    if (!sf.is_open()) return true;

    try {
        nlohmann::json sj;
        sf >> sj;
        for (const auto& sp : sj.value("spawns", nlohmann::json::array())) {
            if (sp.value("type", "") == "boss") {
                cli.spawnX = sp.value("x", 0.0f) - 128.0f;
                cli.spawnY = sp.value("y", 0.0f);
                cli.hasSpawnOverride = true;
                break;
            }
        }
        return true;
    } catch (const std::exception& e) {
        error = "Invalid --stage-file JSON while probing boss-room spawn: " + stagePath + ": " + e.what();
        return false;
    }
}

int main(int argc, char** argv) {
    CliOptions cli;
    bool exitNow = false;
    int exitCode = 0;
    std::string cliError;
    if (!parseCli(argc, argv, cli, exitNow, exitCode, cliError)) {
        std::fprintf(stderr, "%s\nUse --help to see valid options.\n", cliError.c_str());
        return 2;
    }
    if (exitNow) {
        return exitCode;
    }

    std::string directStagePath;
    if (!cli.directStage.empty()) {
        directStagePath = stagePathFor(cli.directStage);
        if (directStagePath.empty()) {
            std::fprintf(stderr, "Invalid --stage id: %s\n", cli.directStage.c_str());
            return 2;
        }
        if (!std::filesystem::exists(directStagePath)) {
            std::fprintf(stderr, "Unknown --stage id: %s\n", cli.directStage.c_str());
            return 2;
        }
        if (!cli.stageFileOverride.empty()) {
            directStagePath = cli.stageFileOverride;
        }
    }

    std::string autotestStagePath;
    if (!cli.autotestStage.empty()) {
        autotestStagePath = stagePathFor(cli.autotestStage);
        if (autotestStagePath.empty()) {
            std::fprintf(stderr, "Invalid --autotest stage id: %s\n", cli.autotestStage.c_str());
            return 2;
        }
        if (!std::filesystem::exists(autotestStagePath)) {
            std::fprintf(stderr, "Unknown --autotest stage id: %s\n", cli.autotestStage.c_str());
            return 2;
        }
        if (!cli.stageFileOverride.empty()) {
            autotestStagePath = cli.stageFileOverride;
        }
    }

    if (!cli.stageFileOverride.empty()) {
        const std::filesystem::path overridePath(cli.stageFileOverride);
        if (overridePath.extension() != ".json") {
            std::fprintf(stderr, "Invalid --stage-file: expected a .json stage file\n");
            return 2;
        }
        if (!std::filesystem::exists(overridePath) || !std::filesystem::is_regular_file(overridePath)) {
            std::fprintf(stderr, "Unknown --stage-file: %s\n", cli.stageFileOverride.c_str());
            return 2;
        }
        mmx::content_paths::allowAdditionalAssetRoot(assetRootForStageFile(std::filesystem::absolute(overridePath)).string());
    }

    if (!cli.autotestStage.empty()) {
        const char* renderCameraX = std::getenv("MMX_AUTOTEST_RENDER_CAMERA_X");
        const char* renderCameraY = std::getenv("MMX_AUTOTEST_RENDER_CAMERA_Y");
        const bool hasRenderCameraX = renderCameraX && *renderCameraX;
        const bool hasRenderCameraY = renderCameraY && *renderCameraY;
        if (hasRenderCameraX != hasRenderCameraY) {
            std::fprintf(stderr, "MMX_AUTOTEST_RENDER_CAMERA_X/Y must be set together\n");
            return 2;
        }
        if (hasRenderCameraX) {
            if (!parseFloatLiteral(renderCameraX, cli.autotestRenderCameraX) ||
                !parseFloatLiteral(renderCameraY, cli.autotestRenderCameraY)) {
                std::fprintf(stderr, "Invalid MMX_AUTOTEST_RENDER_CAMERA_X/Y value: expected numeric coordinates\n");
                return 2;
            }
            cli.hasAutotestRenderCameraOverride = true;
        }
    }

    mmx::Settings::load();
    if (cli.hasSpawnOverride && !cli.directStage.empty() && cli.autotestStage.empty() &&
        !mmx::Settings::practiceMode) {
        std::fprintf(stderr, "--spawn-at with --stage requires Practice Mode enabled in Options.\n");
        return 2;
    }

    if (mmx::autotest_profiles::usesDeathPitSpawn(cli.autotestProfile) &&
        !cli.hasSpawnOverride) {
        cli.spawnX = 5376.0f;
        // Keep the death profile stage-agnostic. A stage coordinate can become
        // safe as decoded geometry evolves; far below the map always exercises
        // the pit-death path on the first gameplay tick.
        cli.spawnY = 50000.0f;
        cli.hasSpawnOverride = true;
    }

    if (mmx::autotest_profiles::usesBossRoomSpawn(cli.autotestProfile) &&
        !cli.hasSpawnOverride) {
        std::string bossSpawnError;
        if (!applyBossRoomSpawnFromStageFile(autotestStagePath, cli, bossSpawnError)) {
            std::fprintf(stderr, "%s\n", bossSpawnError.c_str());
            return 2;
        }
    }

    const bool hiddenWindow =
        !cli.autotestStage.empty() ||
        envFlagEnabled(std::getenv("MEGAMAN_X_SMOKE_ID")) ||
        envFlagEnabled(std::getenv("MEGAMAN_X_WINDOW_HIDDEN"));

    mmx::logging::installReleaseTraceLog(hiddenWindow);

    mmx::Game game(hiddenWindow);

    if (!cli.directStage.empty()) {
        mmx::SaveSystem::load();
        mmx::GameplaySceneConfig gameplayConfig;
        gameplayConfig.stagePath = directStagePath;
        gameplayConfig.stageId = mmx::StageId::fromString(runtimeStageIdFor(cli.directStage));
        if (cli.hasSpawnOverride) {
            gameplayConfig.spawnOverride = {cli.spawnX, cli.spawnY};
            gameplayConfig.hasSpawnOverride = true;
        }
        auto gameplay = std::make_unique<mmx::GameplayScene>(gameplayConfig);
        if (isTestStageAlias(cli.directStage)) {
            addTestStageEnemies(*gameplay);
        }
        gameplay->setSceneManager(&game.sceneManager());
        game.sceneManager().changeScene(std::move(gameplay));
    } else if (!cli.autotestStage.empty()) {
        mmx::SaveSystem::load();
        if (mmx::autotest_profiles::usesBossIntroScene(cli.autotestProfile)) {
            // Boot the boss-intro cutscene directly for fidelity capture.
            auto intro = std::make_unique<mmx::BossIntroScene>();
            intro->stageId = cli.autotestStage;
            intro->stagePath = autotestStagePath;
            intro->stageIdEnum = mmx::StageId::fromString(runtimeStageIdFor(cli.autotestStage));
            intro->setSceneManager(&game.sceneManager());
            game.sceneManager().changeScene(std::move(intro));
        } else {
            mmx::GameplaySceneConfig gameplayConfig;
            gameplayConfig.stagePath = autotestStagePath;
            gameplayConfig.stageId = mmx::StageId::fromString(runtimeStageIdFor(cli.autotestStage));
            if (cli.hasSpawnOverride) {
                gameplayConfig.spawnOverride = {cli.spawnX, cli.spawnY};
                gameplayConfig.hasSpawnOverride = true;
            }
            if (cli.hasAutotestRenderCameraOverride) {
                gameplayConfig.autotestRenderCameraOverride = {
                    cli.autotestRenderCameraX, cli.autotestRenderCameraY};
                gameplayConfig.hasAutotestRenderCameraOverride = true;
            }
            if (mmx::autotest_profiles::grantsAllWeapons(cli.autotestProfile)) {
                gameplayConfig.grantAllWeaponsForAutotest = true;
            }
            if (mmx::autotest_profiles::usesWeaponGetSequence(cli.autotestProfile)) {
                gameplayConfig.autotestWeaponGetSequence = true;
                // The capture must not write save/stat progress; the sequence
                // is the subject, not a real clear.
                gameplayConfig.suppressProgressionRewards = true;
            }
            if (cli.autotestProfile == "boss-entry") {
                gameplayConfig.skipInitialFade = true;
            }
            auto gameplay = std::make_unique<mmx::GameplayScene>(gameplayConfig);
            if (isTestStageAlias(cli.autotestStage)) {
                addTestStageEnemies(*gameplay);
            }
            gameplay->setSceneManager(&game.sceneManager());
            game.sceneManager().changeScene(std::move(gameplay));
        }
        game.enableAutotest(cli.autotestStage, cli.autotestProfile);
    } else if (cli.launchBossRush) {
        auto rush = std::make_unique<mmx::BossRushScene>();
        rush->setSceneManager(&game.sceneManager());
        game.sceneManager().changeScene(std::move(rush));
    } else if (cli.launchBloodyPalace) {
        auto palace = std::make_unique<mmx::BloodyPalaceScene>();
        palace->setSceneManager(&game.sceneManager());
        game.sceneManager().changeScene(std::move(palace));
    } else if (cli.launchRandomizer) {
        mmx::Randomizer rng(cli.randomizerSeed);
        std::printf("Randomizer seed: %s\n", rng.seedString().c_str());
        mmx::GameplaySceneConfig gameplayConfig;
        gameplayConfig.stagePath = stagePathFor("intro-highway");
        gameplayConfig.stageId = mmx::StageId::fromString("intro-highway");
        gameplayConfig.characterPath = "content/x1/characters/x.json";
        gameplayConfig.randomizerMode = true;
        gameplayConfig.randomizerSeed = cli.randomizerSeed;
        auto gameplay = std::make_unique<mmx::GameplayScene>(gameplayConfig);
        gameplay->setSceneManager(&game.sceneManager());
        game.sceneManager().changeScene(std::move(gameplay));
    } else if (cli.launchStageSelect) {
        auto stageSelect = std::make_unique<mmx::StageSelectScene>();
        mmx::ContentPack pack;
        pack.loadFromFile("content/x1/manifest.json");
        stageSelect->setContentPack(pack);
        stageSelect->setSceneManager(&game.sceneManager());
        game.sceneManager().changeScene(std::move(stageSelect));
    } else if (cli.launchMapEditor) {
        auto editor = std::make_unique<mmx::MapEditorScene>();
        mmx::ContentPack pack;
        pack.loadFromFile("content/x1/manifest.json");
        editor->setContentPack(pack);
        editor->setSceneManager(&game.sceneManager());
        game.sceneManager().changeScene(std::move(editor));
    } else if (cli.launchExtras) {
        auto extras = std::make_unique<mmx::ExtrasScene>();
        mmx::ContentPack pack;
        pack.loadFromFile("content/extras/manifest.json");
        extras->setContentPack(pack);
        extras->setSceneManager(&game.sceneManager());
        game.sceneManager().changeScene(std::move(extras));
    } else {
        auto title = std::make_unique<mmx::TitleScene>();
        title->setSceneManager(&game.sceneManager());
        game.sceneManager().changeScene(std::move(title));
    }

    game.run();
    return 0;
}
