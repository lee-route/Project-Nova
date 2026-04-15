#pragma once

#include <vector>

#include <SDL.h>

#include "combat/IDamageable.h"
#include "combat/PlayerCombatController.h"

namespace nova::combat {

class CombatDebugRenderer {
public:
    void SetEnabled(bool enabled);
    [[nodiscard]] bool IsEnabled() const;

    void RenderPlayerAttackHitbox(
        SDL_Renderer* renderer,
        const PlayerCombatController& playerCombatController
    ) const;

    void RenderMonsterHitboxes(
        SDL_Renderer* renderer,
        const std::vector<IDamageable*>& monsters
    ) const;

private:
    static void DrawHitboxOutline(
        SDL_Renderer* renderer,
        const Hitbox& hitbox,
        Uint8 r,
        Uint8 g,
        Uint8 b,
        Uint8 a
    );

    bool enabled_ = false;
};

} // namespace nova::combat
