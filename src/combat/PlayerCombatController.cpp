#include "combat/PlayerCombatController.h"

#include <cmath>

namespace nova::combat {

namespace {
constexpr float kDefaultFacingX = 1.0F;
constexpr float kDefaultFacingY = 0.0F;
}

PlayerCombatController::PlayerCombatController(
    CombatSystem& combatSystem,
    WeaponComponent& weapon,
    const int playerId
)
    : combatSystem_(combatSystem),
      weapon_(weapon),
      playerId_(playerId) {}

void PlayerCombatController::Update(
    const Uint8* keyboardState,
    const float playerX,
    const float playerY,
    float facingX,
    float facingY,
    const std::uint32_t nowMs
) {
    if (keyboardState == nullptr) {
        return;
    }

    const bool isSpaceDown = keyboardState[SDL_SCANCODE_SPACE] != 0;
    const bool justPressedSpace = isSpaceDown && !wasSpaceDown_;
    wasSpaceDown_ = isSpaceDown;

    if (justPressedSpace) {
        combatSystem_.TryStartPlayerAttack(weapon_, nowMs);
    }

    debugAttackHitbox_ = BuildMeleeHitbox(playerX, playerY, facingX, facingY);
    hasDebugAttackHitbox_ = weapon_.IsAttackActive(nowMs);
    combatSystem_.ProcessPlayerAttack(debugAttackHitbox_, weapon_, playerId_, nowMs);
}

bool PlayerCombatController::HasDebugAttackHitbox() const {
    return hasDebugAttackHitbox_;
}

Hitbox PlayerCombatController::GetDebugAttackHitbox() const {
    return debugAttackHitbox_;
}

Hitbox PlayerCombatController::BuildMeleeHitbox(
    const float playerX,
    const float playerY,
    float facingX,
    float facingY
) const {
    const float length = std::sqrt(facingX * facingX + facingY * facingY);
    if (length <= 0.0001F) {
        facingX = kDefaultFacingX;
        facingY = kDefaultFacingY;
    } else {
        facingX /= length;
        facingY /= length;
    }

    const AttackData& attackData = weapon_.GetAttackData();
    const float centerX = playerX + facingX * attackData.range;
    const float centerY = playerY + facingY * attackData.range;

    return Hitbox{
        centerX - attackData.halfWidth,
        centerY - attackData.halfHeight,
        attackData.halfWidth * 2.0F,
        attackData.halfHeight * 2.0F
    };
}

} // namespace nova::combat
