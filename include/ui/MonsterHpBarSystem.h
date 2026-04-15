#pragma once

#include <vector>

#include <SDL.h>

namespace nova::entities {
class Monster;
}

namespace nova::ui {

struct MonsterHpBarEntry {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    int currentHp = 0;
    int maxHp = 1;
    bool visible = false;
};

class MonsterHpBarSystem {
public:
    void SetEnabled(bool enabled);
    [[nodiscard]] bool IsEnabled() const;

    void BuildFrameData(const std::vector<const nova::entities::Monster*>& monsters);
    void Render(SDL_Renderer* renderer) const;
    void Clear();

private:
    static void DrawBar(SDL_Renderer* renderer, const MonsterHpBarEntry& entry);

    bool enabled_ = true;
    std::vector<MonsterHpBarEntry> entries_{};
};

} // namespace nova::ui
