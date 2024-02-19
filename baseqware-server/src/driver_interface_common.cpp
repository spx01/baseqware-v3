#include "driver_interface.hpp"

bool driver_interface::ModuleInfo::read_raw(
  uintptr_t offset, void *dest, size_t size
) const {
#ifdef DEBUG
  if (offset + size > this->size) {
    spdlog::error("ModuleInfo::read_raw: Out of bounds");
    return false;
  }
#endif
  return driver_interface::read_raw(base_address + offset, dest, size);
}
