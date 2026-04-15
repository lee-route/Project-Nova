#include "combat/Hitbox.h"

namespace nova::combat {

bool Hitbox::Intersects(const Hitbox& other) const {
    const bool noOverlapX = (x + w <= other.x) || (other.x + other.w <= x);
    const bool noOverlapY = (y + h <= other.y) || (other.y + other.h <= y);
    return !(noOverlapX || noOverlapY);
}

} // namespace nova::combat
