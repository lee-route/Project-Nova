#pragma once

#include "combat/DamageInfo.h"
#include "combat/Hitbox.h"

namespace nova::combat {

class IDamageable {
public:
    virtual ~IDamageable() = default;

    virtual void ApplyDamage(const DamageInfo& damage) = 0;
    [[nodiscard]] virtual bool IsAlive() const = 0;
    [[nodiscard]] virtual Hitbox GetHurtbox() const = 0;
};

} // namespace nova::combat
