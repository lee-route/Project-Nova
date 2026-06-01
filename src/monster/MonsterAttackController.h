#pragma once

#include "combat/AttackData.h"
#include "combat/Hitbox.h"
#include "core/Vector2.h"

#include <vector>

namespace nova {

class CombatSystem;
class ICombatant;
struct AttackResult;

// The three phases of a single melee attack.
//   Ready    -> idle, allowed to start a new attack
//   Active   -> swing hitbox is "live"; it can deal damage this frame
//   Cooldown -> recovering; cannot attack yet
enum class AttackPhase {
    Ready,
    Active,
    Cooldown
};

// Owns the *attacking behaviour* of a single monster as a small state machine:
//   - opens an "active window" during which a melee hitbox lives IN FRONT of
//     the monster, following its facing direction
//   - damages each target at most ONCE per attack (tracked by id)
//   - runs a cooldown after the window closes
//
// It knows nothing about rendering. A renderer/debug layer may *read* the live
// hitbox via hasActiveHitbox()/activeHitbox(), but cannot change combat state.
//
// Boss-ready: attacks are data-driven (AttackData). A boss AI can swap attacks
// per swing using the tryStartAttack() overload that takes an AttackData.
class MonsterAttackController {
public:
    MonsterAttackController() = default;
    MonsterAttackController(const AttackData& data, int ownerId);

    void setAttackData(const AttackData& data) { attackData_ = data; }
    void setOwnerId(int ownerId) { ownerId_ = ownerId; }
    const AttackData& attackData() const { return attackData_; }

    // Per-frame tick. Advances the state machine and, while Active, refreshes
    // the hitbox from the CURRENT pose and applies once-per-attack damage.
    // Call this every frame BEFORE tryStartAttack().
    void update(float deltaSeconds,
                CombatSystem& combat,
                const Vector2& monsterCenter,
                const Vector2& facing,
                const std::vector<ICombatant*>& targets);

    // Request to begin an attack. Succeeds only when Ready AND the target is in
    // range. Returns true if the active window opened this call. Safe to call
    // every frame: it is a no-op while Active or on Cooldown.
    bool tryStartAttack(const Vector2& monsterCenter,
                        const Vector2& facing,
                        const Vector2& targetCenter);

    // Boss helper: choose which attack to use for THIS swing, then start it.
    bool tryStartAttack(const AttackData& data,
                        const Vector2& monsterCenter,
                        const Vector2& facing,
                        const Vector2& targetCenter);

    // --- state queries (safe for AI and HUD) ---
    AttackPhase phase() const { return phase_; }
    bool isReady() const { return phase_ == AttackPhase::Ready; }
    bool isActive() const { return phase_ == AttackPhase::Active; }
    bool isOnCooldown() const { return phase_ == AttackPhase::Cooldown; }
    float activeRemaining() const { return activeRemaining_; }
    float cooldownRemaining() const { return cooldownRemaining_; }

    // Pure range check, no side effects.
    bool isTargetInRange(const Vector2& monsterCenter,
                         const Vector2& targetCenter) const;

    // --- rendering/debug ONLY (read-only) ---
    bool hasActiveHitbox() const { return phase_ == AttackPhase::Active; }
    Hitbox activeHitbox() const { return currentHitbox_; }

private:
    Hitbox buildMeleeHitbox(const Vector2& monsterCenter,
                            const Vector2& facing) const;
    void applyActiveWindowDamage(CombatSystem& combat,
                                 const std::vector<ICombatant*>& targets);

    AttackData attackData_{};
    int ownerId_ = -1;

    AttackPhase phase_ = AttackPhase::Ready;
    float activeRemaining_ = 0.0f;
    float cooldownRemaining_ = 0.0f;

    // The swing hitbox for the current attack, refreshed each active frame.
    Hitbox currentHitbox_{};
    // Ids already damaged by the current attack (enforces once-per-attack).
    std::vector<int> alreadyHitIds_;
};

} // namespace nova
