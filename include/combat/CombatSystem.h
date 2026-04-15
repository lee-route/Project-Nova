#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "combat/IDamageable.h"
#include "combat/IDamageEventListener.h"
#include "combat/WeaponComponent.h"

namespace nova::combat {

class CombatSystem {
public:
    void RegisterDamageable(IDamageable* damageable);
    void UnregisterDamageable(IDamageable* damageable);
    void SetDamageEventListener(IDamageEventListener* damageEventListener);

    bool TryStartPlayerAttack(WeaponComponent& playerWeapon, std::uint32_t nowMs) const;

    std::size_t ProcessPlayerAttack(
        const Hitbox& playerAttackHitbox,
        WeaponComponent& playerWeapon,
        int playerId,
        std::uint32_t nowMs
    );

private:
    std::vector<IDamageable*> damageables_{};
    std::unordered_map<IDamageable*, std::uint32_t> lastHitAttackIdByTarget_{};
    IDamageEventListener* damageEventListener_ = nullptr;
};

} // namespace nova::combat
