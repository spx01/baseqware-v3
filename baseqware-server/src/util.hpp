#pragma once

// clang-format off
// #include "spdlog/spdlog.h"
#include <spdlog/fmt/bin_to_hex.h>
// clang-format on

namespace util {

template <typename T>
inline auto hexdump(const T &data) {
  return spdlog::to_hex(
    reinterpret_cast<const uint8_t *>(&data),
    reinterpret_cast<const uint8_t *>(&data + 1)
  );
}

} // namespace util
