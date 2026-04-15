#pragma once

#include <cstdint>

#include "combat/HealthComponent.h"
#include "combat/IDamageable.h"

namespace nova::entities {

enum class MonsterLifeState {
    Alive,
    Hit,
    Dead
};

class Monster final : public nova::combat::IDamageable {
public:
    Monster(int id, float x, float y, float width, float height, int maxHp);

    void SetPosition(float x, float y);
    [[nodiscard]] int GetId() const;
    [[nodiscard]] MonsterLifeState GetLifeState() const;
    [[nodiscard]] int GetCurrentHp() const;
    [[nodiscard]] int GetMaxHp() const;
    [[nodiscard]] bool IsHitReacting() const;
    [[nodiscard]] bool ConsumeHitInterruptFlag();
    void UpdateHitState(std::uint32_t deltaMs);
    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] bool IsAiEnabled() const;
    [[nodiscard]] bool IsAttackEnabled() const;
    [[nodiscard]] bool ShouldCleanup() const;
    void MarkForCleanup();

    // IDamageable
    void ApplyDamage(const nova::combat::DamageInfo& damage) override;
    [[nodiscard]] bool IsAlive() const override;
    [[nodiscard]] nova::combat::Hitbox GetHurtbox() const override;

private:
    void EnterDeadState();

    int id_ = -1;
    float x_ = 0.0F;
    float y_ = 0.0F;
    float width_ = 0.0F;
    float height_ = 0.0F;
    MonsterLifeState lifeState_ = MonsterLifeState::Alive;
    std::uint32_t hitStateRemainingMs_ = 0U;
    bool pendingHitInterrupt_ = false;
    bool active_ = true;
    bool aiEnabled_ = true;
    bool attackEnabled_ = true;
    bool cleanupRequested_ = false;
    nova::combat::HealthComponent health_;
};

} // namespace nova::entities
