#include "ui/MonsterHpBarSystem.h"

#include <algorithm>

#include "entities/Monster.h"

namespace nova::ui {

void MonsterHpBarSystem::SetEnabled(const bool enabled) {
    enabled_ = enabled;
}

bool MonsterHpBarSystem::IsEnabled() const {
    return enabled_;
}

void MonsterHpBarSystem::BuildFrameData(const std::vector<const nova::entities::Monster*>& monsters) {
    entries_.clear();
    if (!enabled_) {
        return;
    }

    entries_.reserve(monsters.size());

    for (const nova::entities::Monster* monster : monsters) {
        if (monster == nullptr) {
            continue;
        }

        const bool visible = monster->IsAlive() && monster->IsActive();
        if (!visible) {
            continue;
        }

        const nova::combat::Hitbox body = monster->GetHurtbox();
        entries_.push_back(MonsterHpBarEntry{
            body.x,
            body.y - 8.0F,
            body.w,
            monster->GetCurrentHp(),
            std::max(1, monster->GetMaxHp()),
            true
        });
    }
}

void MonsterHpBarSystem::Render(SDL_Renderer* renderer) const {
    if (!enabled_ || renderer == nullptr) {
        return;
    }

    for (const MonsterHpBarEntry& entry : entries_) {
        if (!entry.visible) {
            continue;
        }
        DrawBar(renderer, entry);
    }
}

void MonsterHpBarSystem::Clear() {
    entries_.clear();
}

void MonsterHpBarSystem::DrawBar(SDL_Renderer* renderer, const MonsterHpBarEntry& entry) {
    const int barX = static_cast<int>(entry.x);
    const int barY = static_cast<int>(entry.y);
    const int barW = std::max(1, static_cast<int>(entry.width));
    const int barH = 5;
    const float ratio = static_cast<float>(entry.currentHp) / static_cast<float>(entry.maxHp);
    const int fillW = std::max(0, static_cast<int>(static_cast<float>(barW) * std::clamp(ratio, 0.0F, 1.0F)));

    const SDL_Rect bgRect{barX, barY, barW, barH};
    const SDL_Rect fillRect{barX, barY, fillW, barH};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20U, 20U, 20U, 220U);
    SDL_RenderFillRect(renderer, &bgRect);

    SDL_SetRenderDrawColor(renderer, 80U, 220U, 80U, 240U);
    SDL_RenderFillRect(renderer, &fillRect);

    SDL_SetRenderDrawColor(renderer, 255U, 255U, 255U, 220U);
    SDL_RenderDrawRect(renderer, &bgRect);
}

} // namespace nova::ui
