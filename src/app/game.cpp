#include "game.h"
#include "harness/autotest_armor_mask.h"
#include "harness/autotest_profiles.h"
#include "harness/autotest_script.h"
#include "constants.h"
#include "screen_transform.h"
#include "input.h"
#include "harness/smoke_script.h"
#include "systems/asset_cache.h"
#include "systems/audio.h"
#include "data/display_config.h"
#include "data/save_system.h"
#include "data/settings.h"
#include "gameplay/gameplay_scene.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <filesystem>

namespace mmx {
namespace {

std::string sanitizedSmokeStem(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_') {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) out = "smoke";
    if (out.size() > 64) out.resize(64);
    return out;
}

int parsePositiveFrameCap(const char* value, int fallback) {
    if (!value || *value == '\0') return fallback;
    errno = 0;
    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed <= 0 || parsed > 20000) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

bool envFlagEnabled(const char* value) {
    return value && *value != '\0' && std::string(value) != "0";
}

unsigned rngSeedFromEnvOrTime() {
    const char* value = std::getenv("MMX_RAND_SEED");
    if (!value || *value == '\0') {
        return static_cast<unsigned>(std::time(nullptr));
    }
    errno = 0;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0') {
        return static_cast<unsigned>(std::time(nullptr));
    }
    return static_cast<unsigned>(parsed);
}

std::filesystem::path localArtifactDirectory(const char* leaf) {
    return std::filesystem::path("build") / leaf;
}

std::filesystem::path autotestShotDirectory() {
    const char* value = std::getenv("MMX_AUTOTEST_SHOT_DIR");
    if (!value || *value == '\0') return localArtifactDirectory("autotest");
    return std::filesystem::path(value);
}

std::filesystem::path smokeShotDirectory() {
    const char* value = std::getenv("MEGAMAN_X_SMOKE_SHOT_DIR");
    if (!value || *value == '\0') return localArtifactDirectory("tmp_smoke");
    return std::filesystem::path(value);
}

} // namespace

Game::Game(bool hiddenWindow) {
    std::srand(rngSeedFromEnvOrTime());

    Settings::load();
    Input::setBindings(Settings::inputBindings);

    SaveSystem::bindState(sceneManager_.runtimeState().save);
    SaveSystem::bindWeaponInventoryState(sceneManager_.runtimeState().weaponInventory);
    Player::bindProgress(sceneManager_.runtimeState().playerProgress);
    AudioManager::bindRuntimeState(sceneManager_.runtimeState().audio);

    const bool audioPlaybackRequested =
        !hiddenWindow &&
        !envFlagEnabled(std::getenv("MEGAMAN_X_AUDIO_DISABLED"));

    int windowFlags = FLAG_WINDOW_RESIZABLE;
    if (Settings::vsync) {
        windowFlags |= FLAG_VSYNC_HINT;
    }
    if (hiddenWindow) {
        windowFlags |= FLAG_WINDOW_HIDDEN;
    }
    SetConfigFlags(windowFlags);
    int winH = display_config::windowHeight(Settings::windowScale);
    int winW = display_config::windowWidth(Settings::windowScale, Settings::aspect43);
    InitWindow(winW, winH, "Megaman X");
    if (audioPlaybackRequested) {
        InitAudioDevice();
        audioDeviceInitialized_ = IsAudioDeviceReady();
    }

    if (Settings::borderlessFullscreen && !hiddenWindow) {
        ToggleBorderlessWindowed();
    } else if (Settings::fullscreen && !hiddenWindow) {
        ToggleFullscreen();
    }

    if (target_.load(INTERNAL_WIDTH, INTERNAL_HEIGHT)) {
        target_.setTextureFilter(TEXTURE_FILTER_POINT);
    } else {
        TraceLog(LOG_ERROR, "Failed to create internal render target");
        running_ = false;
    }

    AudioManager::init(audioDeviceInitialized_);
    AudioManager::setMasterVolume(hiddenWindow ? 0.0f : Settings::masterVolume);
    AudioManager::setMusicVolume(Settings::musicVolume);
    AudioManager::setSFXVolume(Settings::sfxVolume);
    configureFiniteSmokeFromEnv();
}

Game::~Game() {
    sceneManager_.clear();
    SaveSystem::useFallbackWeaponInventoryState();
    SaveSystem::useFallbackState();
    Player::useFallbackProgress();
    Enemy::unloadDefinitions();
    AssetCache::clear();
    AudioManager::shutdown();
    AudioManager::useFallbackRuntimeState();
    target_.reset();
    if (audioDeviceInitialized_) {
        CloseAudioDevice();
        audioDeviceInitialized_ = false;
    }
    CloseWindow();
}

void Game::run() {
    while (!WindowShouldClose() && running_) {
        processFrame();
    }
}

