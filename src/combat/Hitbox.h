#pragma once

namespace nova {

// Axis-aligned bounding box (top-left origin, like an SDL_Rect).
// Used both for an entity's body and for a melee swing volume.
// Pure geometry only: no rendering, no SDL dependency.
struct Hitbox {
    float x = 0.0f; // left
    float y = 0.0f; // top
    float w = 0.0f;
    float h = 0.0f;

    bool intersects(const Hitbox& other) const {
        return x < other.x + other.w &&
               x + w > other.x &&
               y < other.y + other.h &&
               y + h > other.y;
    }

    // True circle test: does a circle (cx, cy, radius) overlap this box?
    // Works by finding the point on the box nearest the circle centre and
    // checking it is within the radius. Used for radial / AoE attacks.
    bool intersectsCircle(float cx, float cy, float radius) const {
        const float nearestX = cx < x ? x : (cx > x + w ? x + w : cx);
        const float nearestY = cy < y ? y : (cy > y + h ? y + h : cy);
        const float dx = cx - nearestX;
        const float dy = cy - nearestY;
        return (dx * dx + dy * dy) <= radius * radius;
    }

    float centerX() const { return x + w * 0.5f; }
    float centerY() const { return y + h * 0.5f; }
};

} // namespace nova
