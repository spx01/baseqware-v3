#pragma once

#include <array>
#include <atomic>
#include <new>

/// A triple buffer suitable for one producer and one consumer.
template <typename T>
class TBuffer {
public:
  TBuffer() noexcept
    : m_dirty(m_data.data()),
      m_ready(m_data.data() + 1),
      m_present(m_data.data() + 2) {}

  void write(T &&data) {
    *m_dirty = std::move(data);
    T *tmp = m_ready.exchange(m_dirty, std::memory_order_relaxed);
    m_dirty = tmp;

    m_stale_flag.store(false, std::memory_order_release);
  }

  // updated will be set to true if the data isn't stale.
  T read(bool &updated) const {
    bool b = m_stale_flag.load(std::memory_order_acquire);
    updated |= !b;
    if (b) { // NOLINT
      return *m_present;
    }

    T *tmp = m_ready.exchange(m_present, std::memory_order_relaxed);
    m_present.store(tmp, std::memory_order_relaxed);

    m_stale_flag.store(true, std::memory_order_relaxed);
    return *m_present;
  }

private:
  std::array<T, 3> m_data{};
  T *m_dirty;
  alignas(std::hardware_destructive_interference_size
  ) mutable std::atomic<T *> m_ready,
    m_present;
  mutable std::atomic<bool> m_stale_flag{false};
};