void Game::processFrame() {
    float frameTime = GetFrameTime();

    // Headless autotest/smoke runs unthrottled (no vsync), so GetFrameTime() is
    // ~0 and the fixed-timestep loop below barely ticks â€” scenes would never
    // advance. Force exactly one physics tick per rendered frame so autotest is
    // deterministic (autotestFrame_ == game ticks), which is what the per-profile
    // snap timings assume.
    if (autotestEnabled_ || finiteSmokeEnabled_) frameTime = PHYSICS_DT;

    // Clamp frame time to prevent spiral of death.
    // If the game hitches (debugger, window drag), we don't want to simulate
    // 200 physics steps to "catch up" â€” that would cause more hitching.
    // Instead, we cap and accept that the game runs in slow motion briefly.
    frameTime = std::min(frameTime, PHYSICS_DT * MAX_PHYSICS_STEPS_PER_FRAME);

    accumulator_ += frameTime;

    // Poll input once per render frame. This captures IsKeyPressed() events
    // that would be missed on high-refresh monitors where many frames have
    // no physics tick. The scene latches presses for the next physics tick.
    Input::poll();
    sceneManager_.pollInput();

    // Fixed-timestep physics loop.
    // Each iteration advances the game by exactly PHYSICS_DT seconds.
    // Multiple iterations per frame when the render is slower than physics
    // (rare on modern hardware at 60Hz). Zero iterations when render is
    // faster than physics (e.g., 144Hz monitor â€” some frames skip physics).
    while (accumulator_ >= PHYSICS_DT) {
        Input::update();
        sceneManager_.handleInput();
        sceneManager_.update(PHYSICS_DT);
        Input::clear();
        accumulator_ -= PHYSICS_DT;
        if (sceneManager_.quitRequested()) {
            running_ = false;
            return;
        }
    }

    // Update music stream (must be called every frame)
    AudioManager::update();

    // Alpha is how far we are between the last physics state and the next.
    // 0.0 = exactly at the last physics tick. 1.0 = exactly at the next.
    // Scenes can use this to interpolate positions for smooth rendering.
    float alpha = accumulator_ / PHYSICS_DT;

    // Phase 1: Render the game at internal SNES resolution
    BeginTextureMode(target_.get());
        ClearBackground(BLACK);
        sceneManager_.render(alpha);
    EndTextureMode();

    // Phase 2: Scale the internal framebuffer to the window
    BeginDrawing();
        ClearBackground(BLACK);

        // Fit the framebuffer into the window at the target display aspect,
        // letterboxing/pillarboxing the remainder. 4:3 stretches the 256x224
        // image horizontally to the classic SNES-on-CRT aspect; native uses
        // square pixels (8:7).
        const float winW = static_cast<float>(GetScreenWidth());
        const float winH = static_cast<float>(GetScreenHeight());
        const screen_transform::InternalViewport viewport =
            screen_transform::internalViewportForWindow(winW, winH, Settings::aspect43);

        // RenderTexture has flipped Y in OpenGL, so we flip the source rect
        Rectangle src = {0, 0, static_cast<float>(INTERNAL_WIDTH),
                         -static_cast<float>(INTERNAL_HEIGHT)};
        Rectangle dst = {viewport.x, viewport.y, viewport.width, viewport.height};

        DrawTexturePro(target_.texture(), src, dst, {0, 0}, 0.0f, WHITE);

        if (IsKeyDown(KEY_F9)) DrawFPS(4, 4);
    EndDrawing();

    if (IsKeyPressed(KEY_F11)) {
        if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) {
            ToggleBorderlessWindowed();
        }
        Settings::borderlessFullscreen = false;
        ToggleFullscreen();
        Settings::fullscreen = IsWindowFullscreen();
        Settings::save();
    }

    if (IsKeyPressed(KEY_F10)) {
        static int shotN = 0;
        char path[64];
        snprintf(path, sizeof(path), "debug_shot_%02d.png", shotN++);
        saveFramebufferShot(path);
    }

    if (finiteSmokeEnabled_) finiteSmokeTick();
    if (autotestEnabled_) autotestTick();
}

void Game::enableAutotest(const std::string& stageId, const std::string& profile) {
    autotestEnabled_ = true;
    sceneManager_.setTransitionsEnabled(false);
    autotestStageId_ = stageId;
    autotestProfile_ = profile;
    autotestFrame_ = 0;
    autotestShotsTaken_ = 0;

    // U66: autotests default to the CLEARED-SAVE loadout. Visual evidence can
    // request a partial mask with MMX_AUTOTEST_ARMOR.
    const AutotestArmorMask armor =
        parseAutotestArmorMask(std::getenv("MMX_AUTOTEST_ARMOR"));
    PlayerProgress progress = Player::captureProgress();
    progress.armorBoots = armor.boots;
    progress.armorHelmet = armor.helmet;
    progress.armorBody = armor.body;
    progress.armorBuster = armor.buster;
    Player::applyProgress(progress);

    const int parityReplayFrames =
        parsePositiveFrameCap(std::getenv("MMX_PARITY_REPLAY_FRAMES"), 120);
    autotestTotalFrames_ =
        autotest_profiles::totalFrames(profile, parityReplayFrames);
}

