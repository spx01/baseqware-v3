#pragma once

#include <cstdint>
#include <utility>

#include <spdlog/spdlog.h>

namespace driver_interface {

bool initialize();
void finalize();
bool read_raw(uint64_t game_address, void *dest, size_t size);
bool is_game_running();

// Has to be updated along with the other definitions from the driver code.
enum ModuleRequest {
  PASED_CLIENT_MODULE,
  PASED_ENGINE_MODULE,
  PASED_MODULE_COUNT_,
};

template <typename T>
bool read(uint64_t address, T &value) {
  return read_raw(address, &value, sizeof(T));
}

struct ModuleInfo {
  uintptr_t base_address = 0;
  size_t size = 0;

  template <typename T>
  bool read(uintptr_t offset, T &value) const {
#ifdef DEBUG
    if (offset + sizeof(T) > size) {
      spdlog::error("ModuleInfo::read: Out of bounds");
      return false;
    }
#endif
    return driver_interface::read(base_address + offset, value);
  }

  bool read_raw(uintptr_t offset, void *dest, size_t size) const;
};

// First bool is true if the module was found.
std::pair<bool, ModuleInfo> get_module_info(ModuleRequest module);
} // namespace driver_interface
