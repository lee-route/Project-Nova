#include "combat/CombatSystem.h"

#include <algorithm>

namespace nova::combat {

void CombatSystem::RegisterDamageable(IDamageable* damageable) {
    if (damageable == nullptr) {
        return;
    }

    const auto it = std::find(damageables_.begin(), damageables_.end(), damageable);
    if (it == damageables_.end()) {
        damageables_.push_back(damageable);
    }
}

void CombatSystem::SetDamageEventListener(IDamageEventListener* damageEventListener) {
    damageEventListener_ = damageEventListener;
}

void CombatSystem::UnregisterDamageable(IDamageable* damageable) {
    damageables_.erase(
        std::remove(damageables_.begin(), damageables_.end(), damageable),
        damageables_.end()
    );
    lastHitAttackIdByTarget_.erase(damageable);
}

bool CombatSystem::TryStartPlayerAttack(WeaponComponent& playerWeapon, const std::uint32_t nowMs) const {
    return playerWeapon.StartAttack(nowMs);
}

std::size_t CombatSystem::ProcessPlayerAttack(
    const Hitbox& playerAttackHitbox,
    WeaponComponent& playerWeapon,
    const int playerId,
    const std::uint32_t nowMs
) {
    if (!playerWeapon.IsAttackActive(nowMs)) {
        return 0U;
    }

    const std::uint32_t currentAttackId = playerWeapon.GetCurrentAttackId();
    const AttackData& attackData = playerWeapon.GetAttackData();
    const DamageInfo damage{
        playerId,
        attackData.damage,
        attackData.knockbackX,
        attackData.knockbackY,
        attackData.hitStunMs,
        true
    };

    std::size_t hitCount = 0U;
    for (IDamageable* target : damageables_) {
        if (target == nullptr || !target->IsAlive()) {
            continue;
        }

        const auto it = lastHitAttackIdByTarget_.find(target);
        if (it != lastHitAttackIdByTarget_.end() && it->second == currentAttackId) {
            continue;
        }

        if (playerAttackHitbox.Intersects(target->GetHurtbox())) {
            const Hitbox hurtbox = target->GetHurtbox();
            target->ApplyDamage(damage); // Monster damage always goes through ApplyDamage.
            lastHitAttackIdByTarget_[target] = currentAttackId;
            if (damageEventListener_ != nullptr) {
                damageEventListener_->OnDamageApplied(DamageEvent{
                    target,
                    damage.amount,
                    hurtbox.x + hurtbox.w * 0.5F,
                    hurtbox.y
                });
            }
            ++hitCount;
        }
    }

    return hitCount;
}

} // namespace nova::combat
