#include "monster/Monster.h"

namespace nova {

Monster::Monster(int id, const Vector2& position, const AttackData& attack)
    : id_(id), position_(position), attackController_(attack, id) {}

Hitbox Monster::bodyHitbox() const {
    Hitbox box;
    box.w = bodyWidth_;
    box.h = bodyHeight_;
    box.x = position_.x - box.w * 0.5f;
    box.y = position_.y - box.h * 0.5f;
    return box;
}

bool Monster::tryAttack(const Vector2& targetCenter) {
    // Face the target so the melee hitbox is spawned toward it.
    facing_ = Vector2{ targetCenter.x - position_.x,
                       targetCenter.y - position_.y };

    return attackController_.tryStartAttack(position_, facing_, targetCenter);
}

} // namespace nova
