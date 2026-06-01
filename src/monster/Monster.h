#pragma once

#include "combat/AttackData.h"
#include "combat/HealthComponent.h"
#include "combat/ICombatant.h"
#include "core/Vector2.h"
#include "monster/MonsterAttackController.h"

#include <vector>

namespace nova {

class CombatSystem;

// Minimal monster that owns ONLY combat-relevant state:
// position, body size, health, and its attack controller.
// Sprites, animation and pathfinding live in other (teammate) modules.
//
// Implements ICombatant so it can also be a *target* of other attacks.
class Monster : public ICombatant {
public:
    Monster(int id, const Vector2& position, const AttackData& attack);

    // --- ICombatant / IDamageable ---
    int id() const override { return id_; }
    Hitbox bodyHitbox() const override;
    bool isAlive() const override { return health_.isAlive(); }
    void applyDamage(const DamageInfo& info) override { health_.applyDamage(info); }

    // Read-only health for HUD/debug (combat goes through applyDamage()).
    const HealthComponent& health() const { return health_; }

    // --- movement state (set by movement/AI modules) ---
    void setPosition(const Vector2& p) { position_ = p; }
    Vector2 position() const { return position_; }
    void setFacing(const Vector2& f) { facing_ = f; }
    Vector2 facing() const { return facing_; }

    // Per-frame combat tick. Advances the attack state machine and applies any
    // active-window damage through the CombatSystem. Call once per frame.
    void update(float deltaSeconds,
                CombatSystem& combat,
                const std::vector<ICombatant*>& targets) {
        attackController_.update(deltaSeconds, combat, position_, facing_, targets);
    }

    // Safe AI entry point: "begin an attack toward this target".
    // Faces the target, then asks the controller to open the active window.
    // Returns true only if a new attack actually started this call.
    // No-op while already swinging or on cooldown, so it is safe to spam.
    bool tryAttack(const Vector2& targetCenter);

    // Read-only access for HUD/debug; no way to mutate combat state from here.
    const MonsterAttackController& attackController() const { return attackController_; }

    // --- rendering/debug ONLY ---
    bool hasActiveAttackHitbox() const { return attackController_.hasActiveHitbox(); }
    Hitbox activeAttackHitbox() const { return attackController_.activeHitbox(); }

private:
    int id_;
    Vector2 position_;
    Vector2 facing_{1.0f, 0.0f};
    float bodyWidth_ = 28.0f;
    float bodyHeight_ = 28.0f;
    HealthComponent health_{60.0f};
    MonsterAttackController attackController_;
};

} // namespace nova
