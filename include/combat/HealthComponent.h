#pragma once

#include "combat/DamageInfo.h"

namespace nova::combat {

class HealthComponent {
public:
    explicit HealthComponent(int maxHp = 100);

    void Reset(int maxHp);
    bool ApplyDamage(const DamageInfo& damage);
    int Heal(int amount);

    [[nodiscard]] int GetCurrentHp() const;
    [[nodiscard]] int GetMaxHp() const;
    [[nodiscard]] bool IsDead() const;

private:
    int maxHp_ = 100;
    int currentHp_ = 100;
};

} // namespace nova::combat
