#pragma once

namespace nova {

enum class DamageType {
    Melee,
    Ranged,
    Magic
};

// A single packet of damage handed to a HealthComponent.
// "sourceId" lets the CombatSystem avoid an attacker hitting itself
// and lets other systems (score, aggro) know who caused the damage.
struct DamageInfo {
    float amount = 0.0f;
    int sourceId = -1;
    DamageType type = DamageType::Melee;
};

} // namespace nova
