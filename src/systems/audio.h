// audio.h - declares audio IDs, runtime event state, and AudioManager.
// Owns: SFX/APU event records, volume state, and BGM/SFX control API.

#pragma once

#include "systems/raylib_resource.h"
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace mmx {

// Sound effect IDs — use these in gameplay code
enum class SFX {
    BusterShot,
    BusterCharged,
    ChargeLoop,
    SpecialWeapon,
    Hit,
    EnemyHit,
    EnemyDeath,
    EnemyShoot,    // walker 0x51 shot launch (raw APU 0x33, U17)
    PlayerHurt,
    PlayerDeath,
    Jump,
    Dash,
    WallSlide,
    Landing,
    Pickup,
    HealthRestore,
    ExtraLife,
    HeartTank,
    SubTank,
    BossIntro,
    BossDeath,
    StageClear,
    MenuSelect,
    MenuMove,
    MenuCancel,
    Pause,
    WeaponSwitch,
    COUNT
};

struct AudioApuEvent { int frame; int command; };
struct AudioSfxEvent { int frame; int id; };
struct AudioPendingApu { int dueFrame; int command; };

struct AudioRuntimeState {
    std::vector<AudioApuEvent> apuLog;
    std::vector<AudioSfxEvent> sfxLog;
    std::vector<AudioPendingApu> apuPending;
    int apuFrame = 0;
    bool chargeLoopActive = false;
};

class AudioManager {
public:
    using ApuEvent = AudioApuEvent;
    using SfxEvent = AudioSfxEvent;
    static void init(bool playbackEnabled);
    static void shutdown();
    static void bindRuntimeState(AudioRuntimeState& state);
    static void useFallbackRuntimeState();
    static bool playbackEnabled() { return playbackEnabled_; }

    // BGM — only one plays at a time
    static void playBGM(const std::string& name, bool looping = true);
    static void stopBGM();
    static void pauseBGM();
    static void resumeBGM();

    // SFX — pooled, max 3 of same sound simultaneously
    static void playSFX(SFX id);

    // APU-id playback (U41): weapon events carry the MEASURED SNES APU
    // command byte (knowledge_base weapon.json "sfx" blocks). Resolution:
    //   1. content/x1/audio/sfx/apu_<hex>.wav — a ripped real sample
    //   2. the small table of command-matching shared samples (0x01/0x17/0x78)
    //   3. silent (e.g. release followups until real samples land — the
    //      shared 0x17 release wav carries the audible cue meanwhile).
    // Every call (immediate or delayed) is appended to apuLog() at its
    // PLAY frame — headless contract tests assert command sequences; the
    // log works without an audio device.
    static void playApu(int commandByte);
    // Queue the command N update() ticks ahead (the measured release pairs
    // are 1 frame apart: 0x17 at release, the weapon followup at +1).
    static void playApuDelayed(int commandByte, int delayFrames);
    static const std::vector<ApuEvent>& apuLog() { return state().apuLog; }
    static const std::vector<SfxEvent>& sfxLog() { return state().sfxLog; }
    static void clearApuLog();

    // Held charge whirr: call every frame with whether X is charging. Starts
    // (and re-triggers to loop) while active, and stops the instant it's false
    // — so the sound ends when the shot is released. The real game sends ONE
    // 0x03 command at the charge catch and the SPC sustains it — the wav loop
    // is the engine's sustain model; the false->true edge logs apu 0x03.
    static void setChargeLoop(bool active);

    // Volume (0.0 - 1.0)
    static void setMasterVolume(float v);
    static void setMusicVolume(float v);
    static void setSFXVolume(float v);

    static float masterVolume() { return masterVol_; }
    static float musicVolume() { return musicVol_; }
    static float sfxVolume() { return sfxVol_; }

    // Call every frame to handle fade transitions + the delayed-APU queue
    // (advances the apuLog frame clock; headless tests call this per
    // simulated frame).
    static void update();

private:
    static std::unordered_map<std::string, MusicResource> bgmCache_;
    static std::unordered_map<int, SoundResource> sfxCache_;
    // APU-id layer (U41)
    static std::unordered_map<int, SoundResource> apuCache_;
    static AudioRuntimeState fallbackState_;
    static AudioRuntimeState* activeState_;
    static AudioRuntimeState& state();
    static void playApuNow(int commandByte);

    // stage-id -> track file stem (from content/x1/audio/bgm.json). Lets the
    // engine play "chill-penguin" while the OGG on disk is "05_chill_penguin".
    static std::unordered_map<std::string, std::string> bgmMap_;

    static MusicResource* currentBGM_;
    static std::string currentBGMName_;
    static bool bgmPlaying_;

    static float masterVol_;
    static float musicVol_;
    static float sfxVol_;

    static bool initialized_;
    static bool playbackEnabled_;

    static void applyMusicVolume();
    static void loadBGMMap();
    static SoundResource loadSFX(SFX id);
    static std::string sfxPath(SFX id);
};

} // namespace mmx
