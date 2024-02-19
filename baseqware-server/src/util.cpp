#include "util.hpp"

#include <thread>

using namespace std::chrono_literals;

namespace util {

SyncTimer::SyncTimer(std::chrono::milliseconds interval)
  : m_last(std::chrono::steady_clock::now()), m_interval(interval) {
}

bool SyncTimer::wait() {
  auto now = std::chrono::steady_clock::now();
  auto diff = now - m_last;
  m_last = now;
  if (diff < m_interval) {
    std::this_thread::sleep_for(m_interval - diff);
    return true;
  }
  return false;
}

} // namespace util
