#include "monster/MonsterAttackController.h"

#include "combat/CombatSystem.h"
#include "combat/DamageInfo.h"

#include <cmath>

namespace nova {

MonsterAttackController::MonsterAttackController(const AttackData& data, int ownerId)
    : attackData_(data), ownerId_(ownerId) {}

void MonsterAttackController::update(float deltaSeconds,
                                     CombatSystem& combat,
                                     const Vector2& monsterCenter,
                                     const Vector2& facing,
                                     const std::vector<ICombatant*>& targets) {
    switch (phase_) {
        case AttackPhase::Ready:
            // Nothing to do until an attack is requested.
            break;

        case AttackPhase::Active: {
            // Keep the hitbox glued in front of the monster as it moves/turns.
            currentHitbox_ = buildMeleeHitbox(monsterCenter, facing);

            // Deal damage to anyone overlapping who hasn't been hit yet.
            applyActiveWindowDamage(combat, targets);

            activeRemaining_ -= deltaSeconds;
            if (activeRemaining_ <= 0.0f) {
                activeRemaining_ = 0.0f;
                phase_ = AttackPhase::Cooldown;
                cooldownRemaining_ = attackData_.cooldownSeconds;
            }
            break;
        }

        case AttackPhase::Cooldown:
            cooldownRemaining_ -= deltaSeconds;
            if (cooldownRemaining_ <= 0.0f) {
                cooldownRemaining_ = 0.0f;
                phase_ = AttackPhase::Ready;
            }
            break;
    }
}

bool MonsterAttackController::tryStartAttack(const Vector2& monsterCenter,
                                             const Vector2& facing,
                                             const Vector2& targetCenter) {
    if (phase_ != AttackPhase::Ready) {
        return false; // mid-swing or recovering
    }
    if (!isTargetInRange(monsterCenter, targetCenter)) {
        return false;
    }

    // Open the active window. Damage itself is applied in update().
    phase_ = AttackPhase::Active;
    activeRemaining_ = attackData_.activeSeconds;
    alreadyHitIds_.clear();
    currentHitbox_ = buildMeleeHitbox(monsterCenter, facing);
    return true;
}

bool MonsterAttackController::tryStartAttack(const AttackData& data,
                                             const Vector2& monsterCenter,
                                             const Vector2& facing,
                                             const Vector2& targetCenter) {
    // Boss path: pick the attack for this swing, then start normally.
    attackData_ = data;
    return tryStartAttack(monsterCenter, facing, targetCenter);
}

bool MonsterAttackController::isTargetInRange(const Vector2& monsterCenter,
                                             const Vector2& targetCenter) const {
    const float dx = targetCenter.x - monsterCenter.x;
    const float dy = targetCenter.y - monsterCenter.y;
    const float distSq = dx * dx + dy * dy;
    // Compare squared distances to avoid a sqrt.
    return distSq <= attackData_.range * attackData_.range;
}

Hitbox MonsterAttackController::buildMeleeHitbox(const Vector2& monsterCenter,
                                                const Vector2& facing) const {
    // Normalise the facing direction so the swing lands in front of the monster.
    float fx = facing.x;
    float fy = facing.y;
    const float len = std::sqrt(fx * fx + fy * fy);
    if (len > 0.0001f) {
        fx /= len;
        fy /= len;
    } else {
        fx = 0.0f;
        fy = 0.0f;
    }

    // Push the swing volume halfway toward the edge of the attack range.
    const float reach = attackData_.range * 0.5f;
    const float swingCenterX = monsterCenter.x + fx * reach;
    const float swingCenterY = monsterCenter.y + fy * reach;

    Hitbox box;
    box.w = attackData_.hitboxWidth;
    box.h = attackData_.hitboxHeight;
    box.x = swingCenterX - box.w * 0.5f;
    box.y = swingCenterY - box.h * 0.5f;
    return box;
}

void MonsterAttackController::applyActiveWindowDamage(
    CombatSystem& combat,
    const std::vector<ICombatant*>& targets) {
    DamageInfo damage;
    damage.amount = attackData_.damage;
    damage.sourceId = ownerId_;
    damage.type = attackData_.damageType;

    // All damage flows through the CombatSystem. We pass the already-hit list so
    // the same target cannot be damaged twice during one active window.
    const AttackResult result =
        combat.resolveMeleeAttack(currentHitbox_, damage, targets, &alreadyHitIds_);

    for (int hitId : result.hitIds) {
        alreadyHitIds_.push_back(hitId);
    }
}

} // namespace nova
