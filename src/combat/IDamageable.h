#pragma once

#include "combat/DamageInfo.h"

namespace nova {

// Contract for anything that can RECEIVE damage (player, monsters, props).
//
// The entity itself decides what a DamageInfo does to it (invulnerability
// frames, armor, death, etc.), so those rules live with the entity instead of
// being scattered through the combat code. The CombatSystem only calls
// applyDamage(); it never edits HP directly.
class IDamageable {
public:
    virtual ~IDamageable() = default;

    // Apply one packet of damage. Implementations may ignore it (i-frames,
    // already dead, immune, ...). No return value: combat does not care.
    virtual void applyDamage(const DamageInfo& info) = 0;

    // Used by the CombatSystem to skip targets that are already dead.
    virtual bool isAlive() const = 0;
};

} // namespace nova
