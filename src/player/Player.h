#pragma once

#include "combat/HealthComponent.h"
#include "combat/ICombatant.h"
#include "core/Vector2.h"

namespace nova {

// The player's COMBAT side only: position, body, health, and how it reacts to
// incoming damage. Implements IDamageable (via ICombatant) so the CombatSystem
// can hand it a DamageInfo without knowing anything about the player.
//
// Rendering / animation / input live in other modules and only READ this
// state (isInHitReaction(), isInvulnerable(), isDead(), health()).
class Player : public ICombatant {
public:
    Player(int id, const Vector2& position, float maxHealth = 100.0f);

    // --- ICombatant / IDamageable ---
    int id() const override { return id_; }
    Hitbox bodyHitbox() const override;
    bool isAlive() const override { return health_.isAlive(); }
    void applyDamage(const DamageInfo& info) override;

    // Tick i-frame and hit-reaction timers. Call once per frame.
    void update(float deltaSeconds);

    // --- movement (set by input/movement module) ---
    void setPosition(const Vector2& p) { position_ = p; }
    Vector2 position() const { return position_; }

    // --- read-only combat state (for renderer / HUD / debug) ---
    const HealthComponent& health() const { return health_; }
    bool isDead() const { return !health_.isAlive(); }
    bool isInvulnerable() const { return invulnRemaining_ > 0.0f; }
    float invulnRemaining() const { return invulnRemaining_; }
    bool isInHitReaction() const { return hitReactionRemaining_ > 0.0f; }
    float hitReactionRemaining() const { return hitReactionRemaining_; }
    int lastHitBy() const { return lastHitBy_; }

    // --- tunables (designers / different difficulties) ---
    void setInvulnerabilityDuration(float seconds) { invulnDuration_ = seconds; }
    void setHitReactionDuration(float seconds) { hitReactionDuration_ = seconds; }

private:
    void beginHitReaction(const DamageInfo& info);
    void beginInvulnerability();
    void handleDeath(const DamageInfo& info);

    int id_;
    Vector2 position_;
    float bodyWidth_ = 30.0f;
    float bodyHeight_ = 30.0f;
    HealthComponent health_;

    float invulnDuration_ = 1.0f;      // i-frames granted after a hit
    float invulnRemaining_ = 0.0f;
    float hitReactionDuration_ = 0.3f; // "got hit" reaction window
    float hitReactionRemaining_ = 0.0f;
    int lastHitBy_ = -1;               // sourceId of the most recent hit
    bool deathHandled_ = false;        // ensures death logic runs only once
};

} // namespace nova
