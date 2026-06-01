#include "boss/BossPatternController.h"

#include "combat/CombatSystem.h"
#include "combat/DamageInfo.h"

#include <cmath>

namespace nova {

namespace {
// Normalise a direction; returns {0,0} if the input is (near) zero length.
Vector2 normalized(const Vector2& v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len <= 0.0001f) {
        return Vector2{0.0f, 0.0f};
    }
    return Vector2{v.x / len, v.y / len};
}
} // namespace

BossPatternController::BossPatternController(int ownerId)
    : ownerId_(ownerId) {}

void BossPatternController::addPattern(const BossPattern& pattern) {
    patterns_.push_back(pattern);
    cooldownRemaining_.push_back(0.0f);
}

void BossPatternController::clearPatterns() {
    patterns_.clear();
    cooldownRemaining_.clear();
    phase_ = BossPhase::Idle;
    currentIndex_ = -1;
}

int BossPatternController::findPattern(BossAttackPattern type) const {
    for (int i = 0; i < static_cast<int>(patterns_.size()); ++i) {
        if (patterns_[i].type == type) {
            return i;
        }
    }
    return -1;
}

BossAttackPattern BossPatternController::currentPattern() const {
    if (currentIndex_ < 0) {
        return BossAttackPattern::None;
    }
    return patterns_[currentIndex_].type;
}

bool BossPatternController::isPatternReady(BossAttackPattern type) const {
    const int idx = findPattern(type);
    if (idx < 0) {
        return false;
    }
    return cooldownRemaining_[idx] <= 0.0f;
}

float BossPatternController::cooldownRemaining(BossAttackPattern type) const {
    const int idx = findPattern(type);
    if (idx < 0) {
        return 0.0f;
    }
    return cooldownRemaining_[idx];
}

std::vector<BossPatternInfo> BossPatternController::patternInfos() const {
    std::vector<BossPatternInfo> infos;
    infos.reserve(patterns_.size());
    for (int i = 0; i < static_cast<int>(patterns_.size()); ++i) {
        BossPatternInfo info;
        info.type = patterns_[i].type;
        info.minRange = patterns_[i].minRange;
        info.maxRange = patterns_[i].maxRange;
        info.weight = patterns_[i].selectionWeight;
        info.ready = cooldownRemaining_[i] <= 0.0f;
        infos.push_back(info);
    }
    return infos;
}

bool BossPatternController::tryStartPattern(BossAttackPattern type,
                                            const Vector2& bossCenter,
                                            const Vector2& facing,
                                            const Vector2& targetCenter) {
    if (isBusy()) {
        return false;
    }
    const int idx = findPattern(type);
    if (idx < 0 || cooldownRemaining_[idx] > 0.0f) {
        return false;
    }

    // Capture the aim direction once at the start of the pattern. Prefer the
    // supplied facing; fall back to "toward the target" if facing is zero.
    Vector2 dir = normalized(facing);
    if (dir.x == 0.0f && dir.y == 0.0f) {
        dir = normalized(Vector2{targetCenter.x - bossCenter.x,
                                 targetCenter.y - bossCenter.y});
    }
    if (dir.x == 0.0f && dir.y == 0.0f) {
        dir = Vector2{1.0f, 0.0f};
    }
    attackFacing_ = dir;

    // Lock the AoE centre where the boss stands when the warning begins, so the
    // telegraph circle does not slide around during the windup.
    aoeCenter_ = bossCenter;

    currentIndex_ = idx;
    phase_ = BossPhase::Windup;
    phaseTimer_ = patterns_[idx].windupSeconds;
    hitboxLive_ = false;
    return true;
}

