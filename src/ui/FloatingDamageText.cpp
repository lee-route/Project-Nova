#include "ui/FloatingDamageText.h"

#include <algorithm>

namespace nova::ui {

FloatingDamageText::FloatingDamageText(const int amount, const float worldX, const float worldY)
    : amount_(amount),
      x_(worldX),
      y_(worldY) {}

void FloatingDamageText::Update(const std::uint32_t deltaMs) {
    elapsedMs_ = std::min(lifetimeMs_, elapsedMs_ + deltaMs);

    // Move up while fading out.
    y_ -= static_cast<float>(deltaMs) * 0.05F;
}

int FloatingDamageText::GetAmount() const {
    return amount_;
}

float FloatingDamageText::GetX() const {
    return x_;
}

float FloatingDamageText::GetY() const {
    return y_;
}

std::uint8_t FloatingDamageText::GetAlpha() const {
    if (elapsedMs_ >= lifetimeMs_) {
        return 0U;
    }

    const float t = static_cast<float>(elapsedMs_) / static_cast<float>(lifetimeMs_);
    const float alpha = (1.0F - t) * 255.0F;
    return static_cast<std::uint8_t>(std::max(0.0F, alpha));
}

bool FloatingDamageText::IsAlive() const {
    return elapsedMs_ < lifetimeMs_;
}

} // namespace nova::ui
