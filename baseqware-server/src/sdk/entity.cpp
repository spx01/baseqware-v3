#include "entity.hpp"

namespace sdk {

std::string PlayerController::get_name(bool &rc) const {
  constexpr int k_max_name_len = 32 * 4;
  char buf[k_max_name_len + 1]{};
  auto addr = this->m_sSanitizedPlayerName(rc);

  if (!addr) {
    rc = false;
    return std::string();
  }
  rc &= driver_interface::read_raw(addr, buf, k_max_name_len);
  return std::string(buf);
}

PlayerController EntityListEntry::at(int idx, bool &rc) const {
  constexpr uintptr_t k_ptr_stride = 0x78;
  uintptr_t addr = 0;
  rc &= driver_interface::read(
    this->addr + (uintptr_t(idx) + 1) * k_ptr_stride, addr
  );
  return PlayerController(addr);
}

EntityListEntry EntityList::get_entry(bool &rc) const {
  uintptr_t entry_addr = 0;
  constexpr uintptr_t k_entry_offset = 0x10;
  rc &= driver_interface::read(this->addr + k_entry_offset, entry_addr);
  return EntityListEntry(entry_addr);
}

} // namespace sdk