void BossPatternController::update(float deltaSeconds,
                                   CombatSystem& combat,
                                   const Vector2& bossCenter,
                                   const Vector2& facing,
                                   const std::vector<ICombatant*>& targets) {
    (void)facing; // aim is captured at pattern start (attackFacing_)

    // Cool down every pattern that is not currently executing.
    for (int i = 0; i < static_cast<int>(cooldownRemaining_.size()); ++i) {
        if (cooldownRemaining_[i] > 0.0f) {
            cooldownRemaining_[i] -= deltaSeconds;
            if (cooldownRemaining_[i] < 0.0f) {
                cooldownRemaining_[i] = 0.0f;
            }
        }
    }

    if (phase_ == BossPhase::Idle || currentIndex_ < 0) {
        return;
    }

    const BossPattern& p = patterns_[currentIndex_];

    switch (phase_) {
        case BossPhase::Windup:
            hitboxLive_ = false;
            phaseTimer_ -= deltaSeconds;
            if (phaseTimer_ <= 0.0f) {
                enterActive(p, bossCenter);
            }
            break;

        case BossPhase::Active:
            runActive(deltaSeconds, p, combat, bossCenter, targets);
            break;

        case BossPhase::Recovery:
            hitboxLive_ = false;
            phaseTimer_ -= deltaSeconds;
            if (phaseTimer_ <= 0.0f) {
                cooldownRemaining_[currentIndex_] = p.cooldownSeconds;
                phase_ = BossPhase::Idle;
                currentIndex_ = -1;
            }
            break;

        case BossPhase::Idle:
            break;
    }
}

void BossPatternController::enterActive(const BossPattern& pattern,
                                        const Vector2& bossCenter) {
    phase_ = BossPhase::Active;
    phaseTimer_ = pattern.activeSeconds;
    strikesDone_ = 1; // the first strike happens this frame
    strikeTimer_ = (pattern.type == BossAttackPattern::MeleeCombo)
                       ? pattern.comboInterval
                       : 0.0f;
    alreadyHitIds_.clear();

    if (pattern.type == BossAttackPattern::ProjectileAttack) {
        hitboxLive_ = false;
        if (onSpawnProjectile) {
            ProjectileSpawn spawn;
            spawn.origin = bossCenter;
            spawn.direction = attackFacing_;
            spawn.speed = pattern.projectileSpeed;
            spawn.damage = pattern.attack.damage;
            spawn.sourceId = ownerId_;
            onSpawnProjectile(spawn);
        }
    } else if (pattern.type == BossAttackPattern::RadialAttack) {
        // The AoE uses a circle (aoeCenter_ locked at warning start), not the
        // AABB swing hitbox. Damage is applied in runActive().
        hitboxLive_ = false;
    } else {
        currentHitbox_ = buildHitbox(pattern, bossCenter);
        hitboxLive_ = true;
    }
}

void BossPatternController::runActive(float deltaSeconds,
                                      const BossPattern& pattern,
                                      CombatSystem& combat,
                                      const Vector2& bossCenter,
                                      const std::vector<ICombatant*>& targets) {
    phaseTimer_ -= deltaSeconds;

    if (pattern.type == BossAttackPattern::ProjectileAttack) {
        // Spawn the remaining projectiles spread across the active window.
        strikeTimer_ -= deltaSeconds;
        if (strikesDone_ < pattern.projectileCount && strikeTimer_ <= 0.0f) {
            if (onSpawnProjectile) {
                ProjectileSpawn spawn;
                spawn.origin = bossCenter;
                spawn.direction = attackFacing_;
                spawn.speed = pattern.projectileSpeed;
                spawn.damage = pattern.attack.damage;
                spawn.sourceId = ownerId_;
                onSpawnProjectile(spawn);
            }
            ++strikesDone_;
            strikeTimer_ = (pattern.comboInterval > 0.0f) ? pattern.comboInterval
                                                          : 0.1f;
        }
        hitboxLive_ = false;
    } else if (pattern.type == BossAttackPattern::RadialAttack) {
        // Circular AoE: damage everyone inside the locked circle, once each.
        hitboxLive_ = false;
        applyRadialDamage(pattern, combat, targets);
    } else {
        // Melee-style: keep the hitbox glued to the (possibly dashing) boss.
        currentHitbox_ = buildHitbox(pattern, bossCenter);
        hitboxLive_ = true;

        // A combo "re-arms" between strikes so a target in the box is hit
        // multiple times (once per strike).
        if (pattern.type == BossAttackPattern::MeleeCombo &&
            pattern.comboHits > 1) {
            strikeTimer_ -= deltaSeconds;
            if (strikeTimer_ <= 0.0f && strikesDone_ < pattern.comboHits) {
                alreadyHitIds_.clear();
                ++strikesDone_;
                strikeTimer_ = pattern.comboInterval;
            }
        }

        applyMeleeDamage(pattern, combat, targets);
    }

    if (phaseTimer_ <= 0.0f) {
        phase_ = BossPhase::Recovery;
        phaseTimer_ = pattern.recoverySeconds;
        hitboxLive_ = false;
    }
}

