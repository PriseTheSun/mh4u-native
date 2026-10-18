#pragma once
#include "player_actor.hpp"
#include <bit>
namespace mh4u::test_model_actor_fixture {
namespace na=native_actor;namespace jp=joint_pose;namespace bridge=player_position_bridge;
using U=std::uint32_t;constexpr U one=0x3f800000;
constexpr jp::Bits4 identity{0,0,0,one};
inline U bits(float value){return std::bit_cast<U>(value);}
inline na::PlayerModelContext model_context(){
 return {jp::Bits3{one,one,one},jp::Null{},mh4u::motion_transform_math::Basis{one,0,0,0,0,one,0,0,0,0,one,0},identity,
  bridge::OrientationCallback::b05328,bridge::OutputCallbacks::standard_95b490_95b940};
}
inline void model_inputs(na::PlayerInitial& seed){
 seed.position_fields={0x39,0x91,bits(4),0,one,0,4};
 seed.controller.player.fieldsd10={bits(1),bits(2),bits(3)};
 for(unsigned i=0;i<2;++i){auto& l=seed.layers[i].animation.layer;l.interpolation=l.remaining_interpolation=0;}
}
} // namespace mh4u::test_model_actor_fixture
