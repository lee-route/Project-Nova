#pragma once

#include <cstdint>

namespace nova::combat {

struct DamageInfo {
    int sourceId = -1;
    int amount = 0;
    float knockbackX = 0.0F;
    float knockbackY = 0.0F;
    std::uint32_t hitStunMs = 0U;
    bool canKill = true;
};

} // namespace nova::combat
