#pragma once

namespace nova::combat {

struct Hitbox {
    float x = 0.0F;
    float y = 0.0F;
    float w = 0.0F;
    float h = 0.0F;

    [[nodiscard]] bool Intersects(const Hitbox& other) const;
};

} // namespace nova::combat