Hitbox BossPatternController::buildHitbox(const BossPattern& pattern,
                                         const Vector2& bossCenter) const {
    // MeleeCombo and DashAttack: a box placed in front along the aim direction.
    // (RadialAttack does not use this; it uses a circle via resolveRadialAttack.)
    Hitbox box;
    const float reach = pattern.attack.range * 0.5f;
    const float cx = bossCenter.x + attackFacing_.x * reach;
    const float cy = bossCenter.y + attackFacing_.y * reach;
    box.w = pattern.attack.hitboxWidth;
    box.h = pattern.attack.hitboxHeight;
    box.x = cx - box.w * 0.5f;
    box.y = cy - box.h * 0.5f;
    return box;
}

void BossPatternController::applyMeleeDamage(
    const BossPattern& pattern,
    CombatSystem& combat,
    const std::vector<ICombatant*>& targets) {
    DamageInfo damage;
    damage.amount = pattern.attack.damage;
    damage.sourceId = ownerId_;
    damage.type = pattern.attack.damageType;

    const AttackResult result =
        combat.resolveMeleeAttack(currentHitbox_, damage, targets, &alreadyHitIds_);
    for (int hitId : result.hitIds) {
        alreadyHitIds_.push_back(hitId);
    }
}

void BossPatternController::applyRadialDamage(
    const BossPattern& pattern,
    CombatSystem& combat,
    const std::vector<ICombatant*>& targets) {
    DamageInfo damage;
    damage.amount = pattern.attack.damage;
    damage.sourceId = ownerId_;
    damage.type = pattern.attack.damageType;

    // Circular hit detection via the CombatSystem. alreadyHitIds_ persists for
    // the whole active window, so each victim is damaged once per blast.
    const AttackResult result = combat.resolveRadialAttack(
        aoeCenter_, pattern.radialRadius, damage, targets, &alreadyHitIds_);
    for (int hitId : result.hitIds) {
        alreadyHitIds_.push_back(hitId);
    }
}

AoeCircleDebug BossPatternController::aoeDebug() const {
    AoeCircleDebug info;
    if (currentIndex_ < 0 ||
        patterns_[currentIndex_].type != BossAttackPattern::RadialAttack) {
        return info; // not a radial pattern -> nothing to draw
    }
    if (phase_ == BossPhase::Windup || phase_ == BossPhase::Active) {
        info.visible = true;
        info.warning = (phase_ == BossPhase::Windup);
        info.active = (phase_ == BossPhase::Active);
        info.center = aoeCenter_;
        info.radius = patterns_[currentIndex_].radialRadius;
    }
    return info;
}

bool BossPatternController::isDashing() const {
    return phase_ == BossPhase::Active &&
           currentIndex_ >= 0 &&
           patterns_[currentIndex_].type == BossAttackPattern::DashAttack;
}

Vector2 BossPatternController::dashVelocity() const {
    if (!isDashing()) {
        return Vector2{0.0f, 0.0f};
    }
    const float speed = patterns_[currentIndex_].dashSpeed;
    return Vector2{attackFacing_.x * speed, attackFacing_.y * speed};
}

} // namespace nova
