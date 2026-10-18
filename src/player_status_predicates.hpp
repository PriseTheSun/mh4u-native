#pragma once
#include "player_transition.hpp"

namespace mh4u::player_status_predicates {
// Complete d05ac0. Missing PW224 is unknown, even when the later action test
// would be true: the original always consumes this signed halfword first.
[[nodiscard]] inline std::optional<bool> status224(
    std::optional<std::int16_t> field224,const player_transition::PbwFields& pbw) noexcept {
    if(!field224)return {};
    if(*field224>0)return true;
    if(pbw.field371!=6)return false;
    switch(pbw.field372){
    case 39:case 40:case 42:case 43:case 44:case 45:case 46:case 47:case 63:return true;
    default:return false;
    }
}
}
