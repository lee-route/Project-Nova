#pragma once

#include "combat/IDamageable.h"

namespace nova::combat {

struct DamageEvent {
    const IDamageable* target = nullptr;
    int amount = 0;
    float worldX = 0.0F;
    float worldY = 0.0F;
};

} // namespace nova::combat
