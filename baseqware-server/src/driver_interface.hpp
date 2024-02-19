#pragma once

#include <cstdint>
#include <utility>

namespace driver_interface {

bool initialize();
void finalize();
bool read_raw(uint64_t game_address, void *dest, size_t size);
bool is_game_running();

// Has to be updated along with the other definitions from the driver code.
enum ModuleRequest {
  PASED_CLIENT_MODULE,
  PASED_MODULE_COUNT_,
};

struct ModuleInfo {
  uintptr_t base_address = 0;
  size_t size = 0;
};

// First bool is true if the module was found.
std::pair<bool, ModuleInfo> get_module_info(ModuleRequest module);

template <typename T>
bool read(uint64_t address, T &value) {
  return read_raw(address, &value, sizeof(T));
}

} // namespace driver_interface
