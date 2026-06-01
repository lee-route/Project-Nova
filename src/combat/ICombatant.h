#pragma once

#include "combat/Hitbox.h"
#include "combat/IDamageable.h"

namespace nova {

// Anything the CombatSystem can hit in the world.
// It must be damageable (IDamageable) and expose a body to test against,
// plus an id so an attacker can avoid hitting itself / track who it hit.
class ICombatant : public IDamageable {
public:
    virtual int id() const = 0;
    virtual Hitbox bodyHitbox() const = 0;
};

} // namespace nova
