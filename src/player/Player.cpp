#include "player/Player.h"

namespace nova {

Player::Player(int id, const Vector2& position, float maxHealth)
    : id_(id), position_(position), health_(maxHealth) {}

Hitbox Player::bodyHitbox() const {
    Hitbox box;
    box.w = bodyWidth_;
    box.h = bodyHeight_;
    box.x = position_.x - box.w * 0.5f;
    box.y = position_.y - box.h * 0.5f;
    return box;
}

void Player::applyDamage(const DamageInfo& info) {
    // Rule 1: the dead take no further damage.
    if (!health_.isAlive()) {
        return;
    }
    // Rule 2: ignore hits during the invulnerability window (i-frames).
    if (isInvulnerable()) {
        return;
    }

    // HP is reduced ONLY through the HealthComponent (single source of truth).
    health_.applyDamage(info);
    lastHitBy_ = info.sourceId;

    // Start the visible "got hit" reaction and the i-frame window.
    // These are gameplay flags only; the renderer reads them (flash, knockback).
    beginHitReaction(info);
    beginInvulnerability();

    // Death is checked AFTER the damage is applied.
    if (!health_.isAlive()) {
        handleDeath(info);
    }
}

void Player::update(float deltaSeconds) {
    if (invulnRemaining_ > 0.0f) {
        invulnRemaining_ -= deltaSeconds;
        if (invulnRemaining_ < 0.0f) {
            invulnRemaining_ = 0.0f;
        }
    }
    if (hitReactionRemaining_ > 0.0f) {
        hitReactionRemaining_ -= deltaSeconds;
        if (hitReactionRemaining_ < 0.0f) {
            hitReactionRemaining_ = 0.0f;
        }
    }
}

void Player::beginHitReaction(const DamageInfo& /*info*/) {
    // Keep this data-only: just open a timed window. A renderer can flash the
    // sprite while isInHitReaction() is true; a movement module could apply
    // knockback using lastHitBy() if desired.
    hitReactionRemaining_ = hitReactionDuration_;
}

void Player::beginInvulnerability() {
    invulnRemaining_ = invulnDuration_;
}

void Player::handleDeath(const DamageInfo& /*info*/) {
    if (deathHandled_) {
        return;
    }
    deathHandled_ = true;
    // Clear active reaction/i-frames; "dead" is now the state other systems
    // (animation, game-over, respawn) react to via isDead().
    hitReactionRemaining_ = 0.0f;
    invulnRemaining_ = 0.0f;
}

} // namespace nova
