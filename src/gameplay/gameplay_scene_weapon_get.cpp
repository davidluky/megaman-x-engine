// gameplay_scene_weapon_get.cpp - owns the post-boss weapon-get sequence lane
// (GC2.1b). The measured law lives in gameplay/weapon_get_timeline.h and the
// draw plan in gameplay/weapon_get_presentation.h; this file is the scene's
// side of it: entering the sequence, dispatching its cues, and choosing what
// the stage-clear frame draws.
//
// Presentation only. Nothing here moves player state - the actor is drawn
// through the position-swap idiom the scene already uses for every entity - so
// parity traces are unaffected.

#include "gameplay/gameplay_scene.h"

#include "gameplay/gameplay_presentation.h"
#include "gameplay/weapon_get_presentation.h"
#include "data/save_system.h"
#include "systems/audio.h"

namespace mmx {

// Loads the sequence with what the source proved is per-weapon: the number of
// visible glyphs (silent row breaks do not emit text blips), and the weapon's
// own fire command for the demo. The choreography itself is fixed, because it
// was identical on both recorded bosses.
void GameplayScene::beginWeaponGetSequence() {
    weaponGetParams_ = weapon_get_presentation::Params{};
    weaponGetText_.clear();
    if (!awardedWeapon_.has_value() || bossRushMode) return;

    weaponGetText_ = gameplay_presentation::weaponGetText(*awardedWeapon_);
    weaponGetParams_.timing.textBlipCount =
        weapon_get_presentation::glyphCount(weaponGetText_);

    // The movie showed $01 for Shotgun Ice and $63 for Storm Tornado; KB
    // weapon.json sfx.fire independently says 1 and 99 for exactly those two.
    // That agreement is what lets every weapon use its own measured command
    // instead of one shared placeholder. Fire Wave's is measured SILENT (-1).
    weaponGetParams_.demoApuCommand = awardedWeapon_->sfxFire;

    const auto demoPattern =
        weapon_get_presentation::demoPatternForWeapon(awardedWeapon_->id);
    weaponGetParams_.timing.demoShotCount = demoPattern.shotCount;
    weaponGetParams_.timing.demoShotStepTicks = demoPattern.shotStepTicks;
    weaponGetParams_.demoPatternMeasured = demoPattern.measured;
}

// Capture entry. Enters the sequence exactly as a boss death does - awarded
// weapon, stage clear, tick 0 - without needing a scripted kill. The
// progression side effects of a real clear are deliberately NOT applied.
void GameplayScene::startWeaponGetSequenceForCapture() {
    const std::string bossKey = boss_.type.empty() ? activeStageId_.str() : boss_.type;
    awardedWeapon_ = weapons::awardForBoss(BossId::fromString(bossKey));
    if (!awardedWeapon_.has_value()) return;

    stageClear_ = true;
    stageClearTimer_ = 0;
    stageClearFrozen_ = false;
    hud_.hideBossHP();
    // The stage-clear branch returns before the fade-in decrement, so entering
    // the sequence on frame 0 would otherwise hold the stage fade-in at full
    // black forever. In the real flow it has long since expired.
    fadeInTimer_ = 0;
    beginWeaponGetSequence();
}

bool GameplayScene::weaponGetSequenceActive() const {
    // stageClearTimer_ is incremented before the tick is read, so tick 0 needs
    // timer 1. Requiring that here also keeps the boss-death frame itself - when
    // the flag is set but no tick has elapsed - on the ordinary path, instead of
    // blinking X off for one frame at tick -1.
    return stageClear_ && stageClearTimer_ >= 1 && !weaponGetText_.empty() &&
           awardedWeapon_.has_value();
}

weapon_get_presentation::Plan GameplayScene::weaponGetPlan() const {
    return weapon_get_presentation::plan(weaponGetTick(), weaponGetParams_,
                                         weapon_get_presentation::glyphCount(
                                             weaponGetText_));
}

// One APU dispatch per tick, on the source's own schedule, and the source's own
// return handoff.
void GameplayScene::updateWeaponGetSequence() {
    if (!weaponGetSequenceActive()) return;

    const weapon_get_presentation::Plan plan = weaponGetPlan();
    if (plan.apuCue >= 0) AudioManager::playApu(plan.apuCue);
    if (plan.handoffReady) {
        // Both recorded awards have source-backed no-live-actor tails. Keep
        // each password-grid handoff on screen until its own last captured
        // frame; the other awards still use the existing immediate handoff
        // because no equivalent source tail was recorded.
        const bool chillPenguin = awardedWeapon_->id == "shotgun-ice";
        const bool stormEagle = awardedWeapon_->id == "storm-tornado";
        const bool hasSourceHandoff = chillPenguin || stormEagle;
        const int sourceHandoffEnd = chillPenguin
            ? weapon_get_timeline::kSourceHandoffEndTick
            : weapon_get_timeline::kStormSourceHandoffEndTick;
        if (!hasSourceHandoff || weaponGetTick() >= sourceHandoffEnd) {
            returnToStageSelect_ = true;
        }
    }
}

// In the arena phase X poses and then warps out on the measured {10, 11} px
// ladder. Afterwards he belongs to the sequence's own screens, which are drawn
// over the world pass, so the world stops drawing him.
GameplayScene::WeaponGetActorDraw GameplayScene::weaponGetActorDraw() const {
    WeaponGetActorDraw draw;
    if (!weaponGetSequenceActive()) return draw;

    const weapon_get_presentation::Plan plan = weaponGetPlan();
    const bool inArena = plan.screen == weapon_get_presentation::Screen::Arena;
    draw.hidePlayer = !inArena || !plan.drawActor;
    if (inArena) draw.arenaOffsetY = plan.actorOffsetY;
    return draw;
}

// X on the native demo screen: warps down 128 px at exactly 8 px/tick, then
// holds the pose while the weapon fires. The native spec fallback also shows
// a live preview in the left panel. Both are drawn after the screen because
// the presentation pass is a full-frame fill.
void GameplayScene::renderWeaponGetActor(float alpha) {
    if (!weaponGetSequenceActive()) return;
    const weapon_get_presentation::Plan plan = weaponGetPlan();
    const bool nativeSpecPreview =
        plan.screen == weapon_get_presentation::Screen::SpecText &&
        gameplay_presentation::weaponGetNativeFallbackPanelVisible(
            weaponGetTick()) &&
        !gameplay_presentation::hasWeaponGetSourceScreen(
            plan.screen, *awardedWeapon_);
    if ((!plan.drawActor && !nativeSpecPreview) ||
        (plan.screen != weapon_get_presentation::Screen::Demo &&
         !nativeSpecPreview)) {
        return;
    }

    // The measured source demo framebuffer already owns the complete screen,
    // including the small X pose. Do not draw the engine player over it.
    if (gameplay_presentation::hasWeaponGetSourceScreen(plan.screen,
                                                        *awardedWeapon_)) {
        return;
    }

    const auto layout = gameplay_presentation::weaponGetScreenLayout();
    const Vector2 savedPos = player_.position;
    const Vector2 savedPrev = player_.prevPosition;
    const Vector2 pose = nativeSpecPreview
        ? Vector2{34.0f, 92.0f}
        : Vector2{static_cast<float>(layout.demoActorX),
                  static_cast<float>(layout.demoActorY + plan.actorOffsetY)};
    player_.position = pose;
    player_.prevPosition = pose;
    player_.render(alpha);
    player_.position = savedPos;
    player_.prevPosition = savedPrev;
}

// The stage-clear frame. During the weapon-get sequence the arena phase keeps
// the stage-clear banner (engine chrome: clear time and best time), and from
// the source's first no-live-actor run on, the sequence's own screens take the
// whole frame. Without a weapon award nothing changes.
void GameplayScene::renderStageClearPresentation(float alpha) {
    const int bestTime = SaveSystem::getBestTime(activeStageId_);

    if (!weaponGetSequenceActive()) {
        gameplay_presentation::renderStageClearOverlay(
            stageClear_, stageClearTimer_, stageTimer_, bossActive_, bestTime,
            boss_.isRescued(), awardedWeapon_);
        return;
    }

    const weapon_get_presentation::Plan plan = weaponGetPlan();
    if (plan.screen == weapon_get_presentation::Screen::Arena) {
        gameplay_presentation::renderStageClearBanner(
            stageClearTimer_, stageTimer_, bossActive_, bestTime);
        return;
    }

    gameplay_presentation::renderWeaponGetScreen(
        weaponGetTick(), weaponGetParams_, plan, weaponGetText_, *awardedWeapon_);
    renderWeaponGetActor(alpha);
}

} // namespace mmx
