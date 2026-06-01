#include "combat/CombatSystem.h"

#include "combat/ICombatant.h"

#include <algorithm>

namespace nova {

namespace {
bool isIgnored(int id, const std::vector<int>* ignoreIds) {
    if (ignoreIds == nullptr) {
        return false;
    }
    return std::find(ignoreIds->begin(), ignoreIds->end(), id) != ignoreIds->end();
}
} // namespace

AttackResult CombatSystem::resolveMeleeAttack(const Hitbox& attackHitbox,
                                              const DamageInfo& damage,
                                              const std::vector<ICombatant*>& targets,
                                              const std::vector<int>* ignoreIds) {
    AttackResult result;

    for (ICombatant* target : targets) {
        if (target == nullptr) {
            continue;
        }
        // Never let an attacker damage itself.
        if (target->id() == damage.sourceId) {
            continue;
        }
        // Already hit by this same swing -> skip (once-per-attack rule).
        if (isIgnored(target->id(), ignoreIds)) {
            continue;
        }
        // Dead things take no further damage.
        if (!target->isAlive()) {
            continue;
        }
        // Geometry test: did the swing overlap this target's body?
        if (attackHitbox.intersects(target->bodyHitbox())) {
            // Hand the packet to the target; IT decides how to react
            // (i-frames, armor, death). Combat does not touch HP directly.
            target->applyDamage(damage);
            result.hitCount += 1;
            result.totalDamage += damage.amount;
            result.hitIds.push_back(target->id());
        }
    }

    return result;
}

AttackResult CombatSystem::resolveRadialAttack(const Vector2& center,
                                               float radius,
                                               const DamageInfo& damage,
                                               const std::vector<ICombatant*>& targets,
                                               const std::vector<int>* ignoreIds) {
    AttackResult result;

    for (ICombatant* target : targets) {
        if (target == nullptr) {
            continue;
        }
        if (target->id() == damage.sourceId) {
            continue;
        }
        if (isIgnored(target->id(), ignoreIds)) {
            continue;
        }
        if (!target->isAlive()) {
            continue;
        }
        // Circular test: is any part of the body within `radius` of `center`?
        if (target->bodyHitbox().intersectsCircle(center.x, center.y, radius)) {
            target->applyDamage(damage);
            result.hitCount += 1;
            result.totalDamage += damage.amount;
            result.hitIds.push_back(target->id());
        }
    }

    return result;
}

} // namespace nova
