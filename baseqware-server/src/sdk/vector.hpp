#pragma once

namespace sdk {
struct Vec2 {
  float x, y;
};

struct Vec3 {
  float x, y, z;
  Vec3 operator+(const Vec3 &rhs) const {
    return {x + rhs.x, y + rhs.y, z + rhs.z};
  }
};
}; // namespace sdk
