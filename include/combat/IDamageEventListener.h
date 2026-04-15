#pragma once

#include "combat/DamageEvent.h"

namespace nova::combat {

class IDamageEventListener {
public:
    virtual ~IDamageEventListener() = default;
    virtual void OnDamageApplied(const DamageEvent& damageEvent) = 0;
};

} // namespace nova::combat
