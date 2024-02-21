#pragma once

#include "common.hpp"
#include "vector.hpp"

namespace sdk {
struct ViewMatrix {
  float m[4][4];

  bool
  w2s(const Vec3 &world, std::pair<int, int> screen_dim, Vec2 &screen) const;
};
} // namespace sdk
