#pragma once

#include <cstdint>

namespace nova::ui {

class FloatingDamageText {
public:
    FloatingDamageText(int amount, float worldX, float worldY);

    void Update(std::uint32_t deltaMs);

    [[nodiscard]] int GetAmount() const;
    [[nodiscard]] float GetX() const;
    [[nodiscard]] float GetY() const;
    [[nodiscard]] std::uint8_t GetAlpha() const;
    [[nodiscard]] bool IsAlive() const;

private:
    int amount_ = 0;
    float x_ = 0.0F;
    float y_ = 0.0F;
    std::uint32_t elapsedMs_ = 0U;
    std::uint32_t lifetimeMs_ = 700U;
};

} // namespace nova::ui
