#include "combat/CombatDebugRenderer.h"

namespace nova::combat {

void CombatDebugRenderer::SetEnabled(const bool enabled) {
    enabled_ = enabled;
}

bool CombatDebugRenderer::IsEnabled() const {
    return enabled_;
}

void CombatDebugRenderer::RenderPlayerAttackHitbox(
    SDL_Renderer* renderer,
    const PlayerCombatController& playerCombatController
) const {
    if (!enabled_ || renderer == nullptr || !playerCombatController.HasDebugAttackHitbox()) {
        return;
    }

    DrawHitboxOutline(renderer, playerCombatController.GetDebugAttackHitbox(), 255U, 64U, 64U, 255U);
}

void CombatDebugRenderer::RenderMonsterHitboxes(
    SDL_Renderer* renderer,
    const std::vector<IDamageable*>& monsters
) const {
    if (!enabled_ || renderer == nullptr) {
        return;
    }

    for (IDamageable* monster : monsters) {
        if (monster == nullptr) {
            continue;
        }
        DrawHitboxOutline(renderer, monster->GetHurtbox(), 64U, 255U, 64U, 255U);
    }
}

void CombatDebugRenderer::DrawHitboxOutline(
    SDL_Renderer* renderer,
    const Hitbox& hitbox,
    const Uint8 r,
    const Uint8 g,
    const Uint8 b,
    const Uint8 a
) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    const SDL_Rect rect{
        static_cast<int>(hitbox.x),
        static_cast<int>(hitbox.y),
        static_cast<int>(hitbox.w),
        static_cast<int>(hitbox.h)
    };
    SDL_RenderDrawRect(renderer, &rect);
}

} // namespace nova::combat
