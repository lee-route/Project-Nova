#pragma once

#include "combat/Hitbox.h"
#include "combat/DamageInfo.h"
#include "core/Vector2.h"

#include <vector>

namespace nova {

class ICombatant;

// Summary of what one attack call did, returned to the caller (e.g. for VFX
// and, importantly, so the attacker can remember who it already hit).
struct AttackResult {
    int hitCount = 0;
    float totalDamage = 0.0f;
    std::vector<int> hitIds;   // ids of combatants damaged by THIS call
    bool connected() const { return hitCount > 0; }
};

// THE single choke-point for every attack in the game.
// Monsters, the player, traps... they all call into here instead of
// editing each other's HealthComponents directly. That keeps damage
// rules consistent and easy to debug.
class CombatSystem {
public:
    // Apply a melee swing: every target whose body overlaps `attackHitbox`
    // (and that is not the attacker) takes `damage`.
    //
    // `ignoreIds` lets a multi-frame attack skip targets it has already hit,
    // which is how "damage the player only once per swing" is enforced.
    // Pass nullptr to damage everyone overlapping.
    AttackResult resolveMeleeAttack(const Hitbox& attackHitbox,
                                    const DamageInfo& damage,
                                    const std::vector<ICombatant*>& targets,
                                    const std::vector<int>* ignoreIds = nullptr);

    // Apply a circular area attack (AoE): every target whose body overlaps the
    // circle (`center`, `radius`) and that is not the attacker takes `damage`.
    // Same `ignoreIds` rule as melee, so an AoE hits each target only once.
    AttackResult resolveRadialAttack(const Vector2& center,
                                     float radius,
                                     const DamageInfo& damage,
                                     const std::vector<ICombatant*>& targets,
                                     const std::vector<int>* ignoreIds = nullptr);
};

} // namespace nova
