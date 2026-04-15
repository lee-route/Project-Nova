#include "ui/DamageTextSystem.h"

#include <algorithm>
#include <array>
#include <string>

namespace nova::ui {

void DamageTextSystem::SetEnabled(const bool enabled) {
    enabled_ = enabled;
}

bool DamageTextSystem::IsEnabled() const {
    return enabled_;
}

void DamageTextSystem::OnDamageApplied(const nova::combat::DamageEvent& damageEvent) {
    if (!enabled_ || damageEvent.amount <= 0) {
        return;
    }

    floatingTexts_.emplace_back(damageEvent.amount, damageEvent.worldX, damageEvent.worldY);
}

void DamageTextSystem::Update(const std::uint32_t deltaMs) {
    if (!enabled_) {
        return;
    }

    for (FloatingDamageText& text : floatingTexts_) {
        text.Update(deltaMs);
    }

    floatingTexts_.erase(
        std::remove_if(
            floatingTexts_.begin(),
            floatingTexts_.end(),
            [](const FloatingDamageText& text) { return !text.IsAlive(); }
        ),
        floatingTexts_.end()
    );
}

void DamageTextSystem::Render(SDL_Renderer* renderer) const {
    if (!enabled_ || renderer == nullptr) {
        return;
    }

    for (const FloatingDamageText& text : floatingTexts_) {
        DrawAmount(
            renderer,
            text.GetAmount(),
            static_cast<int>(text.GetX()),
            static_cast<int>(text.GetY()),
            2,
            text.GetAlpha()
        );
    }
}

void DamageTextSystem::Clear() {
    floatingTexts_.clear();
}

void DamageTextSystem::DrawDigit(
    SDL_Renderer* renderer,
    const int digit,
    const int x,
    const int y,
    const int scale,
    const std::uint8_t alpha
) {
    if (renderer == nullptr || digit < 0 || digit > 9) {
        return;
    }

    static constexpr std::array<std::array<bool, 7>, 10> kSevenSeg = {{
        {{true, true, true, false, true, true, true}},   // 0
        {{false, false, true, false, false, true, false}}, // 1
        {{true, false, true, true, true, false, true}},  // 2
        {{true, false, true, true, false, true, true}},  // 3
        {{false, true, true, true, false, true, false}}, // 4
        {{true, true, false, true, false, true, true}},  // 5
        {{true, true, false, true, true, true, true}},   // 6
        {{true, false, true, false, false, true, false}}, // 7
        {{true, true, true, true, true, true, true}},    // 8
        {{true, true, true, true, false, true, true}}    // 9
    }};

    const int segLen = 4 * scale;
    const int segThick = scale;
    const int segGap = scale;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255U, 220U, 64U, alpha);

    auto drawSeg = [&](const SDL_Rect rect) {
        SDL_RenderFillRect(renderer, &rect);
    };

    const SDL_Rect a{x + segGap, y, segLen, segThick};
    const SDL_Rect b{x, y + segGap, segThick, segLen};
    const SDL_Rect c{x + segGap + segLen, y + segGap, segThick, segLen};
    const SDL_Rect d{x + segGap, y + segGap + segLen, segLen, segThick};
    const SDL_Rect e{x, y + 2 * segGap + segLen, segThick, segLen};
    const SDL_Rect f{x + segGap + segLen, y + 2 * segGap + segLen, segThick, segLen};
    const SDL_Rect g{x + segGap, y + 2 * segGap + 2 * segLen, segLen, segThick};

    const auto& s = kSevenSeg[digit];
    if (s[0]) drawSeg(a);
    if (s[1]) drawSeg(b);
    if (s[2]) drawSeg(c);
    if (s[3]) drawSeg(d);
    if (s[4]) drawSeg(e);
    if (s[5]) drawSeg(f);
    if (s[6]) drawSeg(g);
}

void DamageTextSystem::DrawAmount(
    SDL_Renderer* renderer,
    const int amount,
    const int x,
    const int y,
    const int scale,
    const std::uint8_t alpha
) {
    const std::string value = std::to_string(amount);
    const int digitW = 6 * scale;
    const int spacing = 2 * scale;
    const int totalW = static_cast<int>(value.size()) * digitW + (static_cast<int>(value.size()) - 1) * spacing;
    int cursorX = x - totalW / 2;

    for (const char c : value) {
        const int digit = c - '0';
        DrawDigit(renderer, digit, cursorX, y, scale, alpha);
        cursorX += digitW + spacing;
    }
}

} // namespace nova::ui
