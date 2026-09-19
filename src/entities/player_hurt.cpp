// Damage/action ownership measured in player/hurt_entry_2026-09-19.
#include "entities/player.h"
#include "data/difficulty.h"
#include "systems/audio.h"
#include <algorithm>

namespace mmx {

void Player::takeDamage(int amount, float knockbackDirX) {
    if (isInvulnerable()) return;
    amount = std::max(1, static_cast<int>(amount * DifficultySettings::playerDamageMultiplier()));
    if (hasArmor) amount = std::max(1, (amount + 1) / 2);
    health -= amount;
    // Taking damage preserves the accumulated Buster charge.
    if (health <= 0) {
        forceDeath();
        return;
    }
    hurtInitPending_ = true;
    hurtRecoveryPending_ = false;
    hurtEntryFrame_ = false;
    hurtKnockbackDirection_ = knockbackDirX < 0 ? -1.0f : 1.0f;
    iframeTimer_ = std::max(1, iframeDuration - (hasArmor ? 1 : 0));
    // 84:9D2E writes action0E/do0, retaining the interrupted pose/speed.
    changeState(PlayerState::Hurt);
}

void Player::forceDeath() {
    health = 0;
    deathTimer_ = 0;
    hurtInitPending_ = false;
    hurtRecoveryPending_ = false;
    hurtEntryFrame_ = false;
    velocity = {0, 0};
    changeState(PlayerState::Die);
}

bool Player::updateHurtTransition() {
    if (hurtInitPending_ && state_ == PlayerState::Hurt) {
        hurtInitPending_ = false;
        hurtEntryFrame_ = true;
        velocity = {hurtKnockbackDirection_ * hurtKnockbackX,
                    hurtKnockbackY * (hasArmor ? 0.5f : 1.0f)};
        hurtTimer_ = std::max(1, hurtDuration - (hasArmor ? 1 : 0));
        anim_.play("hurt");
        AudioManager::playApu(0x09);
        // 81:8664..86A2 returns before animation advancement or integration.
        prevPosition = position;
        return true;
    }
    if (hurtRecoveryPending_) {
        hurtRecoveryPending_ = false;
        // Grounded action0/do0 was selected on the preceding held row.
        velocity = {0, 0};
        anim_.play("idle");
        hurtEntryFrame_ = true;
        prevPosition = position;
        return true;
    }
    return false;
}

void Player::updateHurt() {
    if (--hurtTimer_ > 0) return;
    // The completed animation changes action before ordinary movement can
    // resume. Grounded recovery retains the old pose/speed for this row.
    if (onGround) {
        AudioManager::playApu(0x07); // 81:86BE..86C4; CP mailbox at2282.
        state_ = PlayerState::Idle;
        hurtRecoveryPending_ = true;
    } else {
        changeState(PlayerState::Fall);
    }
    hurtEntryFrame_ = true;
    prevPosition = position;
}

} // namespace mmx
