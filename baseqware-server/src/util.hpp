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

class ScopeGuard {
public:
  template <typename F>
  ScopeGuard(F &&f) : m_f(std::forward<F>(f)) {} // NOLINT
  void defuse() {
    m_f = [] {};
  }
  ~ScopeGuard() { m_f(); }

private:
  std::function<void()> m_f;
};

class SyncTimer {
public:
  SyncTimer(std::chrono::milliseconds interval);

  void set_interval(std::chrono::milliseconds interval) {
    m_interval = interval;
  }

  /// Returns false if already late, otherwise waits and returns true.
  bool wait();

private:
  std::chrono::time_point<std::chrono::steady_clock> m_last;
  std::chrono::milliseconds m_interval;
};

} // namespace util
