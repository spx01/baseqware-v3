#pragma once

#include <cstdint>

#include "../../../offsets/client.dll.hpp"
#include "../driver_interface.hpp"
#include "common.hpp"
#include "vector.hpp"

namespace sdk {

SDK_DECLARE_CLASS(BaseEntity)
SDK_DECLARE_MEMBER(uintptr_t, m_pGameSceneNode, C_BaseEntity::m_pGameSceneNode);
SDK_DECLARE_MEMBER(int, m_iTeamNum, C_BaseEntity::m_iTeamNum);
SDK_DECLARE_MEMBER(int, m_lifeState, C_BaseEntity::m_lifeState);
};

SDK_DECLARE_CLASS(PlayerController)
SDK_DECLARE_MEMBER(uint32_t, m_hPlayerPawn, CCSPlayerController::m_hPlayerPawn);
SDK_DECLARE_MEMBER(
  uintptr_t, m_sSanitizedPlayerName, CCSPlayerController::m_sSanitizedPlayerName
);

std::string get_name(bool &rc) const;
BaseEntity get_pawn(driver_interface::ModuleInfo client, bool &rc) const;
}
;

SDK_DECLARE_CLASS(EntityListEntry)

PlayerController at(int idx, bool &rc) const;

// TODO:
static constexpr int k_max_idx = 64;
}
;

SDK_DECLARE_CLASS(EntityList)

EntityListEntry get_entry(bool &rc) const;
}
;

SDK_DECLARE_CLASS(GameSceneNode)
SDK_DECLARE_MEMBER(Vec3, m_vecAbsOrigin, CGameSceneNode::m_vecAbsOrigin);
}
;

} // namespace sdk
