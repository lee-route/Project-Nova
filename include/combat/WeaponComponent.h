#pragma once

#include <cstdint>

#include "combat/AttackData.h"

namespace nova::combat {

class WeaponComponent {
public:
    explicit WeaponComponent(AttackData attackData = {});

    [[nodiscard]] const AttackData& GetAttackData() const;
    void SetAttackData(const AttackData& attackData);

    [[nodiscard]] bool CanStartAttack(std::uint32_t nowMs) const;
    bool StartAttack(std::uint32_t nowMs);
    [[nodiscard]] bool IsAttackActive(std::uint32_t nowMs) const;
    [[nodiscard]] std::uint32_t GetCurrentAttackId() const;

private:
    AttackData attackData_{};
    std::uint32_t attackStartedAtMs_ = 0U;
    std::uint32_t lastAttackAtMs_ = 0U;
    std::uint32_t currentAttackId_ = 0U;
    bool hasAttackedOnce_ = false;
    bool attacking_ = false;
};

} // namespace nova::combat
