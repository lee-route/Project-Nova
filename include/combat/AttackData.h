#pragma once

#include <cstdint>

namespace nova::combat {

struct AttackData {
    int damage = 10;
    std::uint32_t cooldownMs = 300U;
    std::uint32_t activeStartMs = 0U;
    std::uint32_t activeEndMs = 150U;
    float range = 42.0F;
    float halfWidth = 16.0F;
    float halfHeight = 16.0F;
    float knockbackX = 0.0F;
    float knockbackY = 0.0F;
    std::uint32_t hitStunMs = 100U;
};

} // namespace nova::combat
