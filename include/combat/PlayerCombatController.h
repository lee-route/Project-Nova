#pragma once

#include <cstdint>

#include <SDL.h>

#include "combat/CombatSystem.h"

namespace nova::combat {

class PlayerCombatController {
public:
    PlayerCombatController(CombatSystem& combatSystem, WeaponComponent& weapon, int playerId);

    void Update(
        const Uint8* keyboardState,
        float playerX,
        float playerY,
        float facingX,
        float facingY,
        std::uint32_t nowMs
    );
    [[nodiscard]] bool HasDebugAttackHitbox() const;
    [[nodiscard]] Hitbox GetDebugAttackHitbox() const;

private:
    [[nodiscard]] Hitbox BuildMeleeHitbox(
        float playerX,
        float playerY,
        float facingX,
        float facingY
    ) const;

    CombatSystem& combatSystem_;
    WeaponComponent& weapon_;
    int playerId_ = -1;
    bool wasSpaceDown_ = false;
    bool hasDebugAttackHitbox_ = false;
    Hitbox debugAttackHitbox_{};
};

} // namespace nova::combat
