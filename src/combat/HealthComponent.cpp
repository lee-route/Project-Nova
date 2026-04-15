#include "combat/HealthComponent.h"

#include <algorithm>

namespace nova::combat {

HealthComponent::HealthComponent(const int maxHp) {
    Reset(maxHp);
}

void HealthComponent::Reset(const int maxHp) {
    maxHp_ = std::max(1, maxHp);
    currentHp_ = maxHp_;
}

bool HealthComponent::ApplyDamage(const DamageInfo& damage) {
    if (IsDead() || damage.amount <= 0) {
        return false;
    }

    currentHp_ = std::max(0, currentHp_ - damage.amount);
    if (!damage.canKill && currentHp_ == 0) {
        currentHp_ = 1;
    }
    return true;
}

int HealthComponent::Heal(const int amount) {
    if (amount <= 0 || IsDead()) {
        return currentHp_;
    }
    currentHp_ = std::min(maxHp_, currentHp_ + amount);
    return currentHp_;
}

int HealthComponent::GetCurrentHp() const {
    return currentHp_;
}

int HealthComponent::GetMaxHp() const {
    return maxHp_;
}

bool HealthComponent::IsDead() const {
    return currentHp_ <= 0;
}

} // namespace nova::combat
