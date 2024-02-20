#pragma once

#include "../driver_interface.hpp"

#define SDK_DECLARE_CLASS(name)                  \
  struct name {                                  \
    uintptr_t addr;                              \
    operator bool() const {                      \
      return this->addr != 0;                    \
    }                                            \
    operator uintptr_t() const {                 \
      return this->addr;                         \
    }                                            \
    name(uintptr_t addr) noexcept : addr(addr) { \
    }

#define SDK_DECLARE_MEMBER(type, name, offset)                  \
  type name(bool &rc) const {                                   \
    type value;                                                 \
    rc &= driver_interface::read(this->addr + (offset), value); \
    return value;                                               \
  }                                                             \
  void name##_into(type &value, bool &rc) const {               \
    rc &= driver_interface::read(this->addr + (offset), value); \
  }