void Game::enableFiniteSmoke(const std::string& id, const std::string& profile, int frames) {
    finiteSmokeEnabled_ = true;
    sceneManager_.setTransitionsEnabled(false);
    finiteSmokeId_ = sanitizedSmokeStem(id);
    finiteSmokeProfile_ = sanitizedSmokeStem(profile);
    finiteSmokeFrame_ = 0;
    finiteSmokeShotsTaken_ = 0;
    finiteSmokeTotalFrames_ = std::clamp(frames, 1, 20000);
}

void Game::configureFiniteSmokeFromEnv() {
    const char* id = std::getenv("MEGAMAN_X_SMOKE_ID");
    if (!id || *id == '\0') return;

    const char* profile = std::getenv("MEGAMAN_X_SMOKE_PROFILE");
    const char* frames = std::getenv("MEGAMAN_X_SMOKE_FRAMES");
    enableFiniteSmoke(id, profile && *profile ? profile : "boot",
                      parsePositiveFrameCap(frames, 240));
}

void Game::saveFramebufferShot(const char* path) {
    ImageResource img;
    if (!img.loadFromTexture(target_.texture())) {
        TraceLog(LOG_WARNING, "Autotest: failed to capture framebuffer for %s", path);
        return;
    }
    img.flipVertical();  // raylib RenderTexture is flipped vertically
    img.exportTo(path);
}

void Game::finiteSmokeSnap(int atFrame) {
    if (finiteSmokeFrame_ != atFrame) return;

    const std::filesystem::path dir = smokeShotDirectory();
    std::filesystem::create_directories(dir);

    char filename[160];
    snprintf(filename, sizeof(filename), "smoke_%s_%s_%02d.png",
             finiteSmokeId_.c_str(), finiteSmokeProfile_.c_str(),
             finiteSmokeShotsTaken_++);

    const std::string path = (dir / filename).string();
    saveFramebufferShot(path.c_str());
}

void Game::finiteSmokeTick() {
    finiteSmokeFrame_++;
    int f = finiteSmokeFrame_;

    smoke_script::runFrame(finiteSmokeId_, finiteSmokeProfile_, f);

    finiteSmokeSnap(1);
    finiteSmokeSnap(std::max(1, finiteSmokeTotalFrames_ / 2));
    finiteSmokeSnap(std::max(1, finiteSmokeTotalFrames_ - 1));

    if (f >= finiteSmokeTotalFrames_) {
        TraceLog(LOG_INFO, "Smoke: %s:%s done in %d frames, %d shots saved",
                 finiteSmokeId_.c_str(), finiteSmokeProfile_.c_str(),
                 f, finiteSmokeShotsTaken_);
        running_ = false;
    }
}

void Game::autotestSnap(int atFrame) {
    if (autotestFrame_ != atFrame) return;
    const bool frameNamed =
        envFlagEnabled(std::getenv("MMX_AUTOTEST_FRAME_NAMES"));
    const std::filesystem::path dir = autotestShotDirectory();
    std::filesystem::create_directories(dir);

    char filename[160];
    if (frameNamed) {
        snprintf(filename, sizeof(filename), "autotest_%s_%s_f%04d.png",
                 autotestStageId_.c_str(), autotestProfile_.c_str(), atFrame);
    } else {
        snprintf(filename, sizeof(filename), "autotest_%s_%s_%02d.png",
                 autotestStageId_.c_str(), autotestProfile_.c_str(),
                 autotestShotsTaken_);
    }
    autotestShotsTaken_++;

    const std::string path = (dir / filename).string();
    saveFramebufferShot(path.c_str());
}

void Game::autotestTick() {
    autotestFrame_++;
    int f = autotestFrame_;

    // MMX_AUTOTEST_SNAP_EVERY=N: additionally snap every Nth frame (dense
    // capture for animation/flicker debugging — profiles only snap a
    // handful of fixed frames, which hides per-frame artifacts).
    static const int snapEvery = []() {
        const char* e = std::getenv("MMX_AUTOTEST_SNAP_EVERY");
        return e ? std::atoi(e) : 0;
    }();
    if (snapEvery > 0 && f % snapEvery == 0) autotestSnap(f);

    // The profile script owns scripted input for this frame and can request
    // fixed-frame captures through the callback.
    autotest_script::runFrame(autotestProfile_, f, [this](int atFrame) {
        autotestSnap(atFrame);
    });

    if (f >= autotestTotalFrames_) {
        TraceLog(LOG_INFO, "Autotest: '%s:%s' done in %d frames, %d shots saved",
                 autotestStageId_.c_str(), autotestProfile_.c_str(), f, autotestShotsTaken_);
        running_ = false;
    }
}

} // namespace mmx
