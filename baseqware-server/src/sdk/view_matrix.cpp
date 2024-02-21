#include "view_matrix.hpp"

bool sdk::ViewMatrix::w2s(
  const Vec3 &world, std::pair<int, int> screen_dim, Vec2 &screen
) const {
  constexpr float k_epsilon = 1e-2F;
  float half_x = static_cast<float>(screen_dim.first) / 2.F;  // NOLINT
  float half_y = static_cast<float>(screen_dim.second) / 2.F; // NOLINT
  auto h = m[3][0] * world.x + m[3][1] * world.y + m[3][2] * world.z + m[3][3];
  if (h < k_epsilon) {
    return false;
  }
  screen.x = half_x
    + (m[0][0] * world.x + m[0][1] * world.y + m[0][2] * world.z + m[0][3])
      * half_x / h;
  screen.y = half_y
    - (m[1][0] * world.x + m[1][1] * world.y + m[1][2] * world.z + m[1][3])
      * half_y / h;
  return true;
}
