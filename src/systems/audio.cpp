// audio.cpp - manages music, SFX, APU events, and audio runtime state.
// Owns: audio caches, volume controls, fallback state, and playback logs.

#include "systems/audio.h"
#include "systems/pcm_loop.h"
#include "data/content_paths.h"
#include "data/kb_paths.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>

namespace mmx {

namespace {
struct BgmLoopRegion {
    std::size_t begin, end;
    unsigned sampleRate;
};
std::unordered_map<std::string, BgmLoopRegion> bgmLoopRegions;

// The audio thread consumes these buffers; the game thread supplies whole
// blocks with the source loop already joined, without frame-timed seeking.
class BgmLoopStream {
public:
    bool start(const std::string& path, const BgmLoopRegion& region, float volume) {
        reset();
        Wave wave = LoadWave(path.c_str());
        const bool accepted = wave.data && wave.sampleSize == 16 &&
            wave.sampleRate == region.sampleRate &&
            pcm_.load(static_cast<const std::int16_t*>(wave.data), wave.frameCount,
                      wave.channels, region.begin, region.end);
        const unsigned channels = wave.channels;
        UnloadWave(wave);
        if (!accepted) {
            TraceLog(LOG_WARNING, "Audio: invalid PCM source/loop region for %s", path.c_str());
            return false;
        }
        SetAudioStreamBufferSizeDefault(blockFrames);
        stream_ = LoadAudioStream(region.sampleRate, 16, channels);
        SetAudioStreamBufferSizeDefault(0);
        if (!IsAudioStreamValid(stream_)) {
            reset();
            return false;
        }
        SetAudioStreamVolume(stream_, volume);
        update();  // Prime both buffers before starting the device cursor.
        PlayAudioStream(stream_);
        return true;
    }
    bool active() const { return stream_.buffer != nullptr; }
    void reset() {
        if (active()) {
            StopAudioStream(stream_);
            UnloadAudioStream(stream_);
        }
        stream_ = {};
        pcm_.clear();
        paused_ = false;
    }
    void update() {
        if (!active() || paused_) return;
        // raylib has two processed flags. Bound each game update to those
        // two buffers even if a device consumes a block during submission.
        for (int i = 0; i < 2 && IsAudioStreamProcessed(stream_); ++i) {
            pcm_.fill(block_.data(), blockFrames);
            UpdateAudioStream(stream_, block_.data(), blockFrames);
        }
    }
    void pause() {
        if (active()) { paused_ = true; PauseAudioStream(stream_); }
    }
    void resume() {
        if (active()) { paused_ = false; ResumeAudioStream(stream_); }
    }
    void volume(float value) {
        if (active()) SetAudioStreamVolume(stream_, value);
    }
private:
    static constexpr int blockFrames = 4096;
    AudioStream stream_{};
    PcmLoop pcm_;
    std::array<std::int16_t, blockFrames * 2> block_{};
    bool paused_ = false;
};
BgmLoopStream bgmLoopStream;

bool readLoopRegion(const nlohmann::json& value, BgmLoopRegion& region) {
    if (!value.is_object()) return false;
    for (const char* key : {"start_frame", "end_frame", "sample_rate"}) {
        const auto it = value.find(key);
        if (it == value.end() || !it->is_number_integer() || *it < 0) return false;
    }
    const auto begin = value["start_frame"].get<std::uint64_t>();
    const auto end = value["end_frame"].get<std::uint64_t>();
    const auto rate = value["sample_rate"].get<std::uint64_t>();
    if (begin >= end || end > std::numeric_limits<std::size_t>::max() ||
        rate == 0 || rate > std::numeric_limits<unsigned>::max()) return false;
    region = {static_cast<std::size_t>(begin), static_cast<std::size_t>(end),
              static_cast<unsigned>(rate)};
    return true;
}
} // namespace

std::unordered_map<std::string, MusicResource> AudioManager::bgmCache_;
std::unordered_map<int, SoundResource> AudioManager::sfxCache_;
std::unordered_map<std::string, std::string> AudioManager::bgmMap_;

MusicResource* AudioManager::currentBGM_ = nullptr;
std::string AudioManager::currentBGMName_;
bool AudioManager::bgmPlaying_ = false;

float AudioManager::masterVol_ = 0.8f;
float AudioManager::musicVol_ = 0.7f;
float AudioManager::sfxVol_ = 0.8f;

bool AudioManager::initialized_ = false;
bool AudioManager::playbackEnabled_ = false;

std::unordered_map<int, SoundResource> AudioManager::apuCache_;
AudioRuntimeState AudioManager::fallbackState_;
AudioRuntimeState* AudioManager::activeState_ = &AudioManager::fallbackState_;

void AudioManager::init(bool playbackEnabled) {
    if (initialized_) return;
    playbackEnabled_ = playbackEnabled;
    loadBGMMap();
    initialized_ = true;
}


void AudioManager::bindRuntimeState(AudioRuntimeState& newState) {
    activeState_ = &newState;
}

void AudioManager::useFallbackRuntimeState() {
    activeState_ = &fallbackState_;
}

AudioRuntimeState& AudioManager::state() {
    return *activeState_;
}

void AudioManager::loadBGMMap() {
    bgmMap_.clear();
    bgmLoopRegions.clear();
    const auto bgmManifestPath = content_paths::x1AudioBgmManifestPath();
    std::ifstream f(bgmManifestPath ? *bgmManifestPath : "");
    if (!f.is_open()) return;
    try {
        nlohmann::json j;
        f >> j;
        for (auto it = j.begin(); it != j.end(); ++it) {
            // Keys beginning with _ are comment fields (e.g. "_comment").
            if (it.key().empty() || it.key()[0] == '_') continue;
            if (it.value().is_string()) {
                const std::string stem = it.value().get<std::string>();
                if (!content_paths::isSafeStem(stem)) {
                    TraceLog(LOG_WARNING, "Skipping BGM map entry for %s: unsafe track stem", it.key().c_str());
                    continue;
                }
                bgmMap_[it.key()] = stem;
                continue;
            }
            if (!it.value().is_object()) {
                TraceLog(LOG_WARNING, "Skipping invalid BGM map entry for %s", it.key().c_str());
                continue;
            }

            const auto& entry = it.value();
            const auto availableIt = entry.find("available");
            if (availableIt != entry.end()) {
                if (!availableIt->is_boolean()) {
                    TraceLog(LOG_WARNING, "Skipping BGM map entry for %s: available must be boolean", it.key().c_str());
                    continue;
                }
                if (!availableIt->get<bool>()) {
                    // Empty value is a deliberate "known unavailable" sentinel.
                    bgmMap_[it.key()] = "";
                    continue;
                }
            }

            const auto trackIt = entry.find("track");
            if (trackIt == entry.end() || !trackIt->is_string()) {
                TraceLog(LOG_WARNING, "Skipping BGM map entry for %s: missing string track", it.key().c_str());
                continue;
            }
            const std::string stem = trackIt->get<std::string>();
            if (!content_paths::isSafeStem(stem)) {
                TraceLog(LOG_WARNING, "Skipping BGM map entry for %s: unsafe track stem", it.key().c_str());
                continue;
            }
            bgmMap_[it.key()] = stem;
            const auto loopIt = entry.find("loop");
            if (loopIt != entry.end()) {
                BgmLoopRegion region{};
                if (!readLoopRegion(*loopIt, region)) {
                    // Keep the sentinel so a KB overlay cannot silently turn
                    // an invalid configured loop into whole-file playback.
                    bgmMap_[it.key()] = "";
                    TraceLog(LOG_WARNING, "Skipping BGM %s: invalid loop region", it.key().c_str());
                    continue;
                }
                bgmLoopRegions[it.key()] = region;
            }
        }
    } catch (const std::exception& e) {
        TraceLog(LOG_WARNING, "Failed to parse %s: %s",
                 bgmManifestPath ? bgmManifestPath->c_str() : "content/x1/audio/bgm.json",
                 e.what());
    }

    // Overlay KB stage_bgm.json — fills in any stage→track mappings not
    // already present from the content pack. KB data is ROM-verified.
    auto kbBgmPath = kb_paths::resolveKBPath("mmx1/audio/stage_bgm.json");
    std::ifstream kbf(kbBgmPath ? *kbBgmPath : "");
    if (kbf.is_open()) {
        try {
            nlohmann::json kj;
            kbf >> kj;
            int added = 0;
            if (kj.contains("stages") && kj["stages"].is_object()) {
                for (auto it = kj["stages"].begin(); it != kj["stages"].end(); ++it) {
                    const auto& entry = it.value();
                    std::string stageName = entry.value("stage_name", "");
                    std::string track = entry.value("bgm_track", "");
                    if (stageName.empty() || track.empty()) continue;
                    // Convert underscore stage names to hyphenated (KB uses
                    // "chill_penguin", engine uses "chill-penguin")
                    std::replace(stageName.begin(), stageName.end(), '_', '-');
                    if (bgmMap_.find(stageName) == bgmMap_.end()) {
                        bgmMap_[stageName] = track;
                        ++added;
                    }
                }
            }
            if (kj.contains("non_stage_bgm") && kj["non_stage_bgm"].is_object()) {
                for (auto it = kj["non_stage_bgm"].begin(); it != kj["non_stage_bgm"].end(); ++it) {
                    if (it.key().empty() || it.key()[0] == '$') continue;
                    if (!it.value().is_string()) continue;
                    std::string track = it.value().get<std::string>();
                    if (bgmMap_.find(it.key()) == bgmMap_.end()) {
                        bgmMap_[it.key()] = track;
                        ++added;
                    }
                }
            }
            if (added > 0) {
                TraceLog(LOG_INFO, "Audio: added %d BGM mappings from KB stage_bgm.json", added);
            }
        } catch (const std::exception& e) {
            TraceLog(LOG_WARNING, "Failed to parse KB stage_bgm.json: %s", e.what());
        }
    }
}

void AudioManager::shutdown() {
    if (!initialized_) return;

    stopBGM();

    bgmCache_.clear();

    sfxCache_.clear();

    apuCache_.clear();

    initialized_ = false;
    playbackEnabled_ = false;
}

void AudioManager::playBGM(const std::string& name, bool looping) {
    // Request diagnostics remain available in headless runs without opening
    // an audio device. They are not a receipt of audible playback.
    TraceLog(LOG_INFO, "Audio: BGM request frame=%d name=%s looping=%d",
             state().apuFrame, name.c_str(), looping ? 1 : 0);
    if (!playbackEnabled_) return;
    if (name == currentBGMName_ && bgmPlaying_ && looping && bgmLoopStream.active()) return;
    if (name == currentBGMName_ && bgmPlaying_ && currentBGM_ &&
        currentBGM_->get().looping == looping &&
        (looping || IsMusicStreamPlaying(currentBGM_->get()))) return;

    stopBGM();

    // Resolve via bgm.json map first. If the caller passed a stage id
    // (e.g. "chill-penguin") and the map has a track stem for it, use that;
    // otherwise fall through to treating `name` as the track stem directly.
    std::string trackStem = name;
    auto mapIt = bgmMap_.find(name);
    if (mapIt != bgmMap_.end()) {
        if (mapIt->second.empty()) return;
        trackStem = mapIt->second;
    }

    // Cache key is the resolved stem so two stage ids pointing at the same
    // track (e.g. every sigma room) share one loaded music stream.
    if (!content_paths::isSafeStem(trackStem)) {
        TraceLog(LOG_WARNING, "AudioManager: rejecting unsafe BGM stem '%s'", trackStem.c_str());
        return;
    }
    const auto region = bgmLoopRegions.find(name);
    if (looping && region != bgmLoopRegions.end()) {
        const auto path = content_paths::x1BgmTrackPath(trackStem, ".wav");
        if (!path || !bgmLoopStream.start(*path, region->second, masterVol_ * musicVol_)) return;
        currentBGMName_ = name;
        bgmPlaying_ = true;
        applyMusicVolume();
        return;
    }
    auto it = bgmCache_.find(trackStem);
    if (it != bgmCache_.end()) {
        currentBGM_ = &it->second;
    } else {
        MusicResource music;
        auto path = content_paths::x1BgmTrackPath(trackStem, ".ogg");
        if (path && !music.load(*path)) {
            // Try .wav fallback
            path = content_paths::x1BgmTrackPath(trackStem, ".wav");
            if (path) {
                music.load(*path);
            }
        }
        if (!music.valid()) return; // No file found

        auto inserted = bgmCache_.emplace(trackStem, std::move(music));
        currentBGM_ = &inserted.first->second;
    }

    // Apply on both loads and cache hits; a presentation is a one-shot song.
    currentBGM_->setLooping(looping);
    currentBGMName_ = name;
    bgmPlaying_ = true;
    applyMusicVolume();
    PlayMusicStream(currentBGM_->get());
}

void AudioManager::stopBGM() {
    TraceLog(LOG_INFO, "Audio: BGM stop request frame=%d", state().apuFrame);
    if (playbackEnabled_ && bgmPlaying_ && currentBGM_) {
        StopMusicStream(currentBGM_->get());
    }
    bgmLoopStream.reset();
    bgmPlaying_ = false;
    currentBGMName_.clear();
    currentBGM_ = nullptr;
}

void AudioManager::pauseBGM() {
    if (playbackEnabled_ && bgmPlaying_) bgmLoopStream.pause();
    if (playbackEnabled_ && bgmPlaying_ && currentBGM_) {
        PauseMusicStream(currentBGM_->get());
    }
}

void AudioManager::resumeBGM() {
    if (playbackEnabled_ && bgmPlaying_) bgmLoopStream.resume();
    if (playbackEnabled_ && bgmPlaying_ && currentBGM_) {
        ResumeMusicStream(currentBGM_->get());
    }
}

void AudioManager::playSFX(SFX id) {
    int key = static_cast<int>(id);
    auto& runtime = state();
    runtime.sfxLog.push_back({runtime.apuFrame, key});
    if (!playbackEnabled_) return;
    auto it = sfxCache_.find(key);
    if (it == sfxCache_.end()) {
        SoundResource sound = loadSFX(id);
        if (!sound.valid()) return; // File not found
        auto inserted = sfxCache_.emplace(key, std::move(sound));
        it = inserted.first;
    }

    float vol = masterVol_ * sfxVol_;
    it->second.setVolume(vol);
    it->second.play();
}

void AudioManager::setChargeLoop(bool active) {
    // The real game commits ONE 0x03 to the mailbox at the charge catch
    // (hold+31, _buster_runs/FINDINGS.md) and the SPC sustains it — log the
    // edge so contract tests can pin the cadence even headless.
    auto& runtime = state();
    if (active && !runtime.chargeLoopActive) {
        runtime.apuLog.push_back({runtime.apuFrame, 0x03});
    }
    runtime.chargeLoopActive = active;
    if (!playbackEnabled_) return;

    int key = static_cast<int>(SFX::ChargeLoop);
    auto it = sfxCache_.find(key);
    if (it == sfxCache_.end()) {
        SoundResource sound = loadSFX(SFX::ChargeLoop);
        if (!sound.valid()) return;
        it = sfxCache_.emplace(key, std::move(sound)).first;
    }
    if (active) {
        it->second.setVolume(masterVol_ * sfxVol_);
        if (!it->second.isPlaying()) it->second.play();  // (re)start to keep it looping
    } else {
        it->second.stop();                                // ends on release
    }
}

namespace {
// Fallback: measured APU command byte -> matching shipped wav, until ripped
// per-id samples land at the x1ApuSfxPath location. Distinct special-weapon
// commands stay silent instead of sharing invented generic audio. Followup bytes
// (0x02/0x04/0x05/0x2A) stay silent — the shared 0x17 release wav already
// carries the audible cue; doubling it would stack two release sounds.
SFX apuFallback(int command, bool& hasFallback) {
    hasFallback = true;
    switch (command) {
        case 0x01: return SFX::BusterShot;      // buster lemon / ice pellet
        case 0x17: return SFX::BusterCharged;   // shared charged-release base
        // R6 2026-09-06: X's hurt cry. Measured as $09 in BOTH movie
        // harvests (knowledge_base/mmx1/audio/player_hurt_sfx_movies.json)
        // and in sfx_table.json since July. No apu_09.wav is ripped yet,
        // so the existing player_hurt sample carries it.
        case 0x09: return SFX::PlayerHurt;
        // R6.07 2026-09-08: landing's measured command from the plain-v2
        // control (build/luna-session17/r6-controlled-mode/plain-v2-run1),
        // recorded in docs/plans/2026-09-06-recordings-track.md.  No
        // apu_07.wav is ripped yet, so keep the existing Landing sample as
        // the fallback.
        case 0x07: return SFX::Landing;
        // R6 2026-09-07: the boss/enemy hit impact. sfx_table.json has
        // carried $11 for this action since July; the movies put it on the
        // frame of 36 of 42 boss HP drops and inside [-4, +1] for the rest,
        // with none missing (knowledge_base/mmx1/audio/boss_hit_sfx_movies.json).
        // No apu_11.wav is ripped, so the engine's own hit sample carries it.
        case 0x11: return SFX::EnemyHit;
        // CP-ENTRY-V4 owner-approved temporary door/mechanism stand-ins.
        // Keep the measured APU ids in the log; do not promote sfx_table.json.
        case 0x40: return SFX::WallSlide;       // shutter actuation
        case 0x41: return SFX::Landing;         // room-seal thunk
        case 0x78: return SFX::Hit;             // Shotgun Ice interactive shatter
        // CP-B8 death choreography stand-ins until FA3 rips the authentic
        // samples: $13 boss death cry, $93..$96 explosion pop family.
        // $13 is the boss's CRY, not only its death: both movies put it one
        // frame after most HP drops AND on the death frame. It is left
        // mapped here and NOT emitted per hit, because Chill Penguin's six
        // 3-damage drops carry no $13 and the exception has no rule yet.
        case 0x13: return SFX::BossDeath;
        case 0x93:
        case 0x94:
        case 0x95:
        case 0x96: return SFX::EnemyDeath;
        default:
            hasFallback = false;
            return SFX::COUNT;
    }
}
} // namespace

void AudioManager::playApuNow(int commandByte) {
    auto& runtime = state();
    runtime.apuLog.push_back({runtime.apuFrame, commandByte});

    const auto apuPath = content_paths::x1ApuSfxPath(commandByte);
    const bool directSampleAvailable =
        apuPath && std::filesystem::exists(*apuPath);
    if (!playbackEnabled_) {
        if (!directSampleAvailable) {
            bool hasFallback = false;
            const SFX fallback = apuFallback(commandByte, hasFallback);
            if (hasFallback) playSFX(fallback);
        }
        return;
    }

    auto it = apuCache_.find(commandByte);
    if (it == apuCache_.end()) {
        SoundResource sound;
        if (directSampleAvailable) {
            sound.load(*apuPath);
        }
        it = apuCache_.emplace(commandByte, std::move(sound)).first;
    }
    if (it->second.valid()) {
        it->second.setVolume(masterVol_ * sfxVol_);
        it->second.play();
        return;
    }

    bool hasFallback = false;
    const SFX fb = apuFallback(commandByte, hasFallback);
    if (hasFallback) playSFX(fb);
}

void AudioManager::playApu(int commandByte) {
    if (commandByte < 0) return;   // weapon.json null (e.g. silent events)
    playApuNow(commandByte);
}

void AudioManager::playApuDelayed(int commandByte, int delayFrames) {
    if (commandByte < 0) return;
    if (delayFrames <= 0) {
        playApuNow(commandByte);
        return;
    }
    auto& runtime = state();
    runtime.apuPending.push_back({runtime.apuFrame + delayFrames, commandByte});
}

void AudioManager::clearApuLog() {
    auto& runtime = state();
    runtime.apuLog.clear();
    runtime.sfxLog.clear();
    runtime.apuPending.clear();
    runtime.apuFrame = 0;
    runtime.chargeLoopActive = false;
}

void AudioManager::setMasterVolume(float v) {
    masterVol_ = std::clamp(v, 0.0f, 1.0f);
    applyMusicVolume();
}

void AudioManager::setMusicVolume(float v) {
    musicVol_ = std::clamp(v, 0.0f, 1.0f);
    applyMusicVolume();
}

void AudioManager::setSFXVolume(float v) {
    sfxVol_ = std::clamp(v, 0.0f, 1.0f);
}

void AudioManager::update() {
    if (playbackEnabled_ && bgmPlaying_) bgmLoopStream.update();
    if (playbackEnabled_ && bgmPlaying_ && currentBGM_) {
        UpdateMusicStream(currentBGM_->get());
    }

    // Delayed-APU queue (release followups land 1 frame after the 0x17):
    // flush BEFORE advancing the clock — a +1 command queued during frame N
    // (due n+1) stays queued through frame N's update and plays during
    // frame N+1's, logged at frame n+1.
    auto& runtime = state();
    for (size_t i = 0; i < runtime.apuPending.size();) {
        if (runtime.apuPending[i].dueFrame <= runtime.apuFrame) {
            const int cmd = runtime.apuPending[i].command;
            runtime.apuPending.erase(runtime.apuPending.begin() + static_cast<long>(i));
            playApuNow(cmd);
        } else {
            ++i;
        }
    }
    runtime.apuFrame++;
}

void AudioManager::applyMusicVolume() {
    if (playbackEnabled_ && bgmPlaying_) bgmLoopStream.volume(masterVol_ * musicVol_);
    if (playbackEnabled_ && bgmPlaying_ && currentBGM_) {
        SetMusicVolume(currentBGM_->get(), masterVol_ * musicVol_);
    }
}

SoundResource AudioManager::loadSFX(SFX id) {
    std::string path = sfxPath(id);
    if (path.empty()) return {};
    auto resolvedPath = content_paths::resolveAssetPath(path);
    if (!resolvedPath || !std::filesystem::exists(*resolvedPath)) return {};
    SoundResource sound;
    sound.load(*resolvedPath);
    return sound;
}

std::string AudioManager::sfxPath(SFX id) {
    static const char* names[] = {
        "buster_shot",      // BusterShot
        "buster_charged",   // BusterCharged
        "charge_loop",      // ChargeLoop
        "special_weapon",   // SpecialWeapon
        "hit",              // Hit
        "enemy_hit",        // EnemyHit
        "enemy_death",      // EnemyDeath
        "enemy_shoot",      // EnemyShoot (walker shot launch, raw 0x33, U17)
        "player_hurt",      // PlayerHurt
        "player_death",     // PlayerDeath
        "jump",             // Jump
        "dash",             // Dash
        "wall_slide",       // WallSlide
        "landing",          // Landing
        "pickup",           // Pickup
        "health_restore",   // HealthRestore
        "extra_life",       // ExtraLife
        "heart_tank",       // HeartTank
        "sub_tank",         // SubTank
        "boss_intro",       // BossIntro
        "boss_death",       // BossDeath
        "stage_clear",      // StageClear
        "menu_select",      // MenuSelect
        "menu_move",        // MenuMove
        "menu_cancel",      // MenuCancel
        "pause",            // Pause
        "weapon_switch",    // WeaponSwitch
    };

    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(SFX::COUNT)) return "";

    auto path = content_paths::x1SfxPath(names[idx]);
    return path.value_or("");
}

} // namespace mmx
