#pragma once

#include <cstdint>

#include "../../../offsets/client.dll.hpp"
#include "common.hpp"

namespace sdk {

SDK_DECLARE_CLASS(BaseEntity)
SDK_DECLARE_MEMBER(uintptr_t, m_pGameSceneNode, C_BaseEntity::m_pGameSceneNode)
SDK_DECLARE_MEMBER(int, m_iTeamNum, C_BaseEntity::m_iTeamNum);
SDK_DECLARE_MEMBER(int, m_lifeState, C_BaseEntity::m_lifeState);
};

SDK_DECLARE_CLASS(EntityList)
BaseEntity at(int idx, bool &rc) const {
  constexpr uintptr_t k_ptr_stride = 0x78;
  uintptr_t addr = 0;
  rc &= driver_interface::read(
    this->addr + (uintptr_t(idx) + 1) * k_ptr_stride, addr
  );
  return BaseEntity(addr);
};

// TODO:
static constexpr int k_max_idx = 64;
}
;

} // namespace sdk
