#pragma once
#include "player_manager_update.hpp"
namespace mh4u::test_manager_effects {
using U=std::uint32_t;namespace mu=mh4u::player_manager_update;
// PC and source argument interpretation only; no expected calculations.
inline std::optional<mu::Call> call(const std::array<U,6>& row,U foreign_player,U foreign_descriptor){using K=mu::CallKind;const bool foreign=row[2]==foreign_player;switch(row[0]){
 case 0xb085bc:return mu::Call{K::selector};case 0x68f548:return mu::Call{K::slot,row[3]};case 0x911e48:return mu::Call{K::enabled};case 0x568b54:return mu::Call{K::angle};
 case 0xd04924:return mu::Call{K::local_control,0,foreign};case 0xc04fc8:return mu::Call{K::manager_gate};case 0xb07e80:return mu::Call{K::flag6f9};case 0xd0473c:return mu::Call{K::type_group,row[3],foreign};case 0xd049d8:return mu::Call{K::action97};case 0xb0c47c:return mu::Call{K::action145,row[3]};case 0xb16060:return mu::Call{K::toggle,row[3]};case 0xc04130:return mu::Call{K::manager_start,row[3]};case 0x2f7ee4:return mu::Call{K::descriptor,row[4],row[3]==foreign_descriptor};case 0xb1012c:return mu::Call{K::descriptor_predicate,0,foreign};case 0xb449e8:return mu::Call{K::global_gate,0,foreign};case 0xb36e04:return mu::Call{K::vertical_update,0,foreign};case 0xb1a864:return mu::Call{K::vertical_heading,0,foreign};case 0xb3f868:return mu::Call{K::vertical_value,row[3],foreign};default:return {};}}
}
