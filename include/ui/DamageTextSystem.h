#pragma once

#include <cstdint>
#include <vector>

#include <SDL.h>

#include "combat/IDamageEventListener.h"
#include "ui/FloatingDamageText.h"

namespace nova::ui {

class DamageTextSystem final : public nova::combat::IDamageEventListener {
public:
    void SetEnabled(bool enabled);
    [[nodiscard]] bool IsEnabled() const;

    void OnDamageApplied(const nova::combat::DamageEvent& damageEvent) override;
    void Update(std::uint32_t deltaMs);
    void Render(SDL_Renderer* renderer) const;
    void Clear();

private:
    static void DrawDigit(SDL_Renderer* renderer, int digit, int x, int y, int scale, std::uint8_t alpha);
    static void DrawAmount(SDL_Renderer* renderer, int amount, int x, int y, int scale, std::uint8_t alpha);

    bool enabled_ = true;
    std::vector<FloatingDamageText> floatingTexts_{};
};

} // namespace nova::ui
