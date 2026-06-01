#pragma once

#include "combat/DamageInfo.h"

namespace nova {

// Pure data description of ONE attack. No logic here on purpose:
// designers/teammates can tweak these numbers without touching code.
// One monster type = one AttackData (or a list of them later).
struct AttackData {
    float damage = 10.0f;          // how much damage the hit deals
    float range = 32.0f;           // distance (px) at which the monster may start the attack
    float activeSeconds = 0.2f;    // how long the swing hitbox stays "live"
    float cooldownSeconds = 1.0f;  // recovery time AFTER the active window ends
    float hitboxWidth = 24.0f;     // size of the melee swing volume
    float hitboxHeight = 24.0f;
    DamageType damageType = DamageType::Melee;
};

} // namespace nova
