#pragma once

#include "combat/DamageInfo.h"

namespace nova {

// Holds HP for any entity that can take damage.
// Only the CombatSystem (or healing systems) should mutate it,
// which keeps damage rules in one place.
class HealthComponent {
public:
    HealthComponent() = default;
    explicit HealthComponent(float maxHp)
        : maxHealth_(maxHp), currentHealth_(maxHp) {}

    void applyDamage(const DamageInfo& info) {
        if (!isAlive()) {
            return;
        }
        currentHealth_ -= info.amount;
        if (currentHealth_ < 0.0f) {
            currentHealth_ = 0.0f;
        }
    }

    void heal(float amount) {
        if (!isAlive()) {
            return;
        }
        currentHealth_ += amount;
        if (currentHealth_ > maxHealth_) {
            currentHealth_ = maxHealth_;
        }
    }

    bool isAlive() const { return currentHealth_ > 0.0f; }
    float current() const { return currentHealth_; }
    float max() const { return maxHealth_; }

private:
    float maxHealth_ = 100.0f;
    float currentHealth_ = 100.0f;
};

} // namespace nova
