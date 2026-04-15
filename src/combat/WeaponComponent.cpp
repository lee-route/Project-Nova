#include "combat/WeaponComponent.h"

namespace nova::combat {

WeaponComponent::WeaponComponent(AttackData attackData)
    : attackData_(attackData) {}

const AttackData& WeaponComponent::GetAttackData() const {
    return attackData_;
}

void WeaponComponent::SetAttackData(const AttackData& attackData) {
    attackData_ = attackData;
}

bool WeaponComponent::CanStartAttack(const std::uint32_t nowMs) const {
    if (!hasAttackedOnce_) {
        return true;
    }
    return (nowMs - lastAttackAtMs_) >= attackData_.cooldownMs;
}

bool WeaponComponent::StartAttack(const std::uint32_t nowMs) {
    if (!CanStartAttack(nowMs)) {
        return false;
    }
    attacking_ = true;
    attackStartedAtMs_ = nowMs;
    lastAttackAtMs_ = nowMs;
    ++currentAttackId_;
    hasAttackedOnce_ = true;
    return true;
}

bool WeaponComponent::IsAttackActive(const std::uint32_t nowMs) const {
    if (!attacking_) {
        return false;
    }

    const std::uint32_t elapsed = nowMs - attackStartedAtMs_;
    const bool active = elapsed >= attackData_.activeStartMs && elapsed <= attackData_.activeEndMs;
    if (!active && elapsed > attackData_.activeEndMs) {
        // Attack animation/state may continue elsewhere; combat hit window is closed.
        // attacking_ is intentionally not reset here because this method is const.
    }
    return active;
}

std::uint32_t WeaponComponent::GetCurrentAttackId() const {
    return currentAttackId_;
}

} // namespace nova::combat
