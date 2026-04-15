#include "entities/Monster.h"

namespace nova::entities {

namespace {
constexpr std::uint32_t kDefaultHitStateMs = 120U;
}

Monster::Monster(
    const int id,
    const float x,
    const float y,
    const float width,
    const float height,
    const int maxHp
)
    : id_(id),
      x_(x),
      y_(y),
      width_(width),
      height_(height),
      health_(maxHp) {}

void Monster::SetPosition(const float x, const float y) {
    x_ = x;
    y_ = y;
}

int Monster::GetId() const {
    return id_;
}

MonsterLifeState Monster::GetLifeState() const {
    return lifeState_;
}

int Monster::GetCurrentHp() const {
    return health_.GetCurrentHp();
}

int Monster::GetMaxHp() const {
    return health_.GetMaxHp();
}

bool Monster::IsActive() const {
    return active_;
}

bool Monster::IsAiEnabled() const {
    return aiEnabled_;
}

bool Monster::IsAttackEnabled() const {
    return attackEnabled_;
}

bool Monster::ShouldCleanup() const {
    return cleanupRequested_;
}

void Monster::MarkForCleanup() {
    cleanupRequested_ = true;
}

bool Monster::IsHitReacting() const {
    return lifeState_ == MonsterLifeState::Hit && hitStateRemainingMs_ > 0U;
}

bool Monster::ConsumeHitInterruptFlag() {
    const bool wasPending = pendingHitInterrupt_;
    pendingHitInterrupt_ = false;
    return wasPending;
}

void Monster::UpdateHitState(const std::uint32_t deltaMs) {
    if (lifeState_ != MonsterLifeState::Hit) {
        return;
    }

    if (deltaMs >= hitStateRemainingMs_) {
        hitStateRemainingMs_ = 0U;
        lifeState_ = MonsterLifeState::Alive;
        return;
    }
    hitStateRemainingMs_ -= deltaMs;
}

void Monster::ApplyDamage(const nova::combat::DamageInfo& damage) {
    if (lifeState_ == MonsterLifeState::Dead || !active_) {
        return;
    }

    const bool applied = health_.ApplyDamage(damage);
    if (!applied) {
        return;
    }

    if (health_.IsDead()) {
        EnterDeadState();
        return;
    }

    // Non-lethal damage enters Hit state and applies a small knockback.
    lifeState_ = MonsterLifeState::Hit;
    hitStateRemainingMs_ = damage.hitStunMs > 0U ? damage.hitStunMs : kDefaultHitStateMs;
    pendingHitInterrupt_ = true;
    x_ += damage.knockbackX;
    y_ += damage.knockbackY;
}

bool Monster::IsAlive() const {
    return lifeState_ != MonsterLifeState::Dead;
}

nova::combat::Hitbox Monster::GetHurtbox() const {
    return nova::combat::Hitbox{x_, y_, width_, height_};
}

void Monster::EnterDeadState() {
    lifeState_ = MonsterLifeState::Dead;
    pendingHitInterrupt_ = false;
    hitStateRemainingMs_ = 0U;
    aiEnabled_ = false;
    attackEnabled_ = false;
    active_ = false;
    cleanupRequested_ = true;
}

} // namespace nova::entities
