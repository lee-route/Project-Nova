#pragma once

namespace nova {

// Tiny 2D vector used for positions and directions in the top-down world.
// Kept header-only and dependency-free so any module can include it.
struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline float lengthSquared(const Vector2& v) {
    return v.x * v.x + v.y * v.y;
}

inline float distanceSquared(const Vector2& a, const Vector2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

} // namespace nova
